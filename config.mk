CFLAGS=-g
LDFLAGS=

CXX=g++

INSTALL?=install
MAKE?=make

prefix=/usr/local

soversion=0
libname=liblecroy_vxi11.so
full_libname=$(libname).$(soversion)
