#!/usr/bin/perl -w
# This script is meant to live in SoX git.
# Its sole purpose is to update the versions in the debian directory
# so users can always build a binary package after a git checkout.
#
# Written by Pascal Giard <evilynux@gmail.com>.
use strict;

my $file = "configure.ac";
my $changelog = "debian/changelog";
my $rules = "debian/rules";
my $version;
my @content;

die "$file doesn't exist.\n" unless( -e $file );
die "$changelog doesn't exist.\n" unless( -e $changelog );
die "$rules exist.\n" unless( -e $rules );

# Get current version
open( FH, $file );
while( <FH> ) {
    if( $_ =~ m/AC_INIT\(SoX, (\d+\.\d+\.\d+)(git)?,/ ) {
        $version = $1;
        last;
    }
}
close( FH );

die "Can't determine version number.\n" unless( $version );

# Update debian/changelog
my $date = `date +%Y%m%d`;
chop($date);
open( FH, $changelog );
@content = <FH>;
close( FH );
die "Can't modify $changelog!\n"
  unless $content[0] =~ s/^(sox \()\d+\.\d+\.\d+(-\d+\+git)\d{8}(-1\).*)$/$1$version$2$date$3/;

open( FH, "> $changelog" );
print FH @content;
close( FH );

# Update debian/rules
open( FH, $rules );
@content = <FH>;
close( FH );
$_ =~ s/^(DEB_TAR_SRCDIR := sox-)\d+\.\d+\.\d+(.*)$/$1$version$2/
  foreach(@content);
open( FH, ">" . $rules );
print FH @content;
close( FH );
