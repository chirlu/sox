#!/bin/sh

# Thanks to Reynir H. Stefánsson for the original version of this script.

# In marine radio, a Mayday emergency call is transmitted preceded by a
# 30-second alert sound.  The alert sound comprises two audio tones at
# 1300Hz and 2100Hz alternating at a rate of 4Hz.  This script shows how SoX
# can be used to construct an audio file containing the alert sound.

# This is a `Bourne shell' (sh) script but it should be simple to translate
# this to another scripting language if you do not have access to sh.

# If you run this script, you may want to hit Ctrl-C fairly soon after the
# alert tone starts playing---it's not a pleasant sound!

# The synth effect is used to generate each of the tones; "-e mu-law -r 8000"
# selects u-law 8kHz sampling-rate audio (i.e. relatively low fidelity,
# suitable for the marine radio transmission channel); each tone is
# generated at a length of 0.25 seconds to give the required 4Hz
# alternation.

# Note the use of `raw' as the intermediary file format; a self-describing
# (header) format would just get in the way here.  The self-describing
# header is added only at the final stage.

# ---------------------------------------------------------------------------

SOX=../src/sox

rm -f 2tones.ul    # Make sure we append to a file that's initially empty

for freq in 1300 2100; do
  $SOX -e mu-law -r 8000 -n -t raw - synth 0.25 sine $freq gain -3 >> 2tones.ul
done

$SOX -c 1 -r 8000 2tones.ul alert.au repeat - trim 0 30
                   # Repeat to fill 30 seconds and add a file header
rm 2tones.ul       # Tidy up intermediate file

$SOX alert.au -d
