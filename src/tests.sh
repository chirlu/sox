#!/bin/sh
#
# SOX Test script.  This should run without core-dumping or printing any
# messages from the compare program.
#
# This script is manual just quick sanity check of SOX.

file=monkey

# verbose options
#noise=-V

rm -f out.raw out2.raw in.raw 
./sox $noise $file.voc ub.raw 
./sox $noise -t raw -r 8196 -u -b -c 1 ub.raw -r 8196 -s -b sb.raw
./sox $noise -t raw -r 8196 -s -b -c 1 sb.raw -r 8196 -u -b ub2.raw
./sox $noise -r 8196 -u -b -c 1 ub2.raw -r 8196 ub2.voc 
echo Comparing ub.raw to ub2.raw
cmp -l ub.raw ub2.raw
# skip checksum and rate byte
echo Comparing $file.voc to ub2.voc, ignoring Comment field
cmp -l $file.voc ub2.voc | grep -v '^    2[3456]' | grep -v '^    31'
rm -f ub.raw sb.raw ub2.raw ub2.voc
./sox $noise $file.au -u -r 8192 -u -b ub.raw
./sox $noise -r 8192 -u -b ub.raw -U -b ub.au 
./sox $noise ub.au -u ub2.raw 
./sox $noise ub.au -w ub2.sf
rm -f ub.raw ub.au ub2.raw ub.sf 
./sox $noise ub2.sf ub2.aiff
./sox $noise ub2.aiff ub3.sf
echo Comparing ub2.sf to ub3.sf, ignoring Comment field
cmp -l ub2.sf ub3.sf | grep -v '^    2[3456789]'
#rm -f ub2.sf ub2.aiff ub3.sf
#
# Cmp -l of stop.raw and stop2.raw will show that most of the 
# bytes are 1 apart.  This is quantization error.
#
# rm -f stop.raw stop2.raw stop2.au
# Bytes 23 - 26 are the revision level of VOC file utilities and checksum.
# We may use different ones than Sound Blaster utilities do.
# We use 0/1 for the major/minor, SB uses that on the 1.15 utility disk.
