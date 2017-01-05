#!/usr/bin/perl

use warnings;
use strict;

open(CONFIG_FILE, '<', ".config");
while(<CONFIG_FILE>)
{
  chomp;
  if( $_ =~ /^CONFIG_PACKAGE_(.+?)=m$/)
  {
    print("# CONFIG_PACKAGE_$1 is not set\n");
  }
  else
  {
    print("$_\n");
  }
}
