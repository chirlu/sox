@echo off

rem     Test script for sox under DOS derived from tests.sh. This should
rem     run without core-dumping or printing any error messages.

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
dir ub.raw
dir ub2.raw
echo.
echo The two filesizes above should be the same.
pause
echo.
echo.

echo Skip checksum and rate byte. DOS isn't good at this, so just use a
echo rough test.

echo.
dir %file%.voc
dir ub2.voc
echo.
echo The two filesizes above should be the same.
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
dir ub2.sf
dir ub3.sf
echo.
echo The two filesizes above should be the same.
pause
cls

del ub2.sf
del ub2.aif
del ub3.sf

set file=
set noise=
