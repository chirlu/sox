SoX
---

This file contains information specific to the Win32 version of SoX.
Please refer to the README file for general information.

The binary SOX.EXE can be installed anywhere you desire.  The only
restriction is that the included CYGWIN1.DLL and CYGGOMP-1.DLL must be 
located in the same directory as SOX.EXE or somewhere within your PATH.

SoX Helper Applications
-----------------------

SoX also includes support for SOXI.EXE, PLAY.EXE and REC.EXE and their
behaviors are documented in included PDF's.  They have the same
install restrictions as SOX.EXE.

SOXI.EXE, PLAY.EXE, and REC.EXE are not distributed with this package to 
reduce size requirements. They are, in fact, only copies of the original
SOX.EXE binary which changes SOX.EXE's behavior based on the
executable's filename.

If you wish to make use of these utils then you can create them
yourself.

copy sox.exe soxi.exe
copy sox.exe play.exe
copy sox.exe rec.exe

If you are concerned with disk space, the play and record
functionality can be equated using the "-d" option with SOX.EXE.  soxi
functionality can be equated using the "--info" option with SOX.EXE. The
rough syntax is:

play: sox [input files and options] -d [effects]
rec: sox -d [output file and options] [effects]
soxi: sox --info [input files and options]

wget
----

SoX can make use of the wget command line utility to load files over
the internet.  A binary copy of wget has been included with this
package of SoX for your convience.

For SoX to make use of wget, it must be located either in your PATH or
within the same directory that SoX is ran from.

Custom configuration of wget can be made by editing the file wget.ini
contained in the same directory as wget.exe.

Please consult wget's homepage for access to source code as well as
further instructions on configuring.

http://www.gnu.org/software/wget

Acknowledgements
----------------

SOX.EXE included in this package makes use of the following projects.
See the cygbuild script included with the source code package for
further information on how it was compiled and packaged.

  SoX - http://sox.sourceforge.net

  Ogg Vorbis - http://www.vorbis.com

  FLAC - http://flac.sourceforge.net

  libsndfile - http://www.mega-nerd.com/libsndfile

  WavPack - http://www.wavpack.com

  PNG - http://www.libpng.org/pub/png

Enjoy,
The SoX Development Team

Appendix - MP3 Support
----------------------

SoX contains support for reading and writing MP3 files but does not ship
with the DLL's that perform decoding and encoding of MP3 data because
of patent restrictions.  For further details, refer to:

http://en.wikipedia.org/wiki/MP3#Licensing_and_patent_issues

MP3 support can be enabled by placing Lame encoding DLL and/or
MAD decoding DLL into the same directory as SOX.EXE.  These
can be compiled yourself, may turn up on searches of the internet
or may be included with other MP3 applications already installed
on your system. Try searching for cygmp3lame-0.dll, libmp3lame.dll, 
cygmad-0.dll, and libmad.dll.

Instructions are included here for using Cygwin to create the DLL's. 
It is assumed you already have cygwin installed on your system
with its optional gcc compiler installed.  The commands are ran from
bash shell cygwin installs.

Obtain the latest Lame and MAD source code from approprate locations.

Lame MP3 encoder  http://lame.sourceforge.net
MAD MP3 decoder   http://www.underbit.com/products/mad 

cd lame-398-2
./configure
make
cp libmp3lame/.libs/cygmp3lame-0.dll /path/to/sox/libmp3lame.dll

MAD library up to 0.15.1b has a bug in configure that will not allow
building DLL under cygwin.  So an additional command is needed to
work around this.

cd libmad-0.15.1b
./configure
make
gcc -shared -Wl,-export-all -Wl,--out-implib=libmad.dll.a -o cygmad-0.dll version.o fixed.o bit.o timer.o stream.o frame.o synth.o decoder.o layer12.o layer3.o huffman.o
cp cygmad-0.dll /path/to/sox/libmad.dll

Alternatively, the MAD configure script can be patched to allow building and
installing libmad under standard cygwin directories.

--- ../libmad-0.15.1b.orig/configure.ac	2004-01-23 03:41:32.000000000 -0600
+++ configure.ac	2009-05-25 22:44:50.846400000 -0500
@@ -51,17 +51,17 @@
 	    esac
     esac
 
-dnl    case "$host" in
-dnl	*-*-cygwin* | *-*-mingw*)
-dnl	    LDFLAGS="$LDFLAGS -no-undefined -mdll"
-dnl	    ;;
-dnl    esac
+    case "$host" in
+	*-*-cygwin* | *-*-mingw*)
+	    LDFLAGS="$LDFLAGS -no-undefined"
+	    ;;
+    esac
 fi
 
 dnl Support for libtool.
 
 dnl AC_DISABLE_SHARED
-dnl AC_LIBTOOL_WIN32_DLL
+AC_LIBTOOL_WIN32_DLL
 AC_PROG_LIBTOOL
 
 AC_SUBST(LIBTOOL_DEPS)
--- end diff

cd libmad-0.15.1b
patch -p0 < configure.ac.diff
touch NEWS AUTHORS ChangeLog
autoreconf -i -f
./configure
make
cp .libs/cymad-0.dll /path/to/sox/libmad.dll

It may also be possible to create DLL's using MS VC++ since both
libraries include VC++ project files.  Lame creates a library
call lame_enc.dll which does not correctly export VBR symbols
and MAD does not export any symbols.  Therefore, cygwin is simpliest
and is suggested method.
