#!/usr/bin/perl -w

# wtest.pl -- program to help generate response/error graphs for adpcm/gsm code.
#
# Copyright (C) 1999 Stanley J. Brooks <stabro@megsinet.net> 
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

use strict;
$|=1;

# set default values
my $toast='/usr/bin/toast';
my $sox='../sox';
my $fmt='';

my $rate=8000; # sample rates
my $p=200;  # silence before/after tonepulse
my $env="400:800:400"; # attack, duration, drop

my ($ding,$model,$t,$rms,$lim)=("./ding","./model","sw",11585.2, 0.5);

my $effect=''; # you may want to try a filter

# parse commandline arguments
while ($_ = shift @ARGV) {
	if (m{^-[tgai]$}) {
		$fmt=$_;
	} else {
		unshift @ARGV,$_;
		last;
	}
}
if ($#ARGV >= 0) {
	$effect="@ARGV";
}

# output a nice header explaining the data 
print("# Testing gsm compress/decompress ");
if ($fmt eq '-t') {
	print("by $toast\n");
}else{
	print("by $sox -r$rate in.sw $fmt out.wav $effect\n");
}
print("#   with tone pulses from 0.00 to 0.99 percent of Nyquist\n");
print("#   produced by $ding -f0.xx -v0.5 -e$p:$env:$p d/i0.xx.$t\n");
print("#   col 1 is frequency/Nyquist\n");
print("#   col 2 is (power) dB gain\n");
print("#   col 3 is (power) dB level of error signal\n");
print("#\n#freq dB-gain dB-error\n");

# generate the test data
mkdir("d",0775);
my $f;
my %q;
my $nyq = 1.0; 
for ($f=0.00; $f<1.001; $f+=0.01) {
	my @mod;

	#if ($f>0.995) { $f=0.999; }
	my $s=sprintf("%4.2f",$f);
	#print "$ding -f$s -v0.5 -e$p:$env:$p -d1.0 d/i$s.$t\n";
	qx{$ding -f$s -v0.5 -e$p:$env:$p -d1.0 d/i$s.$t &>/dev/null};

	if ($fmt eq '-t') {
		qx{$toast -l d/i$s.$t};
		qx{$toast -dl d/i$s.$t.gsm};
	}else{
		qx{cp d/i$s.$t d/a$s.$t 2>/dev/null};
		qx{$sox -r$rate d/i$s.$t -g d/g$s.wav $effect 2>/dev/null};
		unlink "d/i$s.$t";
		qx{$sox d/g$s.wav d/i$s.$t 2>/dev/null};
		qx{cp d/i$s.$t d/b$s.$t 2>/dev/null};
		unlink "d/g$s.wav";
	}

	@mod = grep {/v2max/} qx{$model -f$s -e$env $rate d/i$s.$t 2>&1};
	print STDERR "$s: ",@mod;
	$_=shift(@mod);
	if (m{s2max *([0-9.]+), *v2max *([0-9.]+), *rmserr *(-?[0-9.]+)}) {
		#print("$s $1\n");
		#print("$s $1 $3\n");
		my $v = ($1 > $lim)? $1 : $lim;
		my $r = ($3 > $lim)? $3 : $lim;
		my $dbv = 20*log($v/$rms)/log(10);
		my $dbr = 20*log($r/$rms)/log(10);
		printf("%s %.3f %.3f\n",$s,$dbv,$dbr);
	}
	unlink "d/i$s.$t";
}
print("#freq dB-gain dB-error\n");
exit 0;
