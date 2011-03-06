This directory includes hand-crafted project files for building SoX under
MSVC9. The project files may be replaced by expanding CMAKE support in the
future, but for now, this is the easiest way to build SoX with MS Visual C++.
The resulting sox.exe has support for all SoX features except magic, ffmpeg,
and pulseaudio. LAME (libmp3lame.dll or lame_enc.dll), MAD (libmad.dll or
cygmad-0.dll), libsndfile (libsndfile-1.dll) and AMR support (libamrnb-3.dll,
libamrwb-3.dll) are loaded at runtime if they are available.

How to build:

1. Check out the SoX git code into a directory named sox.

2. Extract the source code for the other libraries next to the sox
   directory. Remove the version numbers from the directory names.
   The following versions were tested and successfully built:
   -- flac-1.2.1.tar.gz extracted into directory flac
   -- lame-398.4.tar.gz extracted into directory lame
   -- libid3tag-0.15.1b.tar.gz extracted into directory libid3tag
   -- libmad-0.15.1b.tar.gz extracted into directory libmad
   -- libogg-1.2.2.tar.gz extracted into directory libogg
   -- libpng-1.5.1.tar.gz extracted into directory libpng
   -- libsndfile-1.0.23.tar.gz extracted into directory libsndfile
   -- libvorbis-1.3.2.tar.gz extracted into directory libvorbis
   -- speex-1.2rc1.tar.gz extracted into directory speex
   -- wavpack-4.60.1.tar.bz2 extracted into directory wavpack
   -- zlib-1.2.5.tar.gz extracted into directory zlib

3. Open the sox\msvc9\SoX.sln solution.

4. If any of the above libraries are not available or not wanted, adjust the
   corresponding settings in the soxconfig.h file (in the LibSoX project inside
   the Config Files folder) and remove the corresponding project from the
   solution.

5. Build the solution.

6. The resulting executable files will be in sox\msvc9\Debug or
   sox\msvc9\Release. The resulting sox.exe will dynamically link to
   libmp3lame.dll, libmad.dll, libsndfile-1.dll, libamrnb-3.dll, and
   libamrwb-3.dll if they are available, but will run without them (though the
   corresponding features will be unavailable if they are not present).

Points to note:

- The libsndfile-1.0.20.tar.gz package does not include the sndfile.h header
  file. Normally, before compiling libsndfile, you would create sndfile.h
  (either by processing it via autoconf, by downloading a copy, or by renaming
  sndfile.h.in). However, this SoX solution includes its own version of
  sndfile.h, so you should not create a sndfile.h under the libsndfile folder.
  To repeat: you should extract a clean copy of libsndfile-1.0.20.tar.gz, and
  should not add, process, or rename any files.

- The solution includes an experimental effect called speexdsp that uses the
  speex DSP library. This does not yet enable any support for the speex file
  format or speex codec. The speexdsp effect is simply an experimental effect
  to make use of the automatic gain control and noise filtering components that
  are part of the speex codec package. Support for the speex codec may be added
  later.

- The included libsox project enables OpenMP support. You can disable this
  in the libsox project properties under Configuration Properties, C/C++,
  Language, OpenMP support. If you don't disable it, you will need
  vcomp90.dll and Microsoft.VC90.OpenMP.manifest either installed on your
  machine or copied into the directory next to sox.exe. If you have OpenMP
  support in your copy of Visual Studio, these files can be found here:

  c:\Program Files\Microsoft Visual Studio 9.0\
     vc\redist\x86\Microsoft.VC90.OPENMP

  Note that some editions of Visual Studio might not include OpenMP support.

- The included projects do not enable SSE2. You can enable this in the project
  properties under Configuration Properties, C/C++, Code Generation, Enable
  Enhanced Instruction Set. Note that some editions of Visual Studio might
  not include Enhanced Instruction Set support.

- The included projects set the floating-point model to "fast". This means
  that the compiler is free to optimize floating-point operations. For
  example, the compiler might optimize the expression (14.0 * x / 7.0) into
  (x * 2.0). In addition, the compiler is allowed to leave expression results
  in floating-point registers to store temporary values instead of rounding
  each intermediate result to a 32-bit or 64-bit value. In some cases, these
  optimizations can change the results of floating-point calculations. If you
  need more precise results, you can change this optimization setting can be
  changed to one of the other values. The "precise" setting avoids any
  optimization that might change the result (preserves the order of all
  operations) but keeps optimizations that might give more accurate results
  (such as using more precision than necessary for intermediate values if doing
  so results in faster code). The "strict" setting avoids any optimization that
  might change the result in any way contrary to the C/C++ standard and rounds
  every intermediate result to the requested precision according to standard
  floating-point rounding rules. You can change this setting in the project
  properties under Configuration Properties, C/C++, Code Generation, Floating
  Point Model.
