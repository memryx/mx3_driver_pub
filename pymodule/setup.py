################################################################################
#  @note
#  Copyright (C) 2019-2022 MemryX Limited. All rights reserved.
#
################################################################################

################################################################################
# export 'memx' module to python
# - ref: https://en.wikibooks.org/wiki/Python_Programming/Extending_with_C
# use '$python3 setup.py build' to create 'memx.*.so' in directory 'build/lib*'
# and it can be imported from python under same directory using 'import memx'.
################################################################################
import os, pathlib, sys
import platform
import numpy as np
from setuptools import setup, Extension

# directories
#source_dirs = [
#  os.path.join('..','udriver'),
#  os.path.join('..','udriver','common'),
#  os.path.join('..','udriver','gbf'),
#  os.path.join('..','udriver','dfp'),
#  os.path.join('..','udriver','mpu'),
#  os.path.join('..','udriver','mpuio'),
#  os.path.join('..','udriver','util'),
#  os.path.join('..','udriver','include'),
#  os.path.join('..','udriver','include','common'),
#  os.path.join('..','udriver','include','gbf'),
#  os.path.join('..','udriver','include','dfp'),
#  os.path.join('..','udriver','include','mpu'),
#  os.path.join('..','udriver','include','mpuio'),
#  os.path.join('..','udriver','include','util'),
#  os.path.join('..','udriver','include','tool')
#]
source_dirs = []

# search for all *.c
sources = ['memxmodule.c']
#for source_dir in source_dirs:
#  sources += [str(source) for source in pathlib.Path(source_dir).glob('*.c')]

if sys.platform.startswith('linux'):
  extra_link_args=['-lmemx']
  extra_compile_args=['-O3','-std=c17','-fno-math-errno','-funsafe-math-optimizations',
                      '-ffinite-math-only','-fno-signed-zeros',
                      '-fno-trapping-math','-fno-signaling-nans',
                      '-fcx-limited-range','-fopenmp']

  # faster conversions with AVX on supported hosts
  if str(platform.machine()).lower() == 'x86_64':
    extra_compile_args += ['-mpopcnt','-msse','-msse2','-msse3','-mssse3','-msse4.1','-msse4.2','-mavx','-mavx2','-mfma','-mbmi','-mbmi2','-maes','-mpclmul','-mcx16',
                           '-mf16c','-mfsgsbase','-mlzcnt','-mmovbe','-mprfchw','-mxsave','-mxsavec','-mxsaves','-mxsaveopt','-msahf','-mclflushopt','-madx','-mtune=generic']

  elif str(platform.machine()).lower() == 'aarch64' or str(platform.machine()).lower() == 'armv8l':
    extra_compile_args += ['-march=armv8-a+simd']

elif sys.platform.startswith('win32'):
  extra_link_args=['udriver.lib',
                   '/LIBPATH:..\\udriver\\build']
  extra_compile_args=['/O2','/arch:AVX2']
else:
  raise RuntimeError("Unsupport platform")

# create extension
module = Extension('mxa', # extension name here will be the name to be imported within python later
  sources=sources,
  include_dirs=source_dirs+[np.get_include()],
  extra_link_args=extra_link_args,
  extra_compile_args=extra_compile_args)

# pack to python module
setup(
  name='memryx',
  version='0.1',
  description='MemryX MPU driver interface',
  ext_modules=[module])

