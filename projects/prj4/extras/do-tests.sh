#!/bin/sh

if [ $# -lt 1 ]
then
   echo "usage: $0 SRC_DIR [TEST_FILE_PATH...]" 
   exit 1
fi

if [ ! -d "$1" ]
then
    echo "'$1' is not a directory"
    exit 1
fi

srcDir=`realpath $1`
shift

this=`dirname $0`;
thisDir=`realpath $this`;
testDir=$thisDir/tests

BUILD_DIR=$HOME/tmp/prj4-sol
mkdir -p $BUILD_DIR

cd $BUILD_DIR
rm -f $db
ln -sf $srcDir/*.[ch] .
ln -sf $srcDir/Makefile .
ln -sf $thisDir/*.[ch] .
make 

if [ $# -eq 0 ]
then
   cmpTests=$testDir/*.test
else
    cmpTests=$@
fi

rm -f *.db

PROG=./chat
SHM_SIZE=1  #in KiB
outDir=$HOME/tmp

for t in $cmpTests
do
    base=`basename $t .test`
    out="$outDir/$base.out"
    gold=$testDir/$base.out
    db=':memory:'
    echo "$base" | grep -q 'db:';
    if [ $? -eq 0 ]
    then
	db=`echo "$base" | sed -e 's/.*db://'`.db
    fi
    $PROG $db $SHM_SIZE < $t > $out 2>&1
    echo "$t" | grep -q '-err\.test$'
    [ $? -eq 0 ] && sed -i -e '/^err /s/:.*//' $out       
    if cmp $gold $out > /dev/null
    then
	echo "`basename $t` ok"
	rm $out
    else
	echo "test $t failed; output in $out"
	echo "run 'diff $gold $out' to see differences"
    fi
done

    

