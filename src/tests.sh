#!/bin/sh
#
# SoX Regression Test script.
#
# This script is just a quick sanity check of SoX on lossless format conversions.

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
    Raw) formatText="double"; formatFlags="-f -8" ;;
    au ) formatFlags="-s" ;;
    Wav) formatFlags="-u -b" ;;
  esac
}
  
convertToAndFrom () {
  while [ $# != 0 ]; do
    getFormat $format1; format1Text=$formatText; format1Flags=$formatFlags
    getFormat       $1; format2Text=$formatText; format2Flags=$formatFlags
    ./sox -c $channels -r $rate -n $format1Flags input.$format1 synth $samples's' sin 300-3300 noise
    ./sox $verbose -r $rate -c $channels $format1Flags input.$format1 $format2Flags intermediate.$1
    ./sox $verbose -r $rate -c $channels $format2Flags intermediate.$1 $format1Flags output.$format1

    if cmp -s input.$format1 output.$format1
    then
      echo "ok     channels=$channels \"$format1Text\" <--> \"$format2Text\"."
    else
      echo "*FAIL* channels=$channels \"$format1Text\" <--> \"$format2Text\"."
      exit 1    # This allows failure inspection.
    fi
    rm -f input.$format1 intermediate.$1 output.$format1
    shift
  done
}

do_multichannel_formats () {
  format1=ub
  convertToAndFrom sb ub sw uw s3 u3 sl u4 raw Raw dat au wav aiff aifc flac

  format1=sw
  convertToAndFrom sw uw s3 u3 sl u4 raw Raw dat au wav aiff aifc flac

  format1=u3
  convertToAndFrom s3 u3 sl u4 raw Raw wav aiff aifc flac

  format1=sl
  convertToAndFrom sl u4 Raw wav

  format1=al
  convertToAndFrom al sw uw sl raw Raw dat

  format1=ul
  convertToAndFrom ul sw uw sl raw Raw dat

  format1=Wav
  convertToAndFrom Wav aiff aifc au avr dat maud sf flac
  samples=23492 convertToAndFrom 8svx  # Even number of samples only
  rate=8000 convertToAndFrom voc       # Fixed rate
}

do_singlechannel_formats () {
  format1=Wav
  convertToAndFrom smp
  rate=5512 convertToAndFrom hcom      # Fixed rate

  rate=8000
  format1=wve
  convertToAndFrom al sw uw sl raw Raw dat
}

rate=44100
samples=23493
channels=2 
do_multichannel_formats
channels=1 
do_multichannel_formats
channels=1 
do_singlechannel_formats

./sox -c 1 -n output.ub synth .01 vol .5
if [ `wc -c <output.ub` = 441 ]; then
  echo "ok     synth size"
else
  echo "*FAIL* synth size"
fi
rm output.ub
