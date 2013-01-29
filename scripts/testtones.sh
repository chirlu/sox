#!/bin/sh

# SoX script: testtones.sh               (c) 2009 robs@users.sourceforge.net
# Based on an original idea by Carsten Borchardt
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.
#
# This program is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General
# Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.



# Usage: testtones.sh [RATE [AUDIO_LENGTH]]
#
# This script generate files containing audio tones of various
# frequencies and power levels that can be used to test an audio
# system's reproduction quality and response.  By default, the
# generated files are suitable for writing to a test CD (using a
# programme such as k3b).
#
# The audio files are created in the current directory and a list of
# the files generated is also created.  A description of each
# generated file is sent to the console as the script executes.
#
# RATE (default 44100) is the sample-rate at which to generate the
#   test tones.  Use 48000 to test a PC/work-station or DAT system.
#
# AUDIO_LENGTH (default 36) sets the length in seconds of (most of)
#   the test-tones.
#
# Other parameters can be changed in the Configuration section below.



# Configuration:

sox="../src/sox"	# Where is sox?  E.g. sox, /usr/local/bin/sox
type=wav		# File type, e.g. flac, cdda
rate=44100		# Default sample rate
#chans2="-c 2"		# Uncomment for all files to have 2 channels
part_octave=6		# 5 for harmonic split, 6 for true half octave
tone_length=36		# In seconds; not used for sweeps
fade_time=.05		# Fade in/out time (for smooth start and end of tone)
gain="gain -1"		# Headroom for playpack chain
#NDD=-D			# Uncomment to disable TPDF default dither
vol_dither="-f ge"	# Type of dither to be used for volume tests
plot=yes		# Uncomment to enable PNG plots



plot()
{
  [ -n "$plot" ] && [ -n "$name" ] && $sox -D "$name" -n \
    spectrogram -h -w kaiser -o "`basename "$name" $type`png"
}



next_file()		# Generate incrementing file name; update variables
{
  plot  # the previous file

  file_count=$(($file_count + 1))
  name=`printf "%02i-%s.$type" $file_count "$1" | tr / "~"`
  echo "  $name"
  echo "$name" >> $log

  length=$tone_length; [ -n "$2" ] && length=$2
  total_length=$(($total_length + $length))

  output="-b 16 $chans2 --comment="" $name"
  fade="fade h $fade_time $length $fade_time"
}



# Initialisation:

[ -n "$1" ] && rate="$1"
[ -n "$2" ] && tone_length="$2"

log="`basename "$0"`.log"
: > $log					# Initialise log file

total_length=0					# In seconds
file_count=0

freqs="100 1k 10k"
input="$sox $NDD -r $rate -n"

echo "Creating test audio files with sample-rate = $rate"



echo; echo "Silence:"

next_file silence
$input -D $output trim 0 $length



echo; echo "Noise colours:"

next_file white-noise
$input $output synth $length whitenoise $fade $gain
next_file pink-noise
$input $output synth $length pinknoise $fade $gain
next_file brown-noise
$input $output synth $length brownnoise $fade $gain



echo; echo "Single, fixed frequency pure tones, half octave steps:"

note=-60			# 5 octaves below middle A = 13.75Hz
while [ $note -le 67 ]; do	# 5 and a bit octaves above middle A ~= 20kHz
  if [ -x /usr/bin/bc ]; then
    freq=`echo "scale = 9; 440 * e($note / 12 * l(2))" | bc -l`
    next_file `printf "%.1f" $freq`Hz
  else
    next_file %$note
  fi
  $input $output synth -j 0 sine %$note $fade $gain
  note=$(($note + $part_octave))
  part_octave=$((12 - $part_octave))
done



echo; echo "Single, fixed frequency pure tones, decade/sub-decade steps:"

for freq in 20 50 100 200 500 1k 2k 5k 10k 20k; do
  next_file "${freq}hz"
  $input $output synth sine $freq $fade $gain
done



echo; echo "Single, fixed frequency pure tones, CD frequency sub-harmonics:"

