#!/bin/sh
#
# SOX Test script.
#
# This script is just a quick sanity check of SOX on lossless conversions.

# verbose options
#noise=-V

./sox $noise monkey.au raw1.ub

# Convert between unsigned bytes and signed bytes
./sox $noise -r 8012 -c 1 raw1.ub raw1.sb
./sox $noise -r 8012 -c 1 raw1.sb raw2.ub
if cmp -s raw1.ub raw2.ub
then
    echo "Conversion between unsigned bytes and signed bytes was successful"
else
    echo "Error converting between signed and unsigned bytes"
fi
rm -f raw1.sb raw2.ub

./sox $noise -r 8012 -c 1 raw1.ub raw1.sw
./sox $noise -r 8012 -c 1 raw1.sw raw2.ub
if cmp -s raw1.ub raw2.ub
then
    echo "Conversion between unsigned bytes and signed words was successful"
else
    echo "Error converting between signed words and unsigned bytes"
fi
rm -f raw1.sw raw2.ub

./sox $noise -r 8012 -c 1 raw1.ub raw1.al
./sox $noise -r 8012 -c 1 raw1.al raw2.ub
if cmp -s raw1.ub raw2.ub
then
    echo "Conversion between unsigned bytes and alaw bytes was successful"
else
    echo "Error converting between alaw and unsigned bytes"
fi
rm -f raw1.al raw2.ub


./sox $noise -r 8012 -c 1 raw1.ub raw1.uw
./sox $noise -r 8012 -c 1 raw1.uw raw2.ub
if cmp -s raw1.ub raw2.ub
then
    echo "Conversion between unsigned bytes and unsigned words was successful"
else
    echo "Error converting between unsigned words and unsigned bytes"
fi
rm -f raw1.uw raw2.ub

./sox $noise -r 8012 -c 1 raw1.ub raw1.sl
./sox $noise -r 8012 -c 1 raw1.sl raw2.ub
if cmp -s raw1.ub raw2.ub
then
    echo "Conversion between unsigned bytes and signed long was successful"
else
    echo "Error converting between signed long and unsigned bytes"
fi
rm -f raw1.sl raw2.ub

./sox $noise -r 8012 -c 1 raw1.ub -f -l raw1.raw
./sox $noise -r 8012 -c 1 -f -l raw1.raw raw2.ub
if cmp -s raw1.ub raw2.ub
then
    echo "Conversion between unsigned bytes and float was successful"
else
    echo "Error converting between float and unsigned bytes"
fi
rm -f raw1.raw raw2.ub

rm -f raw1.ub
./sox $noise monkey.au raw1.sw

./sox $noise -r 8012 -c 1 raw1.sw raw1.ul
./sox $noise -r 8012 -c 1 raw1.ul raw2.sw
if cmp -s raw1.sw raw2.sw
then
    echo "Conversion between signed words and ulaw bytes was successful"
else
    echo "Error converting between ulaw and signed words"
fi
rm -f raw1.ul raw2.sw

rm -f raw1.sw

./sox $noise monkey.au -u -b monkey1.wav

echo ""

ext=8svx
./sox $noise monkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s monkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext monkey2.wav

ext=aiff
./sox $noise monkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s monkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext monkey2.wav

# AU doesn't support unsigned so use signed
ext=au
./sox $noise monkey1.wav -s -b convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s monkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext monkey2.wav

ext=avr
./sox $noise monkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s monkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext monkey2.wav

ext=dat
./sox $noise monkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s monkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext monkey2.wav

ext=hcom
# HCOM has to be at specific sample rate.
./sox $noise -r 5512 monkey1.wav nmonkey1.wav
./sox $noise nmonkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s nmonkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext nmonkey1.wav monkey2.wav

ext=maud
./sox $noise monkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s monkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext monkey2.wav

ext=sf
./sox $noise monkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s monkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext monkey2.wav

ext=smp
./sox $noise monkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s monkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext monkey2.wav

ext=voc
./sox $noise -r 8000 monkey1.wav nmonkey1.wav
./sox $noise nmonkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s nmonkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext nmonkey1.wav monkey2.wav

ext=wve
./sox $noise -r 8000 monkey1.wav nmonkey1.wav
./sox $noise nmonkey1.wav convert.$ext
./sox $noise convert.$ext -u -b monkey2.wav
if cmp -s nmonkey1.wav monkey2.wav
then
    echo "Conversion between wav and $ext was successful"
else
    echo "Error converting between wav and $ext."
fi
rm -f convert.$ext nmonkey1.wav monkey2.wav

exit
