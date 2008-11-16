#!/bin/sh

# SoX script: synth.sh               (c) 2008 robs@users.sourceforge.net
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

sox=../src/sox
play=../src/play

A0="pl %-29 pl %-24 pl %-17 pl %-12 pl %-8 pl %-5"
B0="pl %-27 pl %-22 pl %-15 pl %-10 pl %-6 pl %-3"
A1="pl %-17 pl %-12 pl %-9 pl %-5 pl %0 pl %4"
B1="pl %-15 pl %-10 pl %-7 pl %-3 pl %2 pl %6"

e="delay 0 .02 .04 .06 .08 .1 remix - overdrive 40 gain -11 fade 0"
s="$sox -q -n -p synth 0 0 0 80 87"

$play -m -v .8 \
"|$sox \
\"|$sox -n -p synth noise fade 0 .5 .48 trim 0 .15\" \
\"|$sox -n -p synth noise fade h 0 .26 .11 gain -35 lowpass -1 12k\" \
-p splice .15,.06,0 gain -15 lowpass -1 12k highpass -1 9k \
equalizer 14k 1.3 13 \
equalizer 9500 10 8 \
equalizer 7000 10 8 \
equalizer 5200 10 8 \
equalizer 3800 10 8 \
equalizer 1500 10 8 pad 0 .21 remix 1 1 reverb 20 repeat 72" \
"|$sox \
\"|$sox -n -p trim 0 1.4\" \
\"|$s $A0 $e 4 .1 bend .4,+200,.3\" \
\"|$s $A0 $e 4 .1 bend .4,+200,.3\" \
\"|$s $B0 $e 4 .1 bend .4,-200,.3\" \
\"|$s $A0 $e 4 .1 bend .4,-300,.3\" \
\"|$s $A1 $e 4 .1 bend .4,+200,.3\" \
\"|$s $A1 $e 4 .1 bend .4,+200,.3\" \
\"|$s $B1 $e 4 .1 bend .4,-200,.3\" \
\"|$s $A1 $e 4 .1 bend .4,-300,.3\" \
\"|$s $A1 $e 3.05 .1 bend .4,+200,.3\" \
\"|$s $A1 $e .5 .2 \" \
\"|$s $B1 $e .5 .2 \" \
-p pad 0 3 remix 1 1 flanger reverb 70"
