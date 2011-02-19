SoX
---

This file contains information specific to the Win32 version of SoX.
Please refer to the README file for general information.

The binary SOX.EXE can be installed anywhere you desire.  The only
restriction is that the included ZLIB1..DLL and LIBGOMP-1.DLL must be
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

Acknowledgements
----------------

SOX.EXE included in this package makes use of the following projects.
See the cygbuild script included with the source code package for
further information on how it was compiled and packaged.

  SoX - http://sox.sourceforge.net

  FLAC - http://flac.sourceforge.net

  LADSPA - http://www.ladspa.org

  libid3tag - http://www.underbit.com/products/mad

  libsndfile - http://www.mega-nerd.com/libsndfile

  Ogg Vorbis - http://www.vorbis.com

  PNG - http://www.libpng.org/pub/png

  WavPack - http://www.wavpack.com

  wget - http://www.gnu.org/software/wget

Enjoy,
The SoX Development Team

Appendix - wget Support
-----------------------

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

Appendix - MP3 Support
----------------------

SoX contains support for reading and writing MP3 files but does not ship
with the DLL's that perform decoding and encoding of MP3 data because
of patent restrictions.  For further details, refer to:

http://en.wikipedia.org/wiki/MP3#Licensing_and_patent_issues

MP3 support can be enabled by placing Lame encoding DLL and/or
MAD decoding DLL into the same directory as SOX.EXE.  These
can be compiled yourself, they may turn up on searches of the internet
or may be included with other MP3 applications already installed
on your system. For encoding/writing, try searching for lame-enc.dll,
libmp3lame-0.dll, libmp3lame.dll, or cygmp3lame-0.dll.  For
decoding/reading, try searching for libmad-0.dll, libmad.dll or cygmad-0.dll.

Instructions are included here for using MSYS to create the DLL's.
It is assumed you already have MSYS installed on your system
with a working gcc compiler.  The commands are ran from MSYS
bash shell.

Obtain the latest Lame and MAD source code from approprate locations.

Lame MP3 encoder  http://lame.sourceforge.net
MAD MP3 decoder   http://www.underbit.com/products/mad

cd lame-398-2
./configure --disabled-static --enable-shared
make
cp libmp3lame/.libs/libmp3lame-0.dll /path/to/sox

MAD libraries up to 0.15.1b have a bug in configure that will not allow
building DLL under mingw.  This can be resolved by adding LDFLAGS
to configure and editing the generated Makefile to remove an invalid
option.

cd libmad-0.15.1b
./configure --enable-shared --disable-static LDFLAGS="-no-undefined"
[edit Makefile, search for "-fforce-mem" and delete it.]
make
cp libmad-0.dll /path/to/sox/

Appendix - AMR-NB/AMR-WB Support
--------------------------------

SoX contains support for reading and writing AMR-NB and AMR-WB files but
does not ship with the DLL's that perform decoding and encoding of AMR
data because of patent restrictions.

AMR-NB/AMR-WB support can be enabled by placing required DLL's
into the same directory as SOX.EXE.  These can be compiled yourself,
they may turn up on searches of the internet or may be included with other
MP3 applications already installed on your system. For AMR-NB support,
try searching for libamrnb-3.dll, libopencore-amrnb-0.dll, or
libopencore-amrnb.dll. For AMR-WB support, try searching for libamrwb-3.dll,
libopencore-amrwb-0.dll, or libopencore-amrwb.dll.

Instructions are included here for using MSYS to create the DLL's.
It is assumed you already have MSYS installed on your system with
working gcc compiler.  These commands are ran from MSYS bash shell.

Obtain the latest amrnb and amrwb source code from
http://sourceforge.net/projects/opencore-amr .

cd opencore-amr-0.1.2
./configure --enable-shared --disable-static LDFLAGS="-no-undefined"
make
cp amrnb/.libs/libopencore-amrnb-0.dll /path/to/sox
cp amrwb/.libs/libopencore-amrwb-0.dll /path/to/sox

Appendix - LADSPA Plugins
-------------------------

SoX has built in support for LADSPA Plugins.  These plugins are
mostly built for Linux but some are available for Windows.
The Audacity GUI application has a page that points to a collection
of Windows LADSPA plugins.

http://audacity.sourceforge.net/download/plugins

SoX will search for these plugins based on LADSPA_PATH
enviornment variable.  See sox.txt for further information.
