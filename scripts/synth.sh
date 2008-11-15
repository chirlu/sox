#!/bin/sh

sox=../src/sox
play=../src/play

A0="pl %-29 pl %-24 pl %-17 pl %-12 pl %-8 pl %-5"
B0="pl %-27 pl %-22 pl %-15 pl %-10 pl %-6 pl %-3"
A1="pl %-17 pl %-12 pl %-9 pl %-5 pl %0 pl %4"
B1="pl %-15 pl %-10 pl %-7 pl %-3 pl %2 pl %6"

e="delay 0 .02 .04 .06 .08 .1 remix - overdrive 40 gain -10 fade 0"
s="$sox -q -n -p synth 0 0 0 80 87"

$play -m "|$sox -c 2 -n -p synth tpdf fade 0 .4 .4 pad .1 gain -9 highpass 1k reverb 20 repeat 72" \
"|$sox \
\"|$sox -n -p trim 0 1.5\" \
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
