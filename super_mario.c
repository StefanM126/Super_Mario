
///////////////////////////////////////////////////////////////////////////////
// Headers.

#include <stdint.h>
#include "system.h"
#include <stdio.h>

#include "mario_sprites_idx4.h"


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

#define STEP 1
#define JUMP_STEP -2

#define MARIO_ANIM_DELAY 6
#define MARIO_ANIM_JUMP_DELAY 24


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
			uint32_t dst_idx =
				(dst_y+y)*SCREEN_IDX4_W8 +
				(dst_x8+x8);
			uint32_t background_pixels = pack_idx4_p32[dst_idx];
			uint32_t pixels_out = 0;

			for (int i = 0; i < 8; i++){

				if(((pixels>>(i*4))&0xf )== 1){
					pixels_out |= background_pixels&(0xf<<(i*4));

				}else{
					pixels_out |= pixels&(0xf<<(i*4));
				}

			}
			pack_idx4_p32[dst_idx] = pixels_out;
		}
	}
}




//
// void draw_sprite_from_atlas(
// 	uint16_t src_x,
// 	uint16_t src_y,
// 	uint16_t w,
// 	uint16_t h,
// 	uint16_t dst_x,
// 	uint16_t dst_y
// ) {
//
//
// 	for(uint16_t y = 0; y < h; y++){
// 		for(uint16_t x = 0; x < w; x++){
// 			uint32_t src_idx =
// 				(src_y+y)*maiomario__w +
// 				(src_x+x);
// 			uint32_t dst_idx =
// 				(((dst_y+y)%SCREEN_RGB333_H)*SCREEN_RGB333_W) +
// 				(dst_x+x)%SCREEN_RGB333_W;
// 			uint16_t pixel = maiomario__p[src_idx];
// 			unpack_rgb333_p32[dst_idx] = (pixel != 0777)?pixel:unpack_rgb333_p32[dst_idx];
// 		}
// 	}
//
//
// }

///////////////////////////////////////////////////////////////////////////////
// Game code.

int main(void) {

	// Setup.
	gpu_p32[0] = 2; // IDX4 mode.
	gpu_p32[1] = 1; // Packed mode.
	gpu_p32[0x800] = 0x00ff00ff; // Magenta for HUD.

	// Copy palette.
	for(uint8_t i = 0; i < 16; i++){
		palette_p32[i] = palette[i];
	}

	// Game state.
	game_state_t gs;
	gs.mario.pos.x = 0;
	gs.mario.pos.y = SCREEN_IDX4_H/4 *3;
	gs.mario.anim.state = MARIO_IDLE;
	gs.mario.anim.delay_cnt = 0;

	typedef enum{RIGHT, LEFT} mario_direction;
	typedef enum{WALK, JUMP, FALL, SQUAT} mario_state;

	mario_direction direction = RIGHT;
	mario_state state = WALK;
	int mov_x = 0;
	int mov_y = 0;

	while(1){

		/////////////////////////////////////
		// Poll controls.

		mov_x = 0;
		mov_y = 0;
		state = WALK;

        if(joypad.up &&
		   gs.mario.anim.state != MARIO_JUMP &&
           gs.mario.anim.state != MARIO_FALL &&
           gs.mario.anim.state != MARIO_SQUAT
        ){
			mov_y = JUMP_STEP;
			state = JUMP;

        }else if(joypad.right){
			mov_x = +STEP;
			direction = RIGHT;
            if(gs.mario.anim.state != MARIO_JUMP &&
               gs.mario.anim.state != MARIO_FALL
            ){
                state = WALK;
            }

		}else if(joypad.left){
			mov_x = -STEP;
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


		/////////////////////////////////////
		// Gameplay.


		if((gs.mario.pos.x += mov_x*STEP) > SCREEN_IDX4_W){
			if(mov_x < 0){
				gs.mario.pos.x = SCREEN_IDX4_W;
			}else{
				gs.mario.pos.x = 0;
			}
		}

// -----------------------------------------------------
		//if(state == JUMP ){
		//	printf("%d uso\n", brojac++ );
		//	if((gs.mario.pos.y + mov_y*STEP) > SCREEN_IDX4_H){
		//		if(mov_y < 0){
		//			gs.mario.pos.y = SCREEN_IDX4_H;
		//		}else{
		//			gs.mario.pos.y = 0;

		//		}
		//	}
		//}
// -----------------------------------------------------


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
						if(mov_x != 0 ){
							gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
							gs.mario.anim.state = MARIO_STEP_FOURTH;
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

			case MARIO_STEP_FOURTH:
					if(gs.mario.anim.delay_cnt != 0){
							gs.mario.anim.delay_cnt--;
					}else{
						if(mov_x != 0 ){
							gs.mario.anim.delay_cnt = MARIO_ANIM_DELAY;
							gs.mario.anim.state = MARIO_STEP_FIRST;
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

			case MARIO_JUMP:
					if(gs.mario.anim.delay_cnt != 0){
						gs.mario.anim.delay_cnt--;
						mov_y = JUMP_STEP;
						gs.mario.pos.y += mov_y*STEP;
					}else{
						gs.mario.anim.delay_cnt = MARIO_ANIM_JUMP_DELAY;
						gs.mario.anim.state = MARIO_FALL;
					}
					break;

			case MARIO_FALL:
					if(gs.mario.anim.delay_cnt != 0){
							gs.mario.anim.delay_cnt--;
							mov_y = -JUMP_STEP;
							gs.mario.pos.y += mov_y*STEP;
					}else{
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



		/////////////////////////////////////
		// Drawing.


		// Detecting rising edge of VSync.
		WAIT_UNITL_0(gpu_p32[2]);
		WAIT_UNITL_1(gpu_p32[2]);
		// Draw in buffer while it is in VSync.




		// Black background.
		for(uint16_t r1 = 0; r1 < SCREEN_IDX4_H; r1++){
			for(uint16_t c8 = 0; c8 < SCREEN_IDX4_W8; c8++){
				pack_idx4_p32[r1*SCREEN_IDX4_W8 + c8] = 0xeeeeeeee;
			}
		}



		// Draw mario.
		// draw_sprite_from_atlas(
		// 	16, 16*11, 16, 15, SCREEN_RGB333_W/2, SCREEN_RGB333_H/2
		// 	);
        switch(gs.mario.anim.state){
            case MARIO_IDLE:
                draw_sprite(
                    mario[direction][0], 16, 32, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_STEP_FIRST:
                draw_sprite(
                    mario[direction][1], 16, 32, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_STEP_SECOND:
                draw_sprite(
                    mario[direction][2], 16, 32, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_STEP_THIRD:
                draw_sprite(
                    mario[direction][1], 16, 32, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_STEP_FOURTH:
                draw_sprite(
                    mario[direction][0], 16, 32, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_JUMP:
                draw_sprite(
                    mario[direction][3], 16, 32, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_FALL:
                draw_sprite(
                    mario[direction][3], 16, 32, gs.mario.pos.x, gs.mario.pos.y
                );
                break;

            case MARIO_SQUAT:
                draw_sprite(
                    mario[direction][4], 16, 32, gs.mario.pos.x, gs.mario.pos.y
                );
                break;
            }
        }

        return 0;
}

///////////////////////////////////////////////////////////////////////////////
