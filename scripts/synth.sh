#!/bin/sh

# SoX script: synth.sh               (c) 2008-9 robs@users.sourceforge.net
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

# Demonstrates the use of some of the features new in SoX 14.3.0, viz
# nested SoX commands, the synth `pluck' type, and the overdrive
# effect (also used are several other effects).
# Music (c) 2008 robs@users.sourceforge.net.  All rights reserved.

sox=../src/sox

G0="pl %-26 pl %-19 pl %-14 pl %-10 pl %-7 pl %-2"
A0="pl %-24 pl %-17 pl %-12 pl %-8 pl %-5 pl %0"
C0="pl %-26 pl %-21 pl %-14 pl %-9 pl %-5 pl %-2"
D0="pl %-24 pl %-19 pl %-12 pl %-7 pl %-3 pl %0"
E0="pl %-22 pl %-17 pl %-10 pl %-5 pl %-1 pl %2"
Bb0="pl %-23 pl %-16 pl %-11 pl %-7 pl %-4 pl %1"

o="overdrive 27 gain -11"
e="delay 0 .02 .04 .06 .08 .1 remix - overdrive 33 gain -8 fade 0"
s="$sox -q -n -p synth 0 0 1 60 90"
l="$sox -q -n -p synth 0 0 0 50 20"

b="$sox -n -p synth 0 0 0 30 20 pl"
c3="fade h 0 .75"
c="fade h 0 .25"
cs="fade h 0 .25 .05"
m="fade h 0 .5"
sb="fade h 0 1"
r="$sox -n -p trim 0 .25"
r2="$sox -n -p trim 0 .5"
r3="$sox -n -p trim 0 .75"

$sox -m -v .8 \
"|$sox \
\"|$sox -n -p synth noise fade 0 .5 .48 trim 0 .15\" \
\"|$sox -n -p synth noise fade h 0 .26 .11 gain -35 lowpass -1 12k\" \
-p splice .15,.06,0 gain -14 lowpass -1 12k highpass -1 9k \
equalizer 14k 1.3 13 \
equalizer 9500 10 8 \
equalizer 7000 10 8 \
equalizer 5200 10 8 \
equalizer 3800 10 8 \
equalizer 1500 10 8 pad 0 .21 remix 1 1 reverb 10 repeat 56" \
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
\"|$l pl %0 $o fade h 0 2 .5\" \
\"|$sox -n -p trim 0 3.95\" \
\"|$l pl %12 $o trim 0 .5 bend .2,-200,.1\" \
\"|$l pl %12 $o trim 0 .5 bend .2,-200,.1\" \
\"|$l pl %12 $o fade 0 .8 .1 bend .2,-200,.1\" \
\"|$l pl %12 $o trim 0 .3 bend .1,-200,.1\" \
\"|$l pl %12 $o fade 0 1.95 .6 bend .0,-50,1.75 gain 3\" \
\"|$l pl %10 $o fade 0 2 .6 bend .0,-50,1.9\" \
\"|$l pl %9 $o trim 0 2 gain -1\" \
\"|$l pl %8 $o fade h 0 1 .3\" \
\"|$l pl %8 $o fade h 0 1 .1 gain 1.5\" \
\"|$l pl %2 pl %7 delay 0 .02 remix - $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %2 pl %7 delay 0 .02 remix - $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %2 pl %7 delay 0 .02 remix - $o trim 0 .25\" \
\"|$l pl %-5 $o trim 0 .25\" \
\"|$l pl %2 pl %7 delay 0 .02 remix - $o fade h 0 6 6\" \
-p gain -4 remix 1 1 flanger" \
"|$sox \
\"|$sox -n -p trim 0 1.5\" \
\"|$b G1 $m contrast\" \
\"|$b A1 $c3 contrast\" \
\"|$b G#1 $c\" \
\"|$b A1 $c3\" \
\"|$r\" \
\
\"|$b C2 $cs\" \
\"|$b C2 $cs\" \
\"|$r\" \
\"|$b B1 $cs\" \
\"|$b C2 $c3\" \
\"|$r\" \
\
\"|$b D2 $cs\" \
\"|$b D2 $cs\" \
\"|$r\" \
\"|$b C#2 $cs\" \
\"|$b D2 $c\" \
\"|$b C#2 $m\" \
\"|$r\" \
\
\"|$b D2 $cs\" \
\"|$b D2 $cs\" \
\"|$r\" \
\"|$b C#2 $cs\" \
\"|$b D2 $c\" \
\"|$b C#2 $c\" \
\"|$b D2 $c\" \
\"|$b E2 $c\" \
\
\"|$b A1 $c3 contrast\" \
\"|$b B1 $c\" \
\"|$b A1 $c\" \
\"|$b G#1 $c\" \
\"|$b A1 $c\" \
\"|$b B1 $c\" \
\
\"|$b C2 $c3\" \
\"|$b B1 $c\" \
\"|$b C2 $c\" \
\"|$b D2 $c\" \
\"|$b C2 $c\" \
\"|$b B1 $c\" \
\
\"|$b D2 $c3\" \
\"|$b E2 $c\" \
\"|$b D2 $c\" \
\"|$b C#2 $c\" \
\"|$b D2 $c\" \
\"|$b E2 $c\" \
\
\"|$b D2 $c3\" \
\"|$b C#2 $c\" \
\"|$b D2 $c\" \
\"|$b E2 $c\" \
\"|$b D2 $c\" \
\"|$b C#2 $c\" \
\
\"|$b A1 $cs\" \
\"|$b A1 $cs\" \
\"|$r\" \
\"|$b G#1 $c\" \
\"|$b A1 $c3\" \
\"|$b B1 $c\" \
\
\"|$b C2 $c\" \
\"|$b B1 $c\" \
\"|$b C2 $c\" \
\"|$b D2 $c\" \
\"|$b C2 $c3\" \
\"|$r\" \
\
\"|$b D2 $m\" \
\"|$b E2 $m\" \
\"|$b D2 $c\" \
\"|$b C#2 $c\" \
\"|$b D2 $c\" \
\"|$b E2 $c\" \
\
\"|$b F2 $m\" \
\"|$b Bb1 $m\" \
\"|$b C2 $c\" \
\"|$b F2 $c\" \
\"|$b D2 $c\" \
\"|$b Bb1 $c\" \
\
\"|$b E1 $m\" \
\"|$b E2 $m\" \
\"|$b D2 $c\" \
\"|$b E2 $m\" \
\"|$b B1 $c\" \
\
\"|$b E1 $sb\" \
-p lowpass -1 1k remix 1p-12 1p-12" -d
