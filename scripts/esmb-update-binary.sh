#!/bin/bash
######################################################################
# update-binary.sh
#
# Copyright 2015 Sony Corporation.
#
# Author: Ananda Kumar B
######################################################################
source `pwd`/scripts/esmb.conf

if [[ $ARCH == "arm" ]]; then
	CC_PREFIX=arm-linux-gnueabihf-
	SEARCH="ARM,"
elif [[ ($ARCH == "x86") || ($ARCH == "x86_64") ]]; then
	CC_PREFIX=
	SEARCH="x86-64,"
fi
OBJCOPY=$CC_PREFIX"objcopy"
READ_ELF=$CC_PREFIX"readelf"
CUR_DIR=`pwd`
METADATA_FILE=$CUR_DIR/esmbdata.txt

## Subsystem Name ###
SCEL=1
KERNEL=2
BDK_SYSTEM_SERVICE=3
ZIRCON=4
VISOS=5
OTHER=6

### SCM TYPE ###
BDK_REPO=1
GIT=2
CVS=3

# Read input file path for em log if provided.
if [ $# != 0 ]
then
	# BIN_PATH -  base path where the binariy files are located
	BIN_PATH=$1

	# Below three lines of code, gets the recent commitID and git repo name
	commitID=`git rev-parse --verify HEAD`
	BASE_PATH=`cat .git/config | grep "url =" | awk '{print $3}'`
	GIT_NAME=`basename $BASE_PATH`

	BIN_PKG_NAME=$KERNEL_BIN_PKG_NAME

	VERSION="$KERNELVERSION-$SCEL_VER"
	# Framing the esmb metadata format and copying to a file
	echo "<SUB=$SUBSYSTEM_NAME_VALUE><SCM=$SCM_TYPE_VALUE><BIN=$BIN_PKG_NAME><NAM=$GIT_NAME><VER=$VERSION><CID=$commitID>" > $METADATA_FILE

	# Recursively searches all the folders under $BIN_PATH, finds the binary(shared object/executable binary)
	# and embed the esmb metadata info from $METADATA_FILE to the binary
	cd $BIN_PATH

	find . -name *.ko > ./log-find.txt
	find . -name vmlinux >> ./log-find.txt

	for f in `cat ./log-find.txt`
	do
		IS_BIN=`file $f`
		echo $IS_BIN | grep $SEARCH && echo $IS_BIN | grep -E "executable|relocatable|shared object"
		if [ $? == 0 ]
		then
		    res_readelf=`$READ_ELF -p ".esmbdata" $f 2> /dev/null`
		    echo $res_readelf | grep -e "String dump of section '.esmbdata':"
		    if [ $? != 0 ]
		    then
			$OBJCOPY --add-section \
			.esmbdata=$METADATA_FILE --set-section-flags \
			.esmbdata=noload,readonly $f
		    fi
		fi
	done
	rm ./log-find.txt
	cd -
	rm $METADATA_FILE
else
	echo "Pass the base binary path"
	echo "Metadata is not added to the binary"
fi
