#!/usr/bin/perl -w

# ltest.pl -- program to help generate response/error graphs for DSP code.
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
my $sox='../sox';
my $effect='resample';
#my $effect='resample -qs';
#my $effect='resample -q';
#my $effect='resample -ql';
#my $effect='polyphase -cutoff 0.90';
#my $effect='filter 400-2000';
#my $effect='filter 400-2000 1024';

my ($rate0,$rate1)=(8000,22050); # sample rates
my $p=400;  # silence before/after tonepulse
my $env="4000:16000:4000"; # attack, duration, drop

#my ($rate0,$rate1)=(22050,8000); # sample rates
#my $p=1102;  # silence before/after tonepulse
#my $env="-e11025:44100:11025"; # attack, duration, drop

# parse commandline arguments
my $updown = 0; # set to 1 for up/down rate-conversion test
my ($ding,$model,$t,$rms,$lim)=("./ding","./model","sw",11585.2, 0.5);
while ($_ = shift @ARGV) {
	if ($_ eq "-l") {
		($ding,$model,$t,$rms,$lim)=("./lding","./lmodel","sl",759250125.0, 50.0);
	} elsif ($_ eq "-ud") {
		$updown=1;
	} else {
		unshift @ARGV,$_;
		last;
	}
}
if ($#ARGV >= 0) {
	$effect="@ARGV";
}

my $ratechange=0;
if ($effect =~ m{^(resample|polyphase|rate)}) {
	$ratechange=1;
}

# output a nice header explaining the data 
if ($ratechange==0) {
	print("# Testing $sox -r$rate0 d/i0.xx.$t d/j0.xx.$t $effect\n");
} else {
	print("# Testing $sox -r$rate0 d/i*.$t -r$rate1 d/u*.$t $effect\n");
	if ($updown==1) {
		print("#   then back down to $rate0\n");
	}
}
print("#   with tone pulses from 0.00 to 0.99 percent of Nyquist\n");
print("#   produced by $ding -f0.xx -v0.5 -e$p:$env:$p d/i0.xx.$t\n");

# generate the test data
mkdir("d",0775);
my $f;
my %q;
my $nyq = ($rate0<=$rate1)? 1.0:($rate1/$rate0); 
for ($f=0.00; $f<1.001; $f+=0.01) {
	my @mod;

	#if ($f>0.995) { $f=0.999; }
	my $s=sprintf("%4.2f",$f);
	#print "$ding -f$s -v0.5 -d1.0 -e$p:$env:$p d/i$s.$t\n";
	qx{$ding -f$s -v0.5 -d1.0 -e$p:$env:$p d/i$s.$t &>/dev/null};
	if ($ratechange==0) {
		qx{$sox -r$rate0 d/i$s.$t -r$rate0 d/j$s.$t $effect} ;
		@mod = grep {/v2max/} qx{$model -f$s -e$env $rate0 d/j$s.$t 2>&1};
	} else {
		qx{$sox -r$rate0 d/i$s.$t -r$rate1 d/u$s.$t $effect 2>/dev/null};
		if ($updown) {
			qx{$sox -r$rate1 d/u$s.$t -r$rate0 d/o$s.$t $effect 2>/dev/null};
			@mod = grep {/v2max/} qx{$model -f$s -e$env $rate0:$rate0 d/o$s.$t 2>&1};
		}else{
			@mod = grep {/v2max/} qx{$model -f$s -e$env $rate0:$rate1 d/u$s.$t 2>&1};
		}
	}
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
	#unlink "d/i$s.$t","d/u$s.$t","d/o$s.$t";
}

exit 0;
