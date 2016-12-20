import os
import subprocess

config_template = """
CPPTRAJHOME={cpptraj_home}
CPPTRAJBIN={cpptraj_bin}
CPPTRAJLIB={cpptraj_lib}

INSTALL_TARGETS= install_cpptraj install_ambpdb

CC=gcc
CXX=g++
FC=gfortran
# CFLAGS= -O3 -Wall  -DHASBZ2 -DHASGZ -DBINTRAJ -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I{include_dir} -Ixdrfile  -fPIC $(DBGFLAGS)
# CXXFLAGS= -O3 -Wall  -DHASBZ2 -DHASGZ -DBINTRAJ -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I{include_dir}  -Ixdrfile  -fPIC -I{include_dir} $(DBGFLAGS)
# FFLAGS= -O3  -DHASBZ2 -DHASGZ -DBINTRAJ -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I{include_dir} -Ixdrfile -ffree-form -fPIC $(DBGFLAGS)
CFLAGS= -O3 -Wall  -DBINTRAJ -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I{include_dir} -Ixdrfile  -fPIC $(DBGFLAGS)
CXXFLAGS= -O3 -Wall  -DBINTRAJ -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I{include_dir}  -Ixdrfile  -fPIC -I{include_dir} $(DBGFLAGS)
FFLAGS= -O3  -DBINTRAJ -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 -I{include_dir} -Ixdrfile -ffree-form -fPIC $(DBGFLAGS)
SHARED_SUFFIX=.so

LIBCPPTRAJ_TARGET=$(CPPTRAJLIB)/libcpptraj$(SHARED_SUFFIX)

NVCC=
NVCCFLAGS= $(DBGFLAGS)
CUDA_TARGET=

READLINE=
READLINE_HOME=readline
READLINE_TARGET=noreadline

XDRFILE=xdrfile/libxdrfile.a
XDRFILE_HOME=xdrfile
XDRFILE_TARGET=xdrfile/libxdrfile.a

FFT_DEPEND=pub_fft.o
FFT_LIB=pub_fft.o

CPPTRAJ_LIB=  -L{lib_dir} -lopenblas -lgfortran -w
CPPTRAJ_LIB=  -L{lib_dir} -lmkl -lgfortran -w
LDFLAGS=-L{lib_dir} -lnetcdf -lz xdrfile/libxdrfile.a 
SFX=
EXE=
"""

include_dir = os.getenv('LIBRARY_INC')
lib_dir = os.getenv('LIBRARY_LIB')
cpptraj_home = os.getcwd()
cpptraj_bin = os.path.join(cpptraj_home, 'bin')
cpptraj_lib = os.path.join(cpptraj_home, 'lib')

try:
    os.mkdir('lib')
    os.mkdir('bin')
except OSError:
    pass

with open('config.h', 'w') as fh:
    fh.write(config_template.format(cpptraj_home=cpptraj_home,
        cpptraj_bin=cpptraj_bin,
        cpptraj_lib=cpptraj_lib,
        include_dir=include_dir,
        lib_dir=lib_dir))

with open('testp.cpp', 'w') as fh:
    fh.write("""
    #include <cstdio>
    #include "netcdf.h"
    void unused() {int ncid; nc_open("foo.nc", 0, &ncid);}
    int main() { printf("Testing\n"); printf("%s\n",nc_strerror(0)); return 0; }
    """.strip())

subprocess.call('ls {}'.format(include_dir), shell=True)
subprocess.call('g++ -I{include_dir} -o testp testp.cpp'.format(include_dir=include_dir), shell=True)
