SoX
---

This file contains information specific to the MacOS X version of SoX.
Please refer to the README file for general information.

The sox executable can be installed anywhere you desire.  It is a
self-contained statically linked executable.

If the sox executable is invoked with an executable name of soxi, play,
or rec it will perform the functions of those applications as defined
in included documents.  Symlinks are included with this package
but you can also make your own.

Acknowledgements
----------------

The sox exectables included in this package makes use of the following projects:

  SoX - http://sox.sourceforge.net

  FLAC - http://flac.sourceforge.net

  LADSPA - http://www.ladspa.org

  libid3tag - http://www.underbit.com/products/mad

  libltdl - http://www.gnu.org/software/libtool

  libsndfile - http://www.mega-nerd.com/libsndfile

  Ogg Vorbis - http://www.vorbis.com

  PNG - http://www.libpng.org/pub/png

  WavPack - http://www.wavpack.com

Enjoy,
The SoX Development Team

Appendix - wget Support
-----------------------

SoX can make use of the wget command line utility to load files over
the internet or listen to shoutcast streams.  It only needs to be
somewhere in your path to be used by SoX.

Please consult wget's homepage for access to source code as well
as further instructions on configuring and installing.

http://www.gnu.org/software/wget

Appendix - MP3 Support
----------------------

SoX contains support for reading and writing MP3 files but does not ship
with the dylib's that perform decoding and encoding of MP3 data because
of patent restrictions.  For further details, refer to:

http://en.wikipedia.org/wiki/MP3#Licensing_and_patent_issues

MP3 support can be enabled by placing Lame encoding library and/or
MAD decoding library into a standard library search location such
as /usr/lib or set LTDL_LIBRARY_PATH to location.

These can be compiled yourself, they may turn up on searches of the internet
or may be included with other MP3 applications already installed
on your system. Try searching for libmp3lame.dylib and libmad.dylib.

Obtain the latest Lame and MAD source code from approprate locations.

Lame MP3 encoder  http://lame.sourceforge.net
MAD MP3 decoder   http://www.underbit.com/products/mad

If your system is setup to compile software, then the following commands
can be used.  Note: since SoX is distributed as a 32-bit i386 binary,
the library must match that format as well:

cd lame-398-2
./configure CFLAGS="-arch i386 -m32" LDFALGS="-arch i386"
make
sudo make install

cd libmad-0.15.1b
./configure CFLAGS="-arch i386 -m32" LDFALGS="-arch i386"
make
sudo make install

Appendix - AMR-NB/AMR-WB Support
--------------------------------

SoX contains support for reading and writing AMR-NB and AMR-WB files but
does not ship with the DLL's that perform decoding and encoding of AMR
data because of patent restrictions.

AMR-NB/AMR-WB support can be enabled by placing required libraries
into a standard library search location such as /usr/lib
or set LTDL_LIBRARY_PATH to search path.

These can be compiled yourself, they may turn up on searches of the internet
or may be included with other AMR applications already installed
on your system. Try searching for libopencore-amrnb.dylib and
libopencore-amrwb.dylib.

Obtain the latest amrnb and amrwb source code from
http://sourceforge.net/projects/opencore-amr

cd opencore-amr-0.1.2
./configure CFLAGS="-arch i386 -m32" LDFALGS="-arch i386"
make
sudo make install

If that does not work, then try this:

cd opencore-amr-0.1.2
./build_osx.sh

Appendix - LADSPA Plugins
-------------------------

SoX has built in support for LADSPA Plugins.  These plugins are
mostly built for Linux but some are available for OS X.
The Audacity GUI application has a page that points to a collection
of OS X LADSPA plugins.

http://audacity.sourceforge.net/download/plugins

SoX will search for these plugins based on LADSPA_PATH
enviornment variable.  See sox.txt for further information.
