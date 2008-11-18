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

G0="pl %-26 pl %-19 pl %-14 pl %-10 pl %-7 pl %-2"
A0="pl %-24 pl %-17 pl %-12 pl %-8 pl %-5 pl %0"
C0="pl %-26 pl %-21 pl %-14 pl %-9 pl %-5 pl %-2"
D0="pl %-24 pl %-19 pl %-12 pl %-7 pl %-3 pl %0"
E0="pl %-22 pl %-17 pl %-10 pl %-5 pl %-1 pl %2"
Bb0="pl %-23 pl %-16 pl %-11 pl %-7 pl %-4 pl %1"

o="overdrive 40 gain -8"
e="delay 0 .02 .04 .06 .08 .1 remix - $o fade 0"
s="$sox -q -n -p synth 0 0 0 80 87"
l="$sox -q -n -p synth 0 0 0 80 0"

$play -m -v .8 \
"|$sox \
\"|$sox -n -p synth noise fade 0 .5 .48 trim 0 .15\" \
\"|$sox -n -p synth noise fade h 0 .26 .11 gain -35 lowpass -1 12k\" \
-p splice .15,.06,0 gain -14 lowpass -1 12k highpass -1 9k \
equalizer 14k 1.3 13 \
equalizer 9500 10 8 \
equalizer 7000 10 8 \
equalizer 5200 10 8 \
equalizer 3800 10 8 \
equalizer 1500 10 8 pad 0 .21 remix 1 1 reverb 20 repeat 59" \
"|$sox \
\"|$sox -n -p trim 0 1.4\" \
\"|$s $G0 $e 2.6 .1 bend .5,200,.2\" \
\"|$s $C0 $e 2 .1\" \
\"|$s $D0 $e 4 .1\" \
\"|$s $A0 $e 2 .1\" \
\"|$s $C0 $e 2 .1\" \
\"|$s $D0 $e 4 .1\" \
\"|$s $A0 $e 2 .1\" \
\"|$s $C0 $e 2 .1\" \
\"|$s $D0 $e 2 .1\" \
\"|$s $Bb0 $e 2 .1\" \
\"|$s $E0 $e 4 .1\" \
-p pad 0 3 remix 1 1 flanger reverb 70" \
"|$sox \
\"|$sox -n -p trim 0 8\" \
\"|$l pl %7 $o trim 0 .25\" \
\"|$l pl %12 $o trim 0 .2\" \
\"|$l pl %10 $o trim 0 .5 bend .2,-300,.1\" \
\"|$l pl %5 $o trim 0 .5 bend .2,-200,.1\" \
\"|$l pl %0 $o fade 0 .55 .1 bend .2,-200,.1\" \
\"|$l pl %12 $o trim 0 5.95\" \
\"|$l pl %12 $o trim 0 .5 bend .2,-200,.1\" \
\"|$l pl %12 $o trim 0 .5 bend .2,-200,.1\" \
\"|$l pl %12 $o trim 0 .5 bend .2,-200,.1\" \
\"|$l pl %12 $o fade 0 .6 .1 bend .2,-200,.1\" \
\"|$l pl %12 $o trim 0 1.95 bend .3,-100,1\" \
\"|$l pl %10 $o trim 0 2 bend .3,-50,1\" \
\"|$l pl %9 $o trim 0 2 gain -3 \" \
\"|$l pl %8 $o trim 0 2 gain -6 \" \
\"|$l pl %2 pl %7 delay 0 .02 remix - $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %2 pl %7 delay 0 .02 remix - $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %2 pl %7 delay 0 .02 remix - $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %2 pl %7 delay 0 .02 remix - $o fade h 0 6 6\" \
-p gain -4 remix 1 1 flanger"
