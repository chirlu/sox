#!/bin/sh
#
# SOX Regression Test script.
#
# This script is just a quick sanity check of SOX on lossless format conversions.

# verbose options
#verbose=-V

getFormat () {
  formatText=$1; formatFlags=""
  case $1 in
    al ) formatText="alaw byte" ;;
    sb ) formatText="signed byte" ;;
    sl ) formatText="signed long" ;;
    sw ) formatText="signed word" ;;
    ul ) formatText="ulaw byte" ;;
    ub ) formatText="unsigned byte" ;;
    uw ) formatText="unsigned word" ;;
    raw) formatText="float"; formatFlags="-f -l" ;;
    au ) formatFlags="-s -b" ;;
    wav) formatFlags="-u -b" ;;
  esac
}
  
convertToAndFrom () {
  while [ $# != 0 ]; do
    getFormat $format1; format1Text=$formatText; format1Flags=$formatFlags
    getFormat       $1; format2Text=$formatText; format2Flags=$formatFlags
    ./sox $verbose -r $rate monkey.au $format1Flags input.$format1
    ./sox $verbose -r $rate -c 1 $format1Flags input.$format1 $format2Flags intermediate.$1
    ./sox $verbose -r $rate -c 1 $format2Flags intermediate.$1 $format1Flags output.$format1
    if cmp -s input.$format1 output.$format1
    then
      echo "ok     convert \"$format1Text\" <--> \"$format2Text\"."
    else
      echo "*FAIL* convert \"$format1Text\" <--> \"$format2Text\"."
      exit 1    # This allows failure inspection.
    fi
    rm -f input.$format1 intermediate.$1 output.$format1
    shift
  done
}

format1=ub
rate=8012
convertToAndFrom sb sw sl al uw raw

format1=sw
convertToAndFrom sl ul uw raw

format1=wav
convertToAndFrom 8svx aiff au avr dat maud sf smp
rate=5512
convertToAndFrom hcom
rate=8000
convertToAndFrom voc wve flac
