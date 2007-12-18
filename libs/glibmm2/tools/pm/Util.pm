# gtkmm - Util module
#
# Copyright 2001 Free Software Foundation
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or 
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# # but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
# GNU General Public License for more details. 
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
#
#
# This file holds basic functions used throughout gtkmmproc modules.
# Functions in this module are exported so there is no need to 
# request them by module name.
#
package Util;
use strict;
use warnings;

BEGIN {
     use Exporter   ();
     our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

     # set the version for version checking
     $VERSION     = 1.00;
     @ISA         = qw(Exporter);
     @EXPORT      = qw(&string_unquote &string_trim &string_canonical
                       &trace &unique);
     %EXPORT_TAGS = ( );

     # your exported package globals go here,
     # as well as any optionally exported functions
     #@EXPORT_OK   = qw($Var1 %Hashit &func3);
     }
our @EXPORT_OK;


#$ string_unquote($string)
# Removes leading and trailing quotes.
sub string_unquote($)
{
    my ($str) = @_;
    
    $str =~ s/^['`"]// ;
    $str =~ s/['`"]$// ;
 
    return $str;
}
         
# $ string_trim($string)
# Removes leading and trailing white space.
sub string_trim($)
{
  ($_) = @_;
  s/^\s+//;
  s/\s+$//;
  return $_;
}

#  $ string_canonical($string)
# Convert - to _.
sub string_canonical($)
{
  ($_) = @_;
  s/-/_/g ; # g means 'replace all'
  s/\//_/g ; # g means 'replace all'
  return $_;
}

#
#  Back tracing utility.  
#    Prints the call stack.
#
#  void trace()
sub trace()
{
  my ($package, $filename, $line, $subroutine, $hasargs,
   $wantarray, $evaltext, $is_require, $hints, $bitmask) = caller(1);

  no warnings qw(uninitialized);

  my $i = 2;
  print "Trace on ${subroutine} called from ${filename}:${line}\n";
  while (1)
  {
    ($package, $filename, $line, $subroutine) = caller($i);
    $i++;
    next if ($line eq "");
    print "  From ${subroutine} call from ${filename}:${line}\n";
  }
}

sub unique(@)
{
  my %hash;
  foreach (@_)
  {
    $hash{$_}=1;
  }

  return keys %hash;
}

1; # indicate proper module load.

