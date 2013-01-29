#!/bin/sh
# create WAV-Files that can be used to create an audio Test CD
#
# The intent of this Test CD is to allow you to test the reproduction
# quality and response of an audio system by playing back audio
# of various frequencies and power levels.  Browse through the
# script comments to see what all audio files are created.
#
# all files are created in the current directory
#
# Command Line options:
#   testcd.sh filename_prefix audio_length
#
#   filename_prefix is what each soundfile starts with.  Defaults to
#   using testcd as the prefix.
#   audio_length is the length of audio data in seconds.  Defaults to 30
#   seconds.


# length of sample file in seconds

if  [ "$2"  = "" ] ; then
    LENGTH="30"
else
    LENGTH=$2
fi
# use 'fade' effect for smooth start and end of tone
FT="0.05"

# a different default volume
VOL=""

#output file type
OFT=".wav"

#our binary
SOX=../src/sox

# filenameprefix
if  [ "$1" = "" ] ; then
    PRE="testcd"
else
    PRE=$1;
fi
# 2 channel 16 bit signed linear int with CD sampling rate
SOXOPT="-c 2 -r 44100 -e signed-integer -b 16 -n"

# file with list of filenames
LST="${PRE}.lst"


#summarise seconds
TC="0"
#summarize file numbers
FC="0"


newname()
{
    FC="$(( $FC + 1 ))"
    LEN="$2"
    FADE=" fade $FT $LEN $FT" 

    TC="$(( $TC + $LEN ))"

    if [ $FC -lt 10 ] ; then 
        NAME="${PRE}_0${FC}_${1}${OFT}"
    else
        NAME="${PRE}_${FC}_${1}${OFT}"
    fi
    echo -n -e  " \t$1"
    echo "$NAME" >>$LST
}

#empty / delete list file
echo "" >$LST


#
# ok, lets start with the actual creation of the files
#

#
# fixed frequencies,
FREQ="                   19.4  23.1  27.5  32.7  38.9  46.2"
FREQ="$FREQ  55.0  64.4  77.8  92.5 110.0 130.8 155.6 185.0"
FREQ="$FREQ 220.0 261.6 311.1 367.0 440.0 523.3 622.3 740.0"
FREQ="$FREQ 880.0  1046  1245  1480  1760  2093  2489  2960"
FREQ="$FREQ  3520  4186  4978  5920  7040  8372  9956 11840"
FREQ="$FREQ 14080 16744 19912"
#FREQ="5 10 20 50 100 200 500 1000 2000 5000 10000 20000"
echo "\n--- different frequencies"
for f in $FREQ; do
    newname "${f}hz" $LENGTH
$SOX $SOXOPT $NAME synth $LEN sine $f $FADE $VOL
done



#
# frequency sweep
# need some mark every octave
#
FREQ="220-3520"    #4
FREQ="55-14080"    # 8 oct
FREQ="13.75-28160" # 10 oct
OCT=10
TOCT=10
TGES="$(( $OCT * $TOCT ))"
MARKFREQ=622
echo "\n--- frequency sweep range $FREQ"
newname ${FREQ}hz $TGES 
$SOX $SOXOPT $NAME synth $LEN sine $MARKFREQ synth square amod 0.1 0 97 94 vol -3 db synth $LEN sine mix $FREQ  $VOL

FREQ="3520-220"
FREQ="28160-13.75" # 9 oct
newname ${FREQ}hz $TGES 
$SOX $SOXOPT $NAME synth $LEN sine $MARKFREQ synth square amod 0.1 0 97 94 vol -3 db synth $LEN sine mix $FREQ  $VOL

# CD frequencies
FREQ="22050 11025 5512.5 "
echo "\n--- different frequencies $FREQ"
for f in $FREQ; do
    newname "cd${f}hz" $LENGTH
$SOX $SOXOPT $NAME synth $LEN sine $f $FADE $VOL
done


