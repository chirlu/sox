rem Example of how to do batch processing with SoX on MS-Windows.
rem
rem Place this file in the same folder as sox.exe (& rename it as appropriate).
rem You can then drag and drop a selection of files onto the batch file (or
rem onto a `short-cut' to it).
rem
rem In this example, the converted files end up in a folder called `converted',
rem but this, of course, can be changed, as can the parameters to the sox
rem command.

cd %~dp0
mkdir converted
FOR %%A IN (%*) DO sox %%A "converted/%%~nxA" rate -v 44100
pause
