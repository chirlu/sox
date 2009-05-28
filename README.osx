SoX
---

This file contains information specific to the MacOS X version of SoX.
Please refer to the README file for general information.

The sox executable can be installed anywhere you desire.  It is a
self-contained statically linked executable.

If the sox executable is invoked with an executable name of soxi, play,
or rec it will perform the functions of those applications as defined
in included documents.

Symlinks can be created for this purpose; such as:

ln -s sox soxi
ln -s sox play
ln -s sox rec

This also roughly equates to invoking sox with following options:

soxi: sox --info [input files and options]
play: sox [input files and options] -d [effects]
rec: sox -d [output file and options] [effects]

wget
----

SoX can make use of the wget command line utility to load files over
the internet or list to shoutcast streams.  It only needs to be 
somewhere in your path to be used by SoX.

Please consult wget's homepage for access to source code as well
as further instructions on configuring and installing.

http://www.gnu.org/software/wget

Acknowledgements
----------------

The sox exectables included in this package makes use of the following projects:

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

MP3 support can be enabled by placing Lame encoding library and/or
MAD decoding library into a standard library search location such
as /usr/lib or /usr/local/lib..
These can be compiled yourself, may turn up on searches of the internet
or may be included with other MP3 applications already installed
on your system. Try searching for libmp3lame.dylib and libmad.dylib.

Obtain the latest Lame and MAD source code from approprate locations.

Lame MP3 encoder  http://lame.sourceforge.net
MAD MP3 decoder   http://www.underbit.com/products/mad 

If your system is setup to compile software, then the following commands
can be used:

cd lame-398-2
./configure
make
sudo make install

cd libmad-0.15.1b
./configure
make
sudo make install
