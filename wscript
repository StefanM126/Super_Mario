#! /usr/bin/env python
# encoding: utf-8

'''
@author: Milos Subotic <milos.subotic.sm@gmail.com>
@license: MIT

'''

###############################################################################

import os
import sys
import fnmatch
import shutil
import datetime
import glob

import waflib

###############################################################################

APPNAME = 'LPRS2_GPU_Emulator_Super_Mario'

top = '.'

###############################################################################

def prerequisites(ctx):
	ctx.recurse('emulator')

	if sys.platform.startswith('linux'):
		# Ubuntu.
		ctx.exec_command2('apt-get -y install python-pil')
	elif sys.platform == 'win32' and os.name == 'nt' and os.path.sep == '/':
		# MSYS2 Windows /mingw32/bin/python.
		ctx.exec_command2(
			'pacman --noconfirm -S mingw-w64-i686-python-pillow'
		)

def options(opt):
	opt.load('gcc gxx')

	opt.add_option(
		'--app',
		dest = 'app',
		default = None,
		help = 'App to be run'
	)

	opt.recurse('emulator')

def configure(cfg):
	cfg.load('gcc gxx')

	cfg.recurse('emulator')

	cfg.env.append_value('CFLAGS', '-std=c99')

	cfg.find_program(
		'python',
		var = 'PYTHON'
	)
	cfg.find_program(
		'img_to_src',
		var = 'IMG_TO_SRC',
		exts = '.py',
		path_list = os.path.abspath('.')
	)


def build(bld):
	bld.recurse('emulator')




	mario_sprites = sorted(
	glob.glob('images/mario_sprites/*.png')
	)
	bld(
		rule = '${PYTHON} ${IMG_TO_SRC} -o ${TGT[0]} ${SRC} ' + \
			'-f IDX4 -p 0x000000 -v',
		source = mario_sprites,
		target = ['mario_sprites_idx4.c', 'mario_sprites_idx4.h']
	)

	mario_sprites = sorted(
	glob.glob('images/mario_map/*.png')
	)
	bld(
		rule = '${PYTHON} ${IMG_TO_SRC} -o ${TGT[0]} ${SRC} ' + \
			'-f IDX4 -p 0x000000 -v',
		source = mario_sprites,
		target = ['mario_map_idx4.c', 'mario_map_idx4.h']
	)


	mario_sprites = sorted(
	glob.glob('images/mario_sprites_basic/*.png') +  glob.glob('images/mario_map/*.png')
	)

	bld(
		rule = '${PYTHON} ${IMG_TO_SRC} -o ${TGT[0]} ${SRC} ' + \
			'-f IDX4 -p 0x000000 -v',
		source = mario_sprites,
		target = ['mario_sprites_basic_idx4.c', 'mario_sprites_basic_idx4.h']
	)

	mario_sprites_green = sorted(
	glob.glob('images/mario_sprites_basic_green/*.png') +  glob.glob('images/mario_map/*.png')
	)

	bld(
		rule = '${PYTHON} ${IMG_TO_SRC} -o ${TGT[0]} ${SRC} ' + \
			'-f IDX4 -p 0x000000 -v',
		source = mario_sprites_green,
		target = ['mario_sprites_basic_green_idx4.c', 'mario_sprites_basic_green_idx4.h']
	)




	bld.program(
		features = 'cxx',
		source = ['super_mario.c', 'mario_sprites_idx4.c'],
		includes = ['build/'],
		use = 'emulator',
		target = 'super_mario'
	)

	bld.program(
		features = 'cxx',
                source = ['super_mario_basic.c', 'mario_sprites_basic_green_idx4.c', 'map.c'],
		includes = ['build/'],
		use = 'emulator',
		target = 'super_mario_basic'
	)






def run(ctx):
	'''./waf run --app=<NAME>'''
	if ctx.options.app:
		if sys.platform == 'win32':
			# MSYS2
			ctx.exec_command2('build\\' + ctx.options.app)
		else:
			ctx.exec_command2('./build/' + ctx.options.app)

###############################################################################

def exec_command2(self, cmd, **kw):
	# Log output while running command.
	kw['stdout'] = None
	kw['stderr'] = None
	ret = self.exec_command(cmd, **kw)
	if ret != 0:
		self.fatal('Command "{}" returned {}'.format(cmd, ret))
setattr(waflib.Context.Context, 'exec_command2', exec_command2)

###############################################################################

def recursive_glob(pattern, directory = '.'):
	for root, dirs, files in os.walk(directory, followlinks = True):
		for f in files:
			if fnmatch.fnmatch(f, pattern):
				yield os.path.join(root, f)
		for d in dirs:
			if fnmatch.fnmatch(d + '/', pattern):
				yield os.path.join(root, d)

def collect_git_ignored_files():
	for gitignore in recursive_glob('.gitignore'):
		with open(gitignore) as f:
			base = os.path.dirname(gitignore)

			for pattern in f.readlines():
				pattern = pattern[:-1]
				for f in recursive_glob(pattern, base):
					yield f

###############################################################################

def distclean(ctx):
	for fn in collect_git_ignored_files():
		if os.path.isdir(fn):
			shutil.rmtree(fn)
		else:
			os.remove(fn)

def dist(ctx):
	now = datetime.datetime.now()

	ctx.arch_name = '../{}.zip'.format(APPNAME)
	ctx.algo = 'zip'
	ctx.base_name = APPNAME
	# Also pack git.
	waflib.Node.exclude_regs = waflib.Node.exclude_regs.replace(
'''
**/.git
**/.git/**
**/.gitignore''', '')
	# Ignore waf's stuff.
	waflib.Node.exclude_regs += '\n**/.waf*'

###############################################################################
