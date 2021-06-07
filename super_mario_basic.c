
///////////////////////////////////////////////////////////////////////////////
// Headers.

#include <stdint.h>
#include "system.h"
#include <stdio.h>
#include <math.h>


#include "mario_sprites_basic_green_idx4.h"
#include "map.h"


///////////////////////////////////////////////////////////////////////////////
// HW stuff.

#define WAIT_UNITL_0(x) while(x != 0){}
#define WAIT_UNITL_1(x) while(x != 1){}

#define SCREEN_IDX1_W 640
#define SCREEN_IDX1_H 480

#define SCREEN_IDX4_W 320
#define SCREEN_IDX4_H 240
#define SCREEN_RGB333_W 160
#define SCREEN_RGB333_H 120

#define SCREEN_IDX4_W8 (SCREEN_IDX4_W/8)

#define gpu_p32 ((volatile uint32_t*)LPRS2_GPU_BASE)
#define palette_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x1000))
#define unpack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x400000))
#define pack_idx1_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x600000))
#define unpack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0x800000))
#define pack_idx4_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xa00000))
#define unpack_rgb333_p32 ((volatile uint32_t*)(LPRS2_GPU_BASE+0xc00000))
#define joypad_p32 ((volatile uint32_t*)LPRS2_JOYPAD_BASE)

typedef struct {
	unsigned a      : 1;
	unsigned b      : 1;
	unsigned z      : 1;
	unsigned start  : 1;
	unsigned up     : 1;
	unsigned down   : 1;
	unsigned left   : 1;
	unsigned right  : 1;
} bf_joypad;
#define joypad (*((volatile bf_joypad*)LPRS2_JOYPAD_BASE))

typedef struct {
	uint32_t m[SCREEN_IDX1_H][SCREEN_IDX1_W];
} bf_unpack_idx1;
#define unpack_idx1 (*((volatile bf_unpack_idx1*)unpack_idx1_p32))



///////////////////////////////////////////////////////////////////////////////
// Game config.
#define BLOCK_WIDTH  16
#define BLOCK_HEIGHT 16
#define MARIO_HEIGHT 32
#define MARIO_WIDTH  16
#define MAP_WIDTH    20
#define MAP_HEIGHT   15
#define FRAME_WIDTH  20
#define FRAME_HEIGHT 15
#define MOVE_BORDER  9 * 16

#define GRASS_POSITION       SCREEN_IDX4_H - 2 *BLOCK_HEIGHT
#define MARIO_START_POSITION GRASS_POSITION - MARIO_HEIGHT


#define STEP 1
#define JUMP_STEP -2

#define MARIO_ANIM_DELAY 6
#define MARIO_ANIM_JUMP_DELAY 36


uint32_t* mario[2][5] = {
    {
		mario_01_right_idle__p,
		mario_02_right_first_step__p,
		mario_03_right_second_step__p,
		mario_04_right_jump__p,
		mario_05_right_squat__p
	},
	{
		mario_01_left_idle__p,
	 	mario_02_left_first_step__p,
		mario_03_left_second_step__p,
		mario_04_left_jump__p,
		mario_05_left_squat__p
	}

};
///////////////////////////////////////////////////////////////////////////////
// Game data structures.



typedef struct {
	uint16_t x;
	uint16_t y;
} point_t;



typedef enum {
	MARIO_IDLE,
	MARIO_STEP_FIRST,
	MARIO_STEP_SECOND,
	MARIO_STEP_THIRD,
	MARIO_STEP_FOURTH,
	MARIO_JUMP,
	MARIO_FALL,
	MARIO_SQUAT


} mario_anim_states_t;



typedef struct {
	mario_anim_states_t state;
	uint8_t delay_cnt;
} mario_anim_t;



typedef struct {
	point_t pos;
	mario_anim_t anim;
} mario_t;



typedef struct {
	mario_t mario;
} game_state_t;


