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
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

use strict;
$|=1;

# set default values
my $sox='../src/sox';
my $effect='rate';
#my $effect='rate -q';
#my $effect='rate -l';
#my $effect='rate -h -M';
#my $effect='rate -h -b 90';
#my $effect='sinc 400-2000';
#my $effect='sinc -n 1024 400-2000';

#my ($rate0,$rate1)=(44100,44100); # sample rates
my ($rate0,$rate1)=(8000,22050); # sample rates
my $p=400;  # silence before/after tonepulse
my $attack=4000;
my $duration=16000;

#my ($rate0,$rate1)=(22050,8000); # sample rates
#my $p=1102;  # silence before/after tonepulse
#my $attack=11025;
#my $duration=44100;

# parse commandline arguments
my $updown = 0; # set to 1 for up/down rate-conversion test
my ($model,$t,$rms,$lim)=("./model","sw",11585.2, 0.5);
while ($_ = shift @ARGV) {
  if ($_ eq "-l") {
    ($model,$t,$rms,$lim)=("./lmodel","sl",759250125.0, 50.0);
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
if ($effect =~ m{^(rate|upsample|downsample|speed)}) {
  $ratechange=1;
}

# output a nice header explaining the data 
if ($ratechange==0) {
  print("# Testing $sox -c1 -r$rate0 i0.xx.$t j0.xx.$t $effect\n");
} else {
  print("# Testing $sox -c1 -r$rate0 i*.$t -r$rate1 u*.$t $effect\n");
  if ($updown==1) {
    print("#   then back down to $rate0\n");
  }
}
print("#   with tone pulses from 0.01 to 0.99 percent of Nyquist\n");

# generate the test data
my $f;
my %q;
my $nyq = ($rate0<=$rate1)? 1.0:($rate1/$rate0); 
my $l=$attack + $duration + $attack;
my $env="$attack:$duration:$attack"; # attack, duration, drop
for ($f=0.01; $f<.999; $f+=0.01) {
  my @mod;

  my $s=sprintf("%4.2f",$f);
  qx{$sox -2r2 -n i$s.$t synth ${l}s sin $s vol .5 fade h ${attack}s ${l}s ${attack}s pad ${p}s ${p}s};
  if ($ratechange==0) {
    qx{$sox -c1 -r$rate0 i$s.$t -r$rate0 o$s.$t $effect} ;
    @mod = grep {/v2max/} qx{$model -f$s -e$env $rate0 o$s.$t 2>&1};
  } else {
    qx{$sox -c1 -r$rate0 i$s.$t -r$rate1 u$s.$t $effect 2>/dev/null};
    if ($updown) {
      qx{$sox -c1 -r$rate1 u$s.$t -r$rate0 o$s.$t $effect 2>/dev/null};
      @mod = grep {/v2max/} qx{$model -f$s -e$env $rate0:$rate0 o$s.$t 2>&1};
    }else{
      @mod = grep {/v2max/} qx{$model -f$s -e$env $rate0:$rate1 u$s.$t 2>&1};
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
  unlink "i$s.$t","u$s.$t","o$s.$t";
}

exit 0;
