#!/bin/sh

DATE=`date +%Y%m%d`
SCEL_VER=t.2.0.2.2
KERNEL_VER=4.4.138
BUILD_VER=r1

# Require inclusion of "make prepare" to generate include/config/*
# related files
./setup-ribosome-rx-dp
make prepare
make kbuild_headers
fakeroot debian/rules binary-indep full_build=true
fakeroot debian/rules binary-arch-headers
