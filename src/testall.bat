@echo off

rem     First create a working copy of t.bat. Note optional cls and pause.

echo @echo off >t.bat
echo set format=%%1 >>t.bat
echo shift >>t.bat
echo set opts=%%1 %%2 %%3 %%4 %%5 %%6 %%7 %%8 %%9 >>t.bat
echo. >>t.bat
echo cls >>t.bat
echo echo Format: %%format%%   Options: %%opts%% >>t.bat
echo echo on >>t.bat
echo .\sox monkey.wav %%opts%% %%tmp%%\monkey.%%format%% %%effect%% >>t.bat
echo .\sox %%opts%% %%tmp%%\monkey.%%format%% %%tmp%%\monkey1.wav %%effect%% >>t.bat
echo @echo off >>t.bat
echo echo. >>t.bat
echo set format=>>t.bat
echo set opts=>>t.bat
echo pause >>t.bat

rem     Now set up any global effects and call the batch file. Note that
rem     this needs extra work to cope with DOS's limitation of 3-character
rem     extensions on the filename.

set effect=%1 %2 %3 %4 %5 %6 %7 %8 %9

call t.bat 8svx
call t.bat aiff
call t.bat aifc
call t.bat au
call t.bat avr -e unsigned-integer
call t.bat cdr
call t.bat cvs
call t.bat dat
call t.bat vms
call t.bat hcom -r 22050
call t.bat maud
call t.bat raw -r 8130 -t ub
call t.bat sf
call t.bat smp
call t.bat sndt
call t.bat txw
call t.bat voc
call t.bat vox -r 8130
call t.bat wav
call t.bat wve

del t.bat
