This directory includes hand-crafted project files for building SoX under
MSVC9. The project files may be replaced by expanding CMAKE support in the
future, but for now, this is the best build of SoX currently available on
Windows. The resulting sox.exe has support for all SoX features except magic,
ffmpeg, and pulseaudio. LAME (libmp3lame.dll or lame_enc.dll), MAD (libmad.dll
or cygmad-0.dll) and AMR support (libamrnb-3.dll, libamrwb-3.dll) are loaded
at runtime if they are available.

How to build:
1. Check out the SoX CVS code into a directory named sox.
2. Extract the source code for the other libraries next to the sox
directory. Remove the version numbers from the directory names.
-- flac-1.2.1.tar.gz extracted into directory flac
-- lame-398-2.tar.gz extracted into directory lame
-- libid3tag-0.15.1b.tar.gz extracted into directory libid3tag
-- libmad-0.15.0b.tar.gz extracted into directory libmad
-- libogg-1.1.4.tar.gz extracted into directory libogg
-- libpng-1.2.39-no-config.tar.gz extracted into directory libpng
-- libsndfile-1.0.20.tar.gz extracted into directory libsndfile
-- libvorbis-1.2.3.tar.gz extracted into directory libvorbis
-- wavpack-4.50.1.tar.gz extracted into directory wavpack
-- zlib-1.2.3.tar.gz extracted into directory zlib
3. Open the sox\msvc9\SoX.sln solution and build.
7. The resulting executable files will be in sox\msvc9\Debug or
sox\msvc9\Release. The resulting sox.exe will dynamically link to
libmp3lame.dll, libmad.dll, libamrnb-3.dll, and libamrwb-3.dll if they are
available, but will run without them (though the corresponding features will
be unavailable if they are not present).

Points to note:
- The included libsox project enables OpenMP support. You can disable this
in the libsox project properties under Configuration Properties, C/C++,
Language, OpenMP support. If you don't disable it, you will need
vcomp90.dll and Microsoft.VC90.OpenMP.manifest either installed on your
machine or copied into the directory next to sox.exe. If you have OpenMP
support in your copy of Visual Studio, these files can be found here:
c:\Program Files\Microsoft Visual Studio 9.0\vc\redist\x86\Microsoft.VC90.OPENMP.
Note that some editions of Visual Studio might not include OpenMP support.
- The included projects do not enable SSE2. You can enable this in the project
properties under Configuration Properties, C/C++, Code Generation, Enable
Enhanced Instruction Set. Note that some editions of Visual Studio might
not include Enhanced Instruction Set support.
- The included projects set the floating-point model to "fast". This means
that the compiler is free to optimize floating-point operations. For
example, the compiler might optimize the expression (14.0 * x / 7.0) into
(x * 2.0). In addition, the compiler is allowed to use 80-bit
floating-point registers to store temporary values (instead of rounding
each intermediate result to a 32-bit or 64-bit value. In some cases, these
optimizations can change the results of floating-point calculations. If you
run into trouble, you can change the settings to one of the other values.
"precise" avoids any optimization that might change the result but keeps
optimizations that might give more accurate results (such as using 80-bit
temporary register values). "strict" avoids any optimization that might
change the result and rounds every intermediate result to the requested
precision. You can change this setting in the project properties under
Configuration Properties, C/C++, Code Generation, Floating Point Model.

