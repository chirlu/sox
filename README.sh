#!/bin/sh

cat > README << .
			      SoX: Sound eXchange
			      ===================

SoX (Sound eXchange) is the Swiss Army knife of sound processing tools: it
can convert sound files between many different file formats & audio devices,
and can apply many sound effects & transformations, as well as doing basic
analysis and providing input to more capable analysis and plotting tools.

SoX is licensed under the GNU GPL and GNU LGPL.  To be precise, the 'sox'
and 'soxi' programs are distributed under the GPL, while the library
'libsox' (in which most of SoX's functionality resides) is dual-licensed.
Note that some optional components of libsox are GPL only: if you use these,
you must use libsox under the GPL.  See INSTALL for the list of optional
components and their licences.

If this distribution is of source code (as opposed to pre-built binaries),
then you will need to compile and install SoX as described in the 'INSTALL'
file.

Changes between this release and previous releases of SoX can be found in
the 'ChangeLog' file; a summary of the file formats and effects supported in
this release can be found below.  Detailed documentation for using SoX can
be found in the distributed 'man' pages:

  o  sox(1)
  o  soxi(1)
  o  soxformat(7)
  o  libsox(3)

or in plain text or PDF files for those systems without man.

The majority of SoX features and fixes are contributed by SoX users - thank
you very much for making SoX a success!  There are several new features
wanted for SoX, listed on the feature request tracker at the SoX project
home-page:

		    http://sourceforge.net/projects/sox

users are encouraged to implement them!

Please submit bug reports, new feature requests, and patches to the relevant
tracker at the above address, or by email:

		   mailto:sox-devel@lists.sourceforge.net

Also accessible via the project home-page is the SoX users' discussion
mailing list which you can join to discuss all matters SoX with other SoX
users; the mail address for this list is:

		   mailto:sox-users@lists.sourceforge.net

.
cat FEATURES.in \
	| sed "s/!!/			/" \
	| sed "s/^\*\*/    o /" \
	| sed "s/^\*/  o /" \
	| sed "s/(:[a-zA-Z0-9_= -]*:)//g" \
       	>> README

echo version=pmwiki-2.2.0-beta65 ordered=1 urlencoded=1 > Docs.Features
echo -n text= >> Docs.Features
cat FEATURES.in|sed "s/%/%25/g"|sed "s/$/%0a/"|tr -d '\n' >> Docs.Features
