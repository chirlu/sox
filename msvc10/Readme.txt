This directory includes hand-crafted project files for building SoX using the
Microsoft Visual C++ 10.0 (or later) compilers, available through Visual Studio
2010, Visual Studio 2012, or by downloading the freely-available Microsoft
Windows SDK 7.1. This is the easiest way to build SoX with MS Visual C++.
The resulting sox.exe has support for all SoX features except magic, ffmpeg,
ladspa, and pulseaudio. LAME (libmp3lame.dll or lame_enc.dll), MAD (libmad.dll
or cygmad-0.dll), libsndfile (libsndfile-1.dll), and AMR support
(libamrnb-3.dll, libamrwb-3.dll) are loaded at runtime if they are available.
OpenMP support is available only if using Visual Studio Professional or
higher - it is not available if you build SoX via Visual Studio 2010 Express or
via the Microsoft Windows SDK.

How to build:

1. If you don't already have it, install .NET 4.0 (required for msbuild).

   If you don't already have Visual Studio 2010 (or later) or the Windows SDK
   7.1 (or later) installed, download and install the Windows SDK 7.1 from
   Microsoft:

   http://www.microsoft.com/downloads/en/details.aspx?FamilyID=6b6c21d2-2006-4afa-9702-529fa782d63b&displaylang=en

   When installing the Windows SDK, include at least the following features:
   * Windows Headers and Libraries - Windows Headers
   * Windows Headers and Libraries - x86 Libraries
   * Windows Native Code Development - Tools
   * Windows Native Code Development - Visual C++ Compilers

2. Put the SoX code into a directory named sox.

   Extract the source code for the other libraries next to the sox
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

3. If using Visual Studio, open the sox\msvc10\SoX.sln solution in Visual
   Studio.

   If using the Windows SDK, open a normal command prompt, then run:
"c:\Program Files\Microsoft SDKs\Windows\v7.1\bin\SetEnv.cmd" /x86 /Release /xp
   then CD to the sox\msvc10 folder.

4. If any of the above libraries are not available or not wanted, adjust the
   corresponding HAVE_* settings in the soxconfig.h file and remove the
   corresponding project from the SoX.sln solution.

   If using Visual Studio, you will find the soxconfig.h file in the LibSox
   project's Config Files folder.

   If using the Windows SDK, you'll have to use a text editor (i.e. notepad) to
   edit the soxconfig.h file (sox\msvc10\sox\soxconfig.h), and you'll have to
   manually remove the entries for the unwanted projects.

5. If using Visual Studio Professional or above and you want OpenMP support,
   enable it in the project settings for the LibSox and SoX projects
   (Configuration Properties, C/C++, Language, Open MP Support, set to Yes).

6. If using Visual Studio, build the solution using the GUI.

   If using the Windows SDK, run: msbuild SoX.sln

7. The resulting executable files will be in sox\msvc10\Debug or
   sox\msvc10\Release. The resulting sox.exe will dynamically link to
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

- If you enable Open MP support, you will need vcomp100.dll either installed on
  your machine or copied into the directory next to sox.exe. If you have Open
  MP support in your copy of Visual Studio, this file can be found here:

  c:\Program Files\Microsoft Visual Studio 10.0\
     vc\redist\x86\Microsoft.VC100.OPENMP

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
  need more precise results, you can change this optimization setting to one of
  the other values. The "precise" setting avoids any optimization that might
  possibly produce less-accurate results (it preserves the order of all
  operations) but keeps optimizations that might give unexpectedly-accurate
  results (for example, it might keep a temporary result in a double-precision
  register instead of rounding it to single-precision). The "strict" setting
  avoids any optimization that might change the result in any way contrary to
  the C/C++ standard and rounds every intermediate result to the requested
  precision according to standard floating-point rounding rules. You can change
  this setting in the project properties under Configuration Properties, C/C++,
  Code Generation, Floating Point Model.
