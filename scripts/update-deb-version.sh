#/bin/sh

BASE_KERNEL=4.4.138
DATE=`date +%Y%m%d`
SCEL_VER=t.2.0.2.2
COMMENT=$1

if [ $1 ]
then
 fakeroot debian/rules clean
 echo "Current deb build package will be " $BASE_KERNEL.$DATE-$SCEL_VER
 export DEBEMAIL="sony@sony.com"
 export DEBFULLNAME="SCEL developer"
 dch -v $BASE_KERNEL.$DATE-$SCEL_VER -b $COMMENT
else
 echo "Please add your comment"
fi