static int magenta_idx;
void find_magenta_idx(){
	for(uint8_t i = 0; i < 16; i++){

		if(palette_p32[i] == 0x00ff00ff){
			magenta_idx = i;
			return;
		}

	}
	return;

}
static inline uint32_t shift_div_with_round_down(uint32_t num, uint32_t shift){
	uint32_t d = num >> shift;
	return d;
}



static inline uint32_t shift_div_with_round_up(uint32_t num, uint32_t shift){
	uint32_t d = num >> shift;
	uint32_t mask = (1<<shift)-1;
	if((num & mask) != 0){
		d++;
	}
	return d;
}

static void draw_sprite_unpacked(
	uint32_t* src_p,
	uint16_t src_w,
	uint16_t src_h,
	uint16_t dst_x,
	uint16_t dst_y
){


	uint16_t src_w8 = shift_div_with_round_up(src_w, 3);
	uint8_t pixel;

	for(int r = 0; r < src_h; r++){
		for(int c = 0; c < src_w; c++){
			uint16_t src_idx = r * src_w8 + c/8;
			uint32_t pixels = src_p[src_idx];
				pixel = (pixels >> (c%8)*4)&0xf;
				if(src_p == mario_04_left_jump__p || src_p == mario_04_right_jump__p){

					if((r + dst_y)*SCREEN_IDX4_W + (c+dst_x)  > SCREEN_IDX4_W*SCREEN_IDX4_H){
						continue;
					}
				}
				if(pixel != magenta_idx)
					unpack_idx4_p32[(r + dst_y)*SCREEN_IDX4_W + (c+dst_x)] = pixel;

		}
	}
}

static void draw_sprite(
	uint32_t* src_p,
	uint16_t src_w,
	uint16_t src_h,
	uint16_t dst_x,
	uint16_t dst_y
) {
	uint16_t dst_x8 = shift_div_with_round_down(dst_x, 3);
	uint16_t src_w8 = shift_div_with_round_up(src_w, 3);



	for(uint16_t y = 0; y < src_h; y++){
		for(uint16_t x8 = 0; x8 < src_w8; x8++){
			uint32_t src_idx = y*src_w8 + x8;
			uint32_t pixels = src_p[src_idx];
			uint32_t dst_idx;

			dst_idx = (dst_y+y)*SCREEN_IDX4_W8 +(dst_x8+x8);
			uint32_t background_pixels = pack_idx4_p32[dst_idx]; uint32_t pixels_out = 0;
			for (int i = 0; i < 8; i++){

				if(((pixels>>(i*4))&0xf )== magenta_idx){
					pixels_out |= background_pixels&(0xf<<(i*4));

				}else{
					pixels_out |= pixels&(0xf<<(i*4));
				}

			}
			pack_idx4_p32[dst_idx] = pixels_out;
		}
	}
}

static uint8_t* map[] = {frame0, frame1, frame2, frame3, frame4};

uint8_t readMap(uint16_t x, uint16_t y) {
    return map[x/FRAME_WIDTH][y*FRAME_WIDTH + x % FRAME_WIDTH];
}

void writeMap(uint16_t x, uint16_t y, uint8_t value) {
    map[x/FRAME_WIDTH][y*FRAME_WIDTH + x % FRAME_WIDTH] = value;
}

enum Blocks {NONE, GRASS, GROUND, POINT,
			BRICK, EMPTY_POINT, BROKEN_BRICK,
			PIPE_DOWN, PIPE_UP};
