#!/bin/sh
#
# SoX Regression Test script: Lossless file conversion
#
# FIXME: Test sndt

# Options:
#verbose=-V
#all=all

getFormat () {
  formatExt=$1; formatText=$1; formatFlags=""
  case $1 in
    al ) formatText="alaw byte" ;;
    sb ) formatText="signed byte" ;;
    sl ) formatText="signed long" ;;
    sw ) formatText="signed word" ;;
    ul ) formatText="ulaw byte" ;;
    ub ) formatText="unsigned byte" ;;
    uw ) formatText="unsigned word" ;;
    raw) formatText="float"; formatFlags="-f -4" ;;
    Raw) formatText="double"; formatFlags="-f -8" ;;
    au ) formatFlags="-s" ;;
    Wav) formatFlags="-u -1" ;;
    sbX ) formatText="signed byte (swap bits)"; formatExt="sb"; formatFlags="-X" ;;
    sbN ) formatText="signed byte (swap nibbles)"; formatExt="sb"; formatFlags="-N" ;;
    sbXN ) formatText="signed byte (swap nibbles and bits)"; formatExt="sb"; formatFlags="-X -N" ;;
  esac
}
  
convertToAndFrom () {
  while [ $# != 0 ]; do
      if [ "${skip}x" != "x" ] ; then
        format1_skip=`echo ${skip} | grep ${format1}`
        from_skip=`echo ${skip} | grep ${1}`
      fi
      if [ "${format1_skip}x" = "x" -a "${from_skip}x" = "x" ] ; then
        getFormat ${format1}; format1Ext=$formatExt; format1Text=$formatText; format1Flags=$formatFlags
        getFormat         $1; format2Ext=$formatExt; format2Text=$formatText; format2Flags=$formatFlags
        echo ./sox -c $channels -r $rate -n $format1Flags input.$format1Ext synth $samples's' sin 300-3300 noise trapezium
        echo ./sox $verbose -r $rate -c $channels $format1Flags input.$format1Ext $format2Flags intermediate.$format2Ext
        echo ./sox $verbose -r $rate -c $channels $format2Flags intermediate.$format2Ext $format1Flags output.$format1Ext
        ./sox -R -c $channels -r $rate -n $format1Flags input.$format1Ext synth $samples's' sin 300-3300 noise trapezium
        ./sox $verbose -r $rate -c $channels $format1Flags input.$format1Ext $format2Flags intermediate.$format2Ext
        ./sox $verbose -r $rate -c $channels $format2Flags intermediate.$format2Ext $format1Flags output.$format1Ext
        intermediateReference=intermediate`echo "$channels $rate $format1Flags $format1Ext $format2Flags"|tr " " "_"`.$format2Ext

	# Uncomment to generate new reference files
	# N.B. new reference files must be manually checked for correctness
        #cp -i intermediate.$format2Ext $intermediateReference

        if test -f $intermediateReference
        then
          if ! cmp -s $intermediateReference intermediate.$format2Ext
          then
            echo "*FAIL* channels=$channels \"$format1Text\" ---> \"$format2Text\"."
            exit 1    # This allows failure inspection.
          fi
        fi

        if cmp -s input.$format1Ext output.$format1Ext
        then
          echo "ok     channels=$channels \"$format1Text\" <--> \"$format2Text\"."
        else
          echo "*FAIL* channels=$channels \"$format1Text\" <--> \"$format2Text\"."
          exit 1    # This allows failure inspection.
        fi
        rm -f input.$format1Ext intermediate.$format2Ext output.$format1Ext
      fi
      shift
  done
}

do_multichannel_formats () {
  format1=ub
  convertToAndFrom sb ub sw uw s3 u3 sl u4 raw Raw dat au wav aiff aifc flac caf sph

  format1=sw
  convertToAndFrom sw uw s3 u3 sl u4 raw Raw dat au wav aiff aifc flac caf sph

  format1=u3
  convertToAndFrom s3 u3 sl u4 raw Raw wav aiff aifc flac # FIXME: sph

  format1=sl
  convertToAndFrom sl u4 Raw wav aiff aifc caf sph

  format1=al
  convertToAndFrom al sw uw sl raw Raw dat aiff aifc flac caf # FIXME: sph

  format1=ul
  convertToAndFrom ul sw uw sl raw Raw dat aiff aifc flac caf sph

  format1=Wav
  convertToAndFrom Wav aiff aifc au dat sf flac caf sph
}

do_twochannel_formats () {
  format1=Wav
  convertToAndFrom avr maud
  (rate=8000; convertToAndFrom voc) || exit 1      # Fixed rate
  (samples=23492; convertToAndFrom 8svx) || exit 1 # Even number of samples only
}

do_singlechannel_formats () {
  format1=vox
  convertToAndFrom vox sw uw s3 u3 sl u4 raw Raw dat au wav aiff aifc flac caf # FIXME: ima

  format1=ima
  convertToAndFrom ima sw uw s3 u3 sl u4 raw Raw dat au aiff aifc flac caf # FIXME: vox wav

  format1=Wav
  convertToAndFrom smp sb sbX sbN sbXN
  (rate=5512; convertToAndFrom hcom) || exit 1     # Fixed rate

  format1=wve
  (rate=8000; convertToAndFrom al sw uw sl raw Raw dat) || exit 1 # Fixed rate

  format1=prc
  (rate=8000; convertToAndFrom al sw uw sl raw Raw dat) || exit 1 # Fixed rate
}

# Reading and writing performance test
timeIO () {
  while [ $# != 0 ]; do
      if [ "${skip}x" != "x" ] ; then
        from_skip=`echo ${skip} | grep ${1}`
      fi
      if [ "${from_skip}x" = "x" ] ; then
        getFormat $1;
        echo ./sox -c $channels -r $rate -n $formatFlags input.$1 synth $samples's' sin 300-3300 noise trapezium
        echo time ./sox $verbose -r $rate -c $channels $formatFlags input.$1 $formatFlags output.$1
        ./sox -c $channels -r $rate -n $formatFlags input.$1 synth $samples's' sin 300-3300 noise trapezium
        time ./sox $verbose -r $rate -c $channels $formatFlags input.$1 $formatFlags output.$1

        rm -f input.$1 output.$1
      fi
      shift
  done
}


# Run tests

grep -q "^#define HAVE_LIBFLAC" soxconfig.h || skip="flac $skip"
grep -q "^#define HAVE_LIBOGG" soxconfig.h || skip="ogg $skip"
grep -q "^#define HAVE_SNDFILE_H" soxconfig.h || skip="caf $skip"

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

channels=2
samples=10000000
timeIO sb ub sw uw s3 u3 sl u4 raw Raw au wav aiff aifc caf sph # FIXME?: flac dat

./sox -c 1 -n output.ub synth .01 vol .5
if [ `wc -c <output.ub` = 441 ]; then
  echo "ok     synth size"
else
  echo "*FAIL* synth size"
fi
rm output.ub

test -n "$skip" && echo "Skipped: $skip"