for freq in 5512.5 11025 16537.5 22050; do
  next_file "cd-${freq}hz"
  $input $output synth $length sine $freq 0 25 $fade $gain
done



echo; echo "Sweep frequency @ 1 semitone/s, with a mark @ each octave of \`A':"

phase1=75
phase2=1
for freq in %-60/%67 %67/%-60; do
  next_file ${freq}_sweep 127
  $input $output synth sine %30 \
             synth sine amod 8.333333333333333333 0 $phase1 \
             synth squa amod 0.08333333333333333333 0 $phase2 1 gain -9 \
             synth $length sine mix $freq gain -h 3 $gain
  phase1=41.66666666666666667
  phase2=42.66666666666666667
done



echo; echo "Dual, fixed frequency pure tones:"

next_file 9kHz+10kHz
$input $output synth $length sine 9k synth sine mix 10k $fade $gain
next_file 440hz+445hz
$input $output synth $length sine 440 synth sine mix 445 $fade $gain



echo; echo "Single, fixed frequency harmonic tones:"

for freq in 100 1k 5k; do
  for shape in square saw; do
    next_file ${freq}hz_$shape
    $input $output synth $length $shape $freq $fade gain -3
  done
done



echo; echo "Single, fixed frequency pure tones, 12dB attenuation steps:"

for freq in $freqs; do
  for att in 12 24 36 48 60 72 84 96 108; do
    next_file ${freq}hz_${att}dB_att
    $input $output synth $length sine $freq gain -h -$att $fade dither $vol_dither
  done
done



echo; echo "Sweep volume @ 1dB/s, -108->0->-108dB, with a mark (-40dB) every 12dB:"

for freq in $freqs; do
  next_file ${freq}hz_108dB-sweep 216
  $input $output synth $length sin $freq sin %30 \
             synth squ amod 0 100 sine amod 8.333333333333333333 0 75 \
             synth exp amod 0.00462962962962962963 0 0 50 54 \
             squ amod 0.08333333333333333333 0 1 1 \
             remix 1v.99,2v.01 dither $vol_dither
done



echo; echo "1kHz tone offset with 1Hz:"

next_file offset_10%_square
$input $output synth $length square 1 sine 1000 remix 1v0.05,2v0.95 gain -h 0 $fade $gain
next_file offset_50%_sine
$input $output synth $length sine 1 synth sine mix 1000 $fade $gain



echo; echo "Silence on one channel, full volume on the other:"

for freq in $freqs; do
  next_file ${freq}hz_left-chan
  $input $output synth $length sine $freq remix 1 0 gain -h 0 $fade $gain
  next_file ${freq}hz_right-chan
  $input $output synth $length sine $freq remix 0 1 gain -h 0 $fade $gain
done



echo; echo "Phase difference (24, 90, 180 degrees) between channels:"

for freq in $freqs; do
  for phase in 6.667 25 50; do
    next_file ${freq}hz_phase-$phase
    $input $output synth $length sine $freq sine $freq 0 $phase $fade $gain
  done
done



echo; echo "Plucked scale:"

options=
overdrive="gain -3"
for f in pluck pluck_dist; do
  next_file $f 42
  note=-29
  :>tmp.s32

  while [ $note -lt 17 ]; do
    $input -t s32 - synth .4 pluck %$note $options >> tmp.s32
    note=$(($note + 1))
  done
  $input -t s32 - synth 1.2 pluck %$note $options >> tmp.s32

  while [ $note -gt -29 ]; do
    $input -t s32 - synth .4 pluck %$note $options >> tmp.s32
    note=$(($note - 1))
  done
  $input -t s32 - synth pluck %$note $options fade t 0 4 3.6 >> tmp.s32

  $sox -r $rate -c 1 tmp.s32 $output $overdrive remix 1 1 reverb 30

  rm -f tmp.s32
  options="0 0 60 75 0"
  overdrive="overdrive gain -10"
done



plot  # the last file
minutes=$(($total_length / 60))
seconds=$(($total_length - $minutes * 60))
echo; echo "Sample-rate = $rate; # files created = $file_count; total audio length = `printf "%i:%02i" $minutes $seconds`"