void draw_map(uint32_t offset) {
    uint16_t blockOffset = offset /= BLOCK_WIDTH;

	for(int j = 0; j < FRAME_HEIGHT; j++){
		for(int i = 0; i < FRAME_WIDTH; i++){
		    switch (map[(blockOffset + i)/FRAME_WIDTH][j*FRAME_WIDTH + (blockOffset + i) % FRAME_WIDTH]) {

                case GRASS:
						draw_sprite_unpacked(grass_01__p, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT);
						break;

				case GROUND:
						draw_sprite_unpacked(grass_02__p, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT);
						break;

				case POINT:
						draw_sprite_unpacked(point_01__p, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT);
						break;

				case BRICK:
						draw_sprite_unpacked(brick_01__p, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT);
						break;

				case EMPTY_POINT:
						draw_sprite_unpacked(point_02__p, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT);
						break;

				case BROKEN_BRICK:
						draw_sprite_unpacked(brick_02__p, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT);
						writeMap(offset + i, j, NONE);
						break;

				case PIPE_DOWN:
						draw_sprite_unpacked(pipe_02__p, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT);
						break;

				case PIPE_UP:
						draw_sprite_unpacked(pipe_01__p, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT);
						break;
			}



		}
	}

	// draw_sprite(odakle, BLOCK_WIDTH, BLOCK_HEIGHT, i*BLOCK_WIDTH, j*BLOCK_HEIGHT)
}



///////////////////////////////////////////////////////////////////////////////
// Game code.

