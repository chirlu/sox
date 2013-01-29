#!/bin/bash
#
# crossfade_cat.sh
#
# Concatenates two files together with a crossfade of $1 seconds.
# Filenames are specified as $2 and $3.
#
# $4 is optional and specifies if a fadeout should be performed on
# first file.
# $5 is optional and specifies if a fadein should be performed on
# second file.
#
# Example: crossfade_cat.sh 10 infile1.wav infile2.wav auto auto
#
# By default, the script attempts to guess if the audio files
# already have a fadein/out on them or if they just have really
# low volumes that won't cause clipping when mixxing.  If this
# is not detected then the script will perform a fade in/out to
# prevent clipping.
#
# The user may specify "yes" or "no" to force the fade in/out
# to occur.  They can also specify "auto" which is the default.
#
# Crossfaded file is created as "mix.wav".
#
# Original script from Kester Clegg.  Mods by Chris Bagwell to show
# more examples of sox features.
#

SOX=../src/sox
SOXI=../src/soxi

if [ "$3" == "" ]; then
    echo "Usage: $0 crossfade_seconds first_file second_file [ fadeout ] [ fadein ]"
    echo
    echo "If a fadeout or fadein is not desired then specify \"no\" for that option.  \"yes\" will force a fade and \"auto\" will try to detect if a fade should occur."
    echo
    echo "Example: $0 10 infile1.wav infile2.wav auto auto"
    exit 1
fi

fade_length=$1
first_file=$2
second_file=$3

fade_first="auto"
if [ "$4" != "" ]; then
    fade_first=$4
fi

fade_second="auto"
if [ "$5" != "" ]; then
    fade_second=$5
fi

fade_first_opts=
if [ "$fade_first" != "no" ]; then
    fade_first_opts="fade t 0 0:0:$fade_length 0:0:$fade_length"
fi

fade_second_opts=
if [ "$fade_second" != "no" ]; then
    fade_second_opts="fade t 0:0:$fade_length"
fi

echo "crossfade and concatenate files"
echo
echo  "Finding length of $first_file..."
first_length=`$SOX --info -D "$first_file"`
echo "Length is $first_length seconds"

trim_length=`echo "$first_length - $fade_length" | bc`

# Get crossfade section from first file and optionally do the fade out
echo "Obtaining $fade_length seconds of fade out portion from $first_file..."
$SOX "$first_file" -e signed-integer -b 16 fadeout1.wav trim $trim_length

# When user specifies "auto" try to guess if a fadeout is needed.
# "RMS amplitude" from the stat effect is effectively an average
# value of samples for the whole fade length file.  If it seems
# quiet then assume a fadeout has already been done.  An RMS value
# of 0.1 was just obtained from trial and error.
if [ "$fade_first" == "auto" ]; then
    RMS=`$SOX fadeout1.wav 2>&1 -n stat | grep RMS | grep amplitude | cut -d : -f 2 | cut -f 1`
    should_fade=`echo "$RMS > 0.1" | bc`
    if [ $should_fade == 0 ]; then
        echo "Auto mode decided not to fadeout with RMS of $RMS"
        fade_first_opts=""
    else
        echo "Auto mode will fadeout"
    fi
fi

$SOX fadeout1.wav fadeout2.wav $fade_first_opts

# Get the crossfade section from the second file and optionally do the fade in
echo "Obtaining $fade_length seconds of fade in portion from $second_file..."
$SOX "$second_file" -e signed-integer -b 16 fadein1.wav trim 0 $fade_length

# For auto, do similar thing as for fadeout.
if [ "$fade_second" == "auto" ]; then
    RMS=`$SOX fadein1.wav 2>&1 -n stat | grep RMS | grep amplitude | cut -d : -f 2 | cut -f 1`
    should_fade=`echo "$RMS > 0.1" | bc`
    if [ $should_fade == 0 ]; then
        echo "Auto mode decided not to fadein with RMS of $RMS"
        fade_second_opts=""
    else
        echo "Auto mode will fadein"
    fi
fi

$SOX fadein1.wav fadein2.wav $fade_second_opts

# Mix the crossfade files together at full volume
echo "Crossfading..."
$SOX -m -v 1.0 fadeout2.wav -v 1.0 fadein2.wav crossfade.wav

echo "Trimming off crossfade sections from original files..."

$SOX "$first_file" -e signed-integer -b 16 song1.wav trim 0 $trim_length
$SOX "$second_file" -e signed-integer -b 16 song2.wav trim $fade_length
$SOX song1.wav crossfade.wav song2.wav mix.wav

echo -e "Removing temporary files...\n" 
rm fadeout1.wav fadeout2.wav fadein1.wav fadein2.wav crossfade.wav song1.wav song2.wav
mins=`echo "$trim_length / 60" | bc`
secs=`echo "$trim_length % 60" | bc`
echo "The crossfade in mix.wav occurs at around $mins mins $secs secs"

