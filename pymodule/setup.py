################################################################################
#  @note
#  Copyright (c) 2019-2025 MemryX Inc.
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
#  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
#  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
#  CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
#  TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
#  SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
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
  extra_compile_args=['-O3']

  # faster conversions with AVX on supported hosts
  if str(platform.machine()).lower() == 'x86_64':
    extra_compile_args += ['-mpopcnt','-msse','-msse2','-msse3','-mssse3','-msse4.1','-msse4.2','-mavx','-mavx2','-mfma','-mbmi','-mbmi2','-maes','-mpclmul','-mcx16','-mf16c','-mfsgsbase','-mlzcnt','-mmovbe']

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

