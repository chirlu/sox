#!/bin/sh
#
# SoX Regression Test script: Lossless file conversion
#
# FIXME: Test sndt sph ogg

# Options:
#verbose=-V
#all=all

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
      if [ "${skip}x" != "x" ] ; then
        format1_skip=`echo ${skip} | grep ${format1}`
        from_skip=`echo ${skip} | grep ${1}`
      fi
      if [ "${format1_skip}x" = "x" -a "${from_skip}x" = "x" ] ; then
        getFormat ${format1}; format1Text=$formatText; format1Flags=$formatFlags
        getFormat       $1; format2Text=$formatText; format2Flags=$formatFlags
        ./sox -c $channels -r $rate -n $format1Flags input.$format1 synth $samples's' sin 300-3300 noise trapezium
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
      fi
      shift
  done
}

do_multichannel_formats () {
  format1=ub
  convertToAndFrom sb ub sw uw s3 u3 sl u4 raw Raw dat au wav aiff aifc flac caf

  format1=sw
  convertToAndFrom sw uw s3 u3 sl u4 raw Raw dat au wav aiff aifc flac caf

  format1=u3
  convertToAndFrom s3 u3 sl u4 raw Raw wav aiff aifc flac

  format1=sl
  convertToAndFrom sl u4 Raw wav aiff aifc caf

  format1=al
  convertToAndFrom al sw uw sl raw Raw dat aiff aifc flac caf

  format1=ul
  convertToAndFrom ul sw uw sl raw Raw dat aiff aifc flac caf

  format1=Wav
  convertToAndFrom Wav aiff aifc au dat sf flac caf
}

do_twochannel_formats () {
  format1=Wav
  convertToAndFrom avr maud
  (rate=8000; convertToAndFrom voc) || exit 1      # Fixed rate
  (samples=23492; convertToAndFrom 8svx) || exit 1 # Even number of samples only
}

do_singlechannel_formats () {
  format1=vox
  convertToAndFrom vox sw uw s3 u3 sl u4 raw Raw dat au wav aiff aifc flac caf

  format1=Wav
  convertToAndFrom smp
  (rate=5512; convertToAndFrom hcom) || exit 1     # Fixed rate

  format1=wve
  (rate=8000; convertToAndFrom al sw uw sl raw Raw dat) || exit 1 # Fixed rate

  format1=prc
  (rate=8000; convertToAndFrom al sw uw sl raw Raw dat) || exit 1 # Fixed rate
}

grep -q "^#define HAVE_LIBFLAC" stconfig.h || skip="flac $skip"
grep -q "^#define HAVE_LIBOGG" stconfig.h || skip="ogg $skip"
grep -q "^#define HAVE_SNDFILE_H" stconfig.h || skip="caf $skip"

rate=44100
samples=23493

channels=3 
do_multichannel_formats

channels=2 
if [ "$all" = "all" ]; then
  do_multichannel_formats
fi
do_twochannel_formats
format1=cdda         # 2-channel only
convertToAndFrom sw u3 aiff

channels=1 
if [ "$all" = "all" ]; then
  do_multichannel_formats
  do_twochannel_formats
fi
do_singlechannel_formats

./sox -c 1 -n output.ub synth .01 vol .5
if [ `wc -c <output.ub` = 441 ]; then
  echo "ok     synth size"
else
  echo "*FAIL* synth size"
fi
rm output.ub

test -n "$skip" && echo "Skipped: $skip"
