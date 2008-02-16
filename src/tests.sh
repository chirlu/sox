#!/bin/sh
#
# SoX Regression Test script: Lossless file conversion

# Options:
#verbose=-V
#all=all

bindir="."
builddir="."
srcdir="."

# Allow user to override paths.  Useful for testing an installed
# sox.
while [ $# -ne 0 ]; do
    case "$1" in
        --bindir=*)
        bindir=`echo $1 | sed 's/.*=//'`
        ;;

        -i)
        shift
        bindir=$1
        ;;

        --builddir=*)
        builddir=`echo $1 | sed 's/.*=//'`
        ;;

        -b)
        shift
        builddir=$1
        ;;

        --srcdir=*)
        srcdir=`echo $1 | sed 's/.*=//'`
        ;;

        -c)
        shift
        srcdir=$1
        ;;

        *)
        echo "Unknown option"
        exit 1
    esac
    shift
done

getFormat () {
  formatExt=$1; formatText=$1; formatFlags=""
  case $1 in
    al ) formatText="alaw byte" ;;
    s1 ) formatText="signed byte" ;;
    s4 ) formatText="signed long" ;;
    s2 ) formatText="signed word" ;;
    ul ) formatText="ulaw byte" ;;
    u1 ) formatText="unsigned byte" ;;
    u2 ) formatText="unsigned word" ;;
    raw) formatText="float"; formatFlags="-f -4" ;;
    Raw) formatText="double"; formatFlags="-f -8" ;;
    au ) formatFlags="-s" ;;
    Wav) formatFlags="-u -1" ;;
    s1X ) formatText="signed byte (swap bits)"; formatExt="s1"; formatFlags="-X" ;;
    s1N ) formatText="signed byte (swap nibbles)"; formatExt="s1"; formatFlags="-N" ;;
    s1XN ) formatText="signed byte (swap nibbles and bits)"; formatExt="s1"; formatFlags="-X -N" ;;
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
        echo ${bindir}/sox -c $channels -r $rate -n $format1Flags input.$format1Ext synth $samples's' sin 300-3300 noise trapezium
        echo ${bindir}/sox $verbose -r $rate -c $channels $format1Flags input.$format1Ext $format2Flags intermediate.$format2Ext
        echo ${bindir}/sox $verbose -r $rate -c $channels $format2Flags intermediate.$format2Ext $format1Flags output.$format1Ext
        ${bindir}/sox -R -c $channels -r $rate -n $format1Flags input.$format1Ext synth $samples's' sin 300-3300 noise trapezium
        ${bindir}/sox $verbose -r $rate -c $channels $format1Flags input.$format1Ext $format2Flags intermediate.$format2Ext
        ${bindir}/sox $verbose -r $rate -c $channels $format2Flags intermediate.$format2Ext $format1Flags output.$format1Ext
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
  format1=u1
  convertToAndFrom s1 u1 s2 u2 s3 u3 s4 u4 raw Raw dat au wav aiff aifc flac caf sph

  format1=s2
  convertToAndFrom s2 u2 s3 u3 s4 u4 raw Raw dat au wav aiff aifc flac caf sph

  format1=u3
  convertToAndFrom s3 u3 s4 u4 raw Raw wav aiff aifc flac # FIXME: sph

  format1=s4
  convertToAndFrom s4 u4 Raw wav aiff aifc caf sph

  format1=al
  convertToAndFrom al s2 u2 s4 raw Raw dat aiff aifc flac caf # FIXME: sph

  format1=ul
  convertToAndFrom ul s2 u2 s4 raw Raw dat aiff aifc flac caf sph

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
  convertToAndFrom vox s2 u2 s3 u3 s4 u4 raw Raw dat au wav aiff aifc flac caf # FIXME: ima

  format1=ima
  convertToAndFrom ima s2 u2 s3 u3 s4 u4 raw Raw dat au aiff aifc flac caf # FIXME: vox wav

  format1=Wav
  convertToAndFrom smp s1 s1X s1N s1XN sndt
  (rate=5512; convertToAndFrom hcom) || exit 1     # Fixed rate

  format1=wve
  (rate=8000; convertToAndFrom al s2 u2 s4 raw Raw dat) || exit 1 # Fixed rate

  format1=prc
  (rate=8000; convertToAndFrom al s2 u2 s4 raw Raw dat) || exit 1 # Fixed rate
}

# Reading and writing performance test
timeIO () {
  while [ $# != 0 ]; do
      if [ "${skip}x" != "x" ] ; then
        from_skip=`echo ${skip} | grep ${1}`
      fi
      if [ "${from_skip}x" = "x" ] ; then
        getFormat $1;
        echo ${bindir}/sox -c $channels -r $rate -n $formatFlags input.$1 synth $samples's' sin 300-3300 noise trapezium
        echo time ${bindir}/sox $verbose -r $rate -c $channels $formatFlags input.$1 $formatFlags output.$1
        ${bindir}/sox -c $channels -r $rate -n $formatFlags input.$1 synth $samples's' sin 300-3300 noise trapezium
        time ${bindir}/sox $verbose -r $rate -c $channels $formatFlags input.$1 $formatFlags output.$1

        rm -f input.$1 output.$1
      fi
      shift
  done
}


# Run tests

${builddir}/sox_sample_test || exit 1

# Don't test unsupported stuff
${bindir}/sox --help | grep -q "^AUDIO FILE.*\<flac\>" || skip="flac $skip"
${bindir}/sox --help | grep -q "^AUDIO FILE.*\<caf\>" || skip="caf $skip"

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
convertToAndFrom s2 u3 aiff

channels=1 
if [ "$all" = "all" ]; then
  do_multichannel_formats
  do_twochannel_formats
fi
do_singlechannel_formats

${srcdir}/test-comments
if [ $? -eq 0 ]; then
  echo "ok     comments"
else
  echo "*FAIL* comments"
  exit 1
fi

${bindir}/sox -c 1 -r 44100 -n output.u1 synth .01 vol .5
if [ `wc -c <output.u1` = 441 ]; then
  echo "ok     synth size"
else
  echo "*FAIL* synth size"
fi
rm output.u1

channels=2
samples=10000000
timeIO s1 u1 s2 u2 s3 u3 s4 u4 raw Raw au wav aiff aifc caf sph # FIXME?: flac dat

test -n "$skip" && echo "Skipped: $skip"

# Run one last command so return code is not error
# when $skip is empty.
echo "done."
