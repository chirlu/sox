#!/bin/bash
#
# crossfade_cat.sh
#
# Original script from Kester Clegg.  Mods by Chris Bagwell to show
# more examples of sox features.
# Concatenates two files together with a crossfade of $1 seconds.
# Filenames are specified as $2 and $3.
#
# By default, script fades out first file and fades in second file.
# This makes sure that we can mix the two files together with
# no clipping of audio.  If that option is overridden then the
# user must make sure no clipping can occur themselves.
#
# $4 is optional and specifies if a fadeout should be performed on
# first file.
# $5 is optional and specifies if a fadein should be performed on
# second file.
#
# Crossfaded file is created as "mix.wav".

SOX=../src/sox
SOXMIX=../src/soxmix

if [ "$3" == "" ]; then
    echo "Usage: $0 crossfade_seconds first_file second_file [ fadeout ] [ fadein ]"
    echo
    echo "If a fadeout or fadein is not desire then specify \"no\" for that option"
    exit 1
fi

fade_length=$1
first_file=$2
second_file=$3

fade_first="yes"
if [ "$4" != "" ]; then
    fade_first=$4
fi

fade_second="yes"
if [ "$5" != "" ]; then
    fade_second=$5
fi

fade_first_opts=
if [ "$fade_first" == "yes" ]; then
    fade_first_opts="fade h 0 0:0:$fade_length"
fi

fade_second_opts=
if [ "$fade_second" == "yes" ]; then
    fade_second_opts="fade h 0:0:$fade_length"
fi

echo "crossfade and concatenate files"
echo
echo  "Finding length of $first_file..."
first_length=`$SOX "$first_file" 2>&1 -e stat | grep Length | cut -d : -f 2 | cut -d . -f 1 | cut -f 1`
echo "Length is $first_length"

trim_length=`expr $first_length - $fade_length`

# Get crossfade section from first file and optionally do the fade out
echo "Obtaining $fade_length seconds of fade out portion from $first_file..."
$SOX "$first_file" -s -w fadeout.wav trim $trim_length $fade_first_opts
# Get the crossfade section from the second file and optionally do the fade in
echo "Obtaining $fade_length seconds of fade in portion from $second_file..."
$SOX "$second_file" -s -w fadein.wav trim 0 $fade_length $fade_second_opts
# Mix the crossfade files together at full volume
echo "Crossfading..."
$SOXMIX -v 1.0 fadeout.wav -v 1.0 fadein.wav crossfade.wav

echo "Trimming off crossfade sections from original files..."

$SOX "$first_file" -s -w song1.wav trim 0 $trim_length
$SOX "$second_file" -s -w song2.wav trim $fade_length
$SOX song1.wav crossfade.wav song2.wav mix.wav

echo -e "Removing temporary files...\n" 
rm fadeout.wav fadein.wav crossfade.wav song1.wav song2.wav
mins=`expr $trim_length / 60`
secs=`expr $trim_length % 60`
echo "The crossfade in mix.wav occurs at around $mins mins $secs secs"