#
# similar frequencies
#
FREQ1="9000"
FREQ2="10000"
echo "\n--- similar frequencies"
newname ${FREQ1}_${FREQ2} $LENGTH
$SOX $SOXOPT $NAME synth $LEN sine $FREQ1 synth sine mix $FREQ2 $FADE $VOL
FREQ1="440"
FREQ2="445"
newname ${FREQ1}_${FREQ2} $LENGTH
$SOX $SOXOPT $NAME synth $LEN sine $FREQ1 synth sine mix $FREQ2 $FADE $VOL

#
#noise
#
echo "\n--- noise"
newname whitenoise $LENGTH
$SOX  $SOXOPT $NAME synth $LEN whitenoise $FADE $VOL
newname pinknoise $LENGTH
$SOX  $SOXOPT $NAME synth $LEN pinknoise  $FADE $VOL
newname brownnoise $LENGTH
$SOX  $SOXOPT $NAME synth $LEN brownnoise $FADE $VOL



#
# square waves
#
FREQ="100 1000 10000"
echo "\n--- square waves at $FREQ"
for f in $FREQ; do
  newname ${f}_square $LENGTH
$SOX $SOXOPT $NAME synth $LEN square $f vol -12 db $FADE
done


#
# different volumes at a few frequencies
#
FREQ="100 1000 10000"
DB="0 12 24 36 48 60 72 84 96"
echo "\n--- different volumes $DB db at Frequencies $FREQ"
 for f in $FREQ; do
   for d in $DB; do
    newname ${f}_${d}db $LENGTH
    $SOX $SOXOPT $NAME synth $LEN sine $f  vol -$d db $FADE
  done
done

# silence
echo "\n-- silence"
newname silence $LENGTH
$SOX $SOXOPT $NAME synth $LEN sine 1000 vol 0




#
# volume sweep at different frequencies
#

FREQ="100 1000 10000"
MARKFREQ=662
DB=100 # 10sec for 10db
echo "\n--- volume sweep 0..100% at Frequencies $FREQ"
for f in $FREQ; do
  newname ${f}_dbsweep 200
$SOX $SOXOPT $NAME synth $LEN sine $f synth exp amod 0.005 0 0 50 $DB 
done


#
# offset test - a 1K sine with 1Hz square offset of 10%
#
echo "\n--- offset test, 1KHz tone with 1HZ 10% offset"
newname offset $LENGTH
$SOX $SOXOPT $NAME synth $LEN square 1 vol 0.1  synth sine mix 1000 $FADE $VOL
newname offset1 $LENGTH
$SOX $SOXOPT $NAME synth $LEN square 1 0 0 square 1 vol 0.1 $FADE



# effects for different channels
#
# silence on one channel, full power on the other - different frequencies
FREQ="100 1000 10000"
echo "\n--- single channel"
for f in $FREQ; do
 newname ${f}leftchan $LENGTH
 $SOX $SOXOPT $NAME synth $LEN sine $f synth square amod 0 100 square amod 0   0 $Fade $VOL
 newname ${f}rightchan $LENGTH
 $SOX $SOXOPT $NAME synth $LEN sine $f synth square amod 0   0 square amod 0 100 $Fade $VOL
done

# phase error between channels
FREQ="100 1000 10000"
# equal phase/ 24 degrees / 90 degrees / 180 degrees
PHASE="25"
echo "\n--- phase error test between channels at $FREQ "
for f in $FREQ; do
  for p in $PHASE; do
    newname ${f}hz_phase${p} $LENGTH
    $SOX $SOXOPT $NAME synth $LEN sine $f 0 0 sine $f 0 $p  $FADE $VOL
  done
done

#
#
# end - show statistics
#

echo "\n------------------\ncreated $FC files with prefix $PRE type $OFT"
MIN="$(( $TC / 60 ))"
echo "total length is $TC sec = $MIN min"
#---------