int main(int argc, char* argv[]) {


	// Setup.
	gpu_p32[0] = 2; // IDX4 mode.
	gpu_p32[1] = 0; // Unpacked mode.
	gpu_p32[0x800] = 0x00ff00ff; // Magenta for HUD.

	// Copy palette.
	for(uint8_t i = 0; i < 16; i++) {
		palette_p32[i] = palette[i];
	}

	find_magenta_idx();



	// Game state.
	game_state_t gs;
	gs.mario.pos.x = 0;
	gs.mario.pos.y = MARIO_START_POSITION;
	gs.mario.anim.state = MARIO_IDLE;
	gs.mario.anim.delay_cnt = 0;

	typedef enum{RIGHT, LEFT} mario_direction;
	typedef enum{WALK, JUMP, FALL, SQUAT} mario_state;

	mario_direction direction = RIGHT;
	mario_state state = WALK;
	int mov_x = 0;
	int mov_y = 0;
	uint16_t map_position_x, map_position_x_left, map_position_x_right;
	uint16_t map_position_y, map_position_y_down;
    uint16_t offsetCnt = 0;
    uint16_t offset = 0;

	while(1){

		/////////////////////////////////////
		// Poll controls.
		mov_x = 0;
		mov_y = 0;
		state = WALK;
{

        if(joypad.up &&

		   gs.mario.anim.state != MARIO_JUMP &&
           gs.mario.anim.state != MARIO_FALL &&
           gs.mario.anim.state != MARIO_SQUAT
        ){

				mov_y = JUMP_STEP;
				state = JUMP;


        }else if(joypad.right){
			mov_x = +1;
			direction = RIGHT;


            if(gs.mario.anim.state != MARIO_JUMP &&
               gs.mario.anim.state != MARIO_FALL
            ){
                state = WALK;
            }

		}else if(joypad.left){
			mov_x = -1;
			direction = LEFT;
            if(gs.mario.anim.state != MARIO_JUMP &&
               gs.mario.anim.state != MARIO_FALL
            ){
                state = WALK;
            }

		}else if(joypad.down &&
			     gs.mario.anim.state != MARIO_JUMP &&
                 gs.mario.anim.state != MARIO_FALL
        ){
			mov_y = 0;
			state = SQUAT;

		}
}


		/////////////////////////////////////
		// Gameplay.

        if(offsetCnt < MOVE_BORDER)
            gs.mario.pos.x += mov_x*STEP;
			if((int16_t)gs.mario.pos.x<0)
				gs.mario.pos.x = 0;

		offsetCnt += mov_x*STEP;
		if((int16_t)offsetCnt<0)
			offsetCnt = 0;
		else if(offsetCnt == SCREEN_IDX4_W * (sizeof(map)/sizeof(uint8_t*)) - SCREEN_IDX4_W/2)
			offsetCnt -= mov_x*STEP;


		if(gs.mario.pos.x > SCREEN_IDX4_W){
			if(mov_x < 0){
				gs.mario.pos.x = SCREEN_IDX4_W;
			}else{
				gs.mario.pos.x = 0;
			}
		}

        offset = offsetCnt > MOVE_BORDER ? offsetCnt - MOVE_BORDER : 0;

		map_position_y = gs.mario.pos.y / BLOCK_HEIGHT;
		map_position_y_down = (gs.mario.pos.y + MARIO_HEIGHT - 1)/ BLOCK_HEIGHT;
		map_position_x_left = (offset + gs.mario.pos.x - 1) / BLOCK_WIDTH;
		map_position_x_right = (offset + gs.mario.pos.x + MARIO_WIDTH - 1) / BLOCK_WIDTH;

		if(direction == LEFT){

			if(readMap(map_position_x_left, map_position_y ) != 0 //glava
				|| readMap(map_position_x_left, map_position_y + 1 ) != 0 //noge
				|| readMap(map_position_x_left, map_position_y_down ) != 0 ){ // ispod nogu ako predje
                if(offsetCnt < MOVE_BORDER)
				    gs.mario.pos.x -= mov_x*STEP;
                offsetCnt -= mov_x*STEP;
			}
			else if(gs.mario.anim.state == MARIO_FALL){
				if(readMap(map_position_x_left, map_position_y + 2 ) != 0){//ispod nogu
                    if(offsetCnt < MOVE_BORDER)
                        gs.mario.pos.x -= mov_x*STEP;
                    offsetCnt -= mov_x*STEP;
				}
			}else if(gs.mario.anim.state == MARIO_JUMP){
				if(readMap(map_position_x_left, map_position_y -1 ) != 0){//iznad glave
					if(offsetCnt < MOVE_BORDER)
                        gs.mario.pos.x -= mov_x*STEP;
                    offsetCnt -= mov_x*STEP;
				}
			}
		}else if(direction == RIGHT){

			if(readMap(map_position_x_right, map_position_y ) != 0
			|| readMap(map_position_x_right , map_position_y + 1 ) != 0){
			        if(offsetCnt < MOVE_BORDER)
                        gs.mario.pos.x -= mov_x*STEP;
                    offsetCnt -= mov_x*STEP;

			}
			else if(gs.mario.anim.state == MARIO_FALL){
				if(readMap(map_position_x_right, map_position_y + 2 ) != 0){
					if(offsetCnt < MOVE_BORDER)
                        gs.mario.pos.x -= mov_x*STEP;
                    offsetCnt -= mov_x*STEP;
				}
			}else if(gs.mario.anim.state == MARIO_JUMP){
				if(readMap(map_position_x_left, map_position_y -1 ) != 0
				|| readMap(map_position_x_right, map_position_y_down ) != 0 ){
					if(offsetCnt < MOVE_BORDER)
                        gs.mario.pos.x -= mov_x*STEP;
                    offsetCnt -= mov_x*STEP;
				}
			}
		}


// -----------------------------------------------------

{


		switch(gs.mario.anim.state){
			case MARIO_IDLE:
				if(mov_x != 0){
					gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
					gs.mario.anim.state = MARIO_STEP_FIRST;
				}else if(mov_y < 0){
					gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
					gs.mario.anim.state = MARIO_JUMP;
				}else if(state == SQUAT){
					gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
					gs.mario.anim.state = MARIO_SQUAT;
				}
				break;

			case MARIO_STEP_FIRST:
				if(gs.mario.anim.delay_cnt != 0){
						gs.mario.anim.delay_cnt--;
				}else{
					map_position_y = gs.mario.pos.y / BLOCK_HEIGHT + 2;
					map_position_x_left = (offset + gs.mario.pos.x) / BLOCK_WIDTH;
					map_position_x_right = (offset + gs.mario.pos.x + BLOCK_WIDTH - 1) / BLOCK_WIDTH;
					if(direction == RIGHT){
						if(readMap(map_position_x_left, map_position_y) == 0){
							gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
							gs.mario.anim.state = MARIO_FALL;
							break;
						}

					}else if(direction == LEFT){
						if(readMap(map_position_x_right, map_position_y) == 0){
							gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
							gs.mario.anim.state = MARIO_FALL;
							break;
						}

					}

					if(mov_x != 0 ){
						gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
						gs.mario.anim.state = MARIO_STEP_SECOND;
					}else if(mov_y < 0){
						gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
						gs.mario.anim.state = MARIO_JUMP;
					}else if(state == SQUAT){
						gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
						gs.mario.anim.state = MARIO_SQUAT;
					}else{
						gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
						gs.mario.anim.state = MARIO_IDLE;
					}
				}
				break;

			case MARIO_STEP_SECOND:
				if(gs.mario.anim.delay_cnt != 0){
						gs.mario.anim.delay_cnt--;
				}else{

					map_position_y = gs.mario.pos.y / BLOCK_HEIGHT + 2;
					map_position_x_left = (offset + gs.mario.pos.x) / BLOCK_WIDTH;
					map_position_x_right = (offset + gs.mario.pos.x + BLOCK_WIDTH - 1) / BLOCK_WIDTH;

					if(direction == RIGHT){
						if(readMap(map_position_x_left, map_position_y) == 0){
							gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
							gs.mario.anim.state = MARIO_FALL;
							break;
						}

					}else if(direction == LEFT){
						if(readMap(map_position_x_right, map_position_y) == 0){
							gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
							gs.mario.anim.state = MARIO_FALL;
							break;
						}

					}
					if(mov_x != 0 ){
						gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
						gs.mario.anim.state = MARIO_STEP_THIRD;
					}else if(mov_y < 0){
						gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
						gs.mario.anim.state = MARIO_JUMP;
					}else if(state == SQUAT){
						gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
						gs.mario.anim.state = MARIO_SQUAT;
					}else{
						gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
						gs.mario.anim.state = MARIO_IDLE;
					}
				}
				break;

			case MARIO_STEP_THIRD:
					if(gs.mario.anim.delay_cnt != 0){
							gs.mario.anim.delay_cnt--;
					}else{
						map_position_y = gs.mario.pos.y / BLOCK_HEIGHT + 2;
						map_position_x_left = (offset + gs.mario.pos.x) / BLOCK_WIDTH;
						map_position_x_right = (offset + gs.mario.pos.x + BLOCK_WIDTH - 1) / BLOCK_WIDTH;
						if(direction == RIGHT){
							if(readMap(map_position_x_left, map_position_y) == 0){
								gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
								gs.mario.anim.state = MARIO_FALL;
								break;
							}

						}else if(direction == LEFT){
							if(readMap(map_position_x_right, map_position_y) == 0){
								gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
								gs.mario.anim.state = MARIO_FALL;
								break;
							}

						}
						if(mov_x != 0 ){
							gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
							gs.mario.anim.state = MARIO_STEP_FIRST;
						}else if(mov_y < 0){
							gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
							gs.mario.anim.state = MARIO_JUMP;
						}else if(state == SQUAT){
							gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
							gs.mario.anim.state = MARIO_SQUAT;
						}
						else{
							gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
							gs.mario.anim.state = MARIO_IDLE;
						}
					}
					break;



			case MARIO_JUMP:

					if(gs.mario.anim.delay_cnt != 0){
						gs.mario.anim.delay_cnt--;
						map_position_y = gs.mario.pos.y / BLOCK_HEIGHT;
						map_position_x_left = (offset + gs.mario.pos.x) / BLOCK_WIDTH;
						map_position_x_right = (offset + gs.mario.pos.x + BLOCK_WIDTH -1) / BLOCK_WIDTH;

						if(readMap(map_position_x_left, map_position_y) != 0
						|| readMap(map_position_x_right, map_position_y) != 0){

							if(readMap(map_position_x_left, map_position_y) == POINT){
								writeMap(map_position_x_left, map_position_y, EMPTY_POINT);
							}else if(readMap(map_position_x_left, map_position_y) == BRICK){
								writeMap(map_position_x_left, map_position_y, BROKEN_BRICK);
							}

							if(readMap(map_position_x_right, map_position_y) == POINT){
								writeMap(map_position_x_right, map_position_y, EMPTY_POINT);
							}else if(readMap(map_position_x_right, map_position_y) == BRICK){
								writeMap(map_position_x_right, map_position_y, BROKEN_BRICK);
							}

							gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
							gs.mario.anim.state = MARIO_FALL;
							break;
						}
						mov_y = JUMP_STEP;
						gs.mario.pos.y += mov_y*STEP;

					}else{
						gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
						gs.mario.anim.state = MARIO_FALL;
					}
					break;

			case MARIO_FALL:

					map_position_y = gs.mario.pos.y / BLOCK_HEIGHT + 2;
					map_position_x_left = (offset + gs.mario.pos.x) / BLOCK_WIDTH;
					map_position_x_right = (offset + gs.mario.pos.x + BLOCK_WIDTH - 1) / BLOCK_WIDTH;
					if(gs.mario.anim.delay_cnt != 0){
							gs.mario.anim.delay_cnt--;
							if(readMap(map_position_x_left, map_position_y) != 0
							|| readMap(map_position_x_right, map_position_y) != 0){
								gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
								gs.mario.anim.state = MARIO_IDLE;
								break;
							}
							mov_y = -JUMP_STEP;
							gs.mario.pos.y += mov_y*STEP;
					}else{
						if(readMap(map_position_x_left, map_position_y) == 0
						|| readMap(map_position_x_right, map_position_y) == 0){
							gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
							gs.mario.anim.state = MARIO_FALL;
							break;
						}
							gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
							gs.mario.anim.state = MARIO_IDLE;
					}
					break;

			case MARIO_SQUAT:
					if(gs.mario.anim.delay_cnt != 0){
						gs.mario.anim.delay_cnt--;
					}else{
					    gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
						if(state == SQUAT){
							gs.mario.anim.state = MARIO_SQUAT;
						}else{
							gs.mario.anim.state = MARIO_IDLE;
						}
					}
					break;
        }

}

		/////////////////////////////////////
		// Drawing.


		// Detecting rising edge of VSync.
		WAIT_UNITL_0(gpu_p32[2]);
		WAIT_UNITL_1(gpu_p32[2]);
		// Draw in buffer while it is in VSync.




		draw_sprite_unpacked(
			sky__p, SCREEN_IDX4_W, SCREEN_IDX4_H, 0, 0
		);

		//static uint32_t offset = 0;

		draw_map(offset);



		// Draw mario.


        switch(gs.mario.anim.state){
            case MARIO_IDLE:
                draw_sprite_unpacked(
                    mario[direction][0], MARIO_WIDTH, MARIO_HEIGHT, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_STEP_FIRST:
                draw_sprite_unpacked(
                    mario[direction][2], MARIO_WIDTH, MARIO_HEIGHT, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_STEP_SECOND:
                draw_sprite_unpacked(
                    mario[direction][1], MARIO_WIDTH, MARIO_HEIGHT, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_STEP_THIRD:
                draw_sprite_unpacked(
                    mario[direction][0], MARIO_WIDTH, MARIO_HEIGHT, gs.mario.pos.x, gs.mario.pos.y
                );
                break;


            case MARIO_JUMP:
                draw_sprite_unpacked(
                    mario[direction][3], MARIO_WIDTH, MARIO_HEIGHT, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_FALL:
                draw_sprite_unpacked(
                    mario[direction][3], MARIO_WIDTH, MARIO_HEIGHT, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_SQUAT:
                draw_sprite_unpacked(
                    mario[direction][4], MARIO_WIDTH, MARIO_HEIGHT, gs.mario.pos.x, gs.mario.pos.y
                );
                break;
            }
        }

        return 0;
}

///////////////////////////////////////////////////////////////////////////////
