@echo off

rem	Test script for sox under DOS derived from tests.sh. This should
rem	run without core-dumping or printing any error messages.

set file=monkey

rem verbose options

rem set noise=-V

del out.raw
del out2.raw
del in.raw
cls

echo on
.\sox %noise% %file%.voc ub.raw
.\sox %noise% -t raw -r 8196 -u -b -c 1 ub.raw -r 8196 -s -b sb.raw
.\sox %noise% -t raw -r 8196 -s -b -c 1 sb.raw -r 8196 -u -b ub2.raw
.\sox %noise% -r 8196 -u -b -c 1 ub2.raw -r 8196 ub2.voc
@echo off

echo.
xdir ub.raw /c/b
xdir ub2.raw /c/b
echo.
echo The two checksums above should be the same.
pause
echo.
echo.

echo Skip checksum and rate byte. DOS isn't good at this, so just use a
echo rough test.

echo.
xdir %file%.voc /c/b
xdir ub2.voc /c/b
echo.
echo The two lengths above should be the same, if the checksums differ
echo investigate further skipping the internal checksum and rate bytes.
pause
cls

del ub.raw
del sb.raw
del ub2.raw
del ub2.voc

echo on
.\sox %noise% %file%.au -u -r 8192 -u -b ub.raw
.\sox %noise% -r 8192 -u -b ub.raw -U -b ub.au
.\sox %noise% ub.au -u ub2.raw
.\sox %noise% ub.au -w ub2.sf
@echo off

del ub.raw
del ub.au
del ub2.raw
rem del ub.sf

echo on
.\sox %noise% ub2.sf ub2.aif
.\sox %noise% ub2.aif ub3.sf
@echo off

echo Skip comment field containing different filenames. Again, DOS sucks.

echo.
xdir ub2.sf /c/b
xdir ub3.sf /c/b
echo.
echo The two lengths above should be the same, if the checksums differ
echo investigate further skipping the internal filename comments.
pause
cls

del ub2.sf
del ub2.aif
del ub3.sf

rem Cmp -l of stop.raw and stop2.raw will show that most of the
rem bytes are 1 apart.  This is quantization error.
rem
rem rm -f stop.raw stop2.raw stop2.au
rem Bytes 23 - 26 are the revision level of VOC file utilities and checksum.
rem We may use different ones than Sound Blaster utilities do.
rem We use 0/1 for the major/minor, SB uses that on the 1.15 utility disk.

set file=
set noise=
