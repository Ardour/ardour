# gtkmm - GtkDefs module
#
# Copyright 2001 Free Software Foundation
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
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
#
package GtkDefs;
use strict;
use warnings;

use Util;
use Enum;
use Object;
use Property;
use FunctionBase;

#
#  Public functions
#    read_defs(path, file)
#
#    @ get_methods()
#    @ get_signals()
#    @ get_properties()
#
#    $ lookup_enum(c_type)
#    $ lookup_object(c_name)
#    $ lookup_method(c_name)
#    $ lookup_function(c_name)
#    $ lookup_property(object, c_name)
#    $ lookup_signal(object, c_name)
#

BEGIN {
     use Exporter   ();
     our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

     # set the version for version checking
     $VERSION     = 1.00;

     @ISA         = qw(Exporter);
     @EXPORT      = ( );
     %EXPORT_TAGS = ( );

     # your exported package globals go here,
#    # as well as any optionally exported functions
     @EXPORT_OK   = ( );
}
our @EXPORT_OK;

#####################################

use strict;
use warnings;

#####################################

%GtkDefs::enums = (); #Enum
%GtkDefs::objects = (); #Object
%GtkDefs::methods = (); #GtkDefs::Function
%GtkDefs::signals = (); #GtkDefs::Signal
%GtkDefs::properties = (); #Property

@GtkDefs::read = ();
@GtkDefs::file = ();


#####################################
#prototype to get rid of warning
sub read_defs($$;$);

sub read_defs($$;$)
{
  my ($path, $filename, $restrict) = @_;
  $restrict = "" if ($#_ < 2);

  # check that the file is there.
  if ( ! -r "$path/$filename")
  {
     print "Error: can't read defs file $filename\n";
     return;
  }

  # break the tokens into lisp phrases up to three levels deep.
  #   WARNING: reading the following perl statement may induce seizures,
  #   please flush eyes with water immediately, and consult a mortician.
  my @tokens = split(
    m/(
        \(
        (?:
            [^()]*
            \(
            (?:
                [^()]*
                \(
                [^()]*
                \)
            )*
            [^()]*
            \)
        )*
        [^()]*
        \)
    )/x,
    read_file($path, $filename));

  # scan through top level tokens
  while ($#tokens > -1)
  {
    my $token = shift @tokens;
    next if ($token =~ /^\s*$/);

    if ($token =~ /\(include (\S+)\)/)
    {
      read_defs($path,$1,$restrict);
      next;
    }
    elsif ($token =~ /^\(define-flags-extended.*\)$/)
    { on_enum($token); }
    elsif ($token =~ /^\(define-enum-extended.*\)$/)
    { on_enum($token); }
    elsif ($token =~ /^\(define-flags.*\)$/)
    { }
    elsif ($token =~ /^\(define-enum.*\)$/)
    { }
    elsif ($token =~ /^\(define-object.*\)$/)
    { on_object($token); }
    elsif ($token =~ /^\(define-function.*\)$/)
    { on_function($token); }
    elsif ($token =~ /^\(define-method.*\)$/)
    { on_method($token); }
    elsif ($token =~ /^\(define-property.*\)$/)
    { on_property($token); }
    elsif ($token =~ /^\(define-signal.*\)$/)
    { on_signal($token);  }
    elsif ($token =~ /^\(define-vfunc.*\)$/)
    { on_vfunc($token); }
    else
    {
      if ( $token =~ /^\(define-(\S+) (\S+)/)
      {
        # FIXME need to figure out the line number.
        print STDERR "Broken lisp definition for $1 $2.\n";
      }
      else
      {
        print "unknown token $token \n";
      }
    }
  }
}


sub read_file($$)
{
  my ($path, $filename)=@_;
  my @buf = ();

  # don't read a file twice
  foreach (@GtkDefs::read)
  {
    return "" if ($_ eq "$path/$filename");
  }
  push @GtkDefs::read, "$path/$filename";

  # read file while stripping comments
  open(FILE, "$path/$filename");
  while (<FILE>)
  {
     s/^;.*$//;  # remove comments
     chop;      # remove new lines
     push(@buf, $_);
  }
  close(FILE);

  $_ = join("", @buf);
  s/\s+/ /g;
  return $_;
}


sub on_enum($)
{
  my $thing = Enum::new(shift(@_));
  $GtkDefs::enums{$$thing{c_type}} = $thing;
}

sub on_object($)
{
  my $thing = Object::new(shift(@_));
  $GtkDefs::objects{$$thing{c_name}} = $thing;
}

sub on_function($)
{
  my $thing = GtkDefs::Function::new(shift(@_));
  $GtkDefs::methods{$$thing{c_name}} = $thing;
}

sub on_method($)
{
  my $thing = GtkDefs::Function::new(shift(@_));
  $GtkDefs::methods{$$thing{c_name}} = $thing if ($thing);
}

sub on_property($)
{
  my $thing = Property::new(shift(@_));
  $GtkDefs::properties{"$$thing{class}::$$thing{name}"} = $thing;
}

sub on_signal($)
{
  my $thing = GtkDefs::Signal::new(shift(@_));
  $GtkDefs::signals{"$$thing{class}::$$thing{name}"} = $thing;
}

sub on_vfunc($)
{
  my $thing = GtkDefs::Signal::new(shift(@_));
  $GtkDefs::signals{"$$thing{class}::$$thing{name}"} = $thing;
}

##########################

sub get_enums
{
  return sort {$$a{c_type} cmp $$b{c_type}} values %GtkDefs::enums;
}
sub get_methods
{
  return sort {$$a{c_name} cmp $$b{c_name}} values %GtkDefs::methods;
}
sub get_signals
{
  return sort {$$a{name} cmp $$b{name}} values %GtkDefs::signals;
}
sub get_properties
{
  return sort {$$a{name} cmp $$b{name}} values %GtkDefs::properties;
}

sub get_marked
{
  no warnings;
  return grep {$$_{mark}==1} values %GtkDefs::methods; 
}

# This searches for items wrapped by this file and then tries to locate
# other functions/signal/properties which may have been left unmarked.
sub get_unwrapped
{
  # find methods which were used in for a _WRAP
  my @targets;
  push @targets,grep {$$_{entity_type} eq "method" && $$_{mark}==1} values %GtkDefs::methods;
  push @targets,grep {$$_{mark}==1} values %GtkDefs::signals;
  push @targets,grep {$$_{mark}==1} values %GtkDefs::properties;

  # find the classes which used them.
  my @classes = join(" ", unique(map { $$_{class} } @targets));

  # find methods which are in those classes which didn't get marked.
  my @unwrapped;
  my $class;
  foreach $class (@classes)
  {
    push @unwrapped, grep {$$_{class} eq $class && $$_{mark}==0} values %GtkDefs::methods;
    push @unwrapped, grep {$$_{class} eq $class && $$_{mark}==0} values %GtkDefs::properties;
    push @unwrapped, grep {$$_{class} eq $class && $$_{mark}==0} values %GtkDefs::signals;
  }

  return @unwrapped;
}

##########################

sub lookup_enum($)
{
  no warnings;
  my ($c_type) = @_;
  my $obj = $GtkDefs::enums{$c_type};
  return 0 if(!$obj);
  $$obj{mark} = 1;
  return $obj;
}

sub lookup_object($)
{
  no warnings;
  return $GtkDefs::objects{$_[0]};
}

# $objProperty lookup_property($name, $parent_object_name)
sub lookup_property($$)
{
  no warnings;
  my ($parent_object_name, $name) = @_;
  $name =~ s/-/_/g;
  my $obj = $GtkDefs::properties{"${parent_object_name}::${name}"};
  return 0 if ($obj eq "");
  $$obj{mark} = 1;
  return $obj;
}

sub lookup_method_dont_mark($)
{
  no warnings;
  my ($c_name) = @_;
  $c_name =~ s/-/_/g;

  my $obj = $GtkDefs::methods{$c_name};
  return 0 if ($obj eq "");

  return $obj;
}

sub lookup_method($)
{
  my $obj = lookup_method_dont_mark($_);

  $$obj{mark} = 1 if($obj);
  return $obj;
}

sub lookup_function($)
{
  return lookup_method($_[0]);
}

sub lookup_signal($$)
{
  no warnings;
  my ($parent_object_name, $name) = @_;

  $name =~ s/-/_/g;
  my $obj = $GtkDefs::signals{"${parent_object_name}::${name}"};
  return 0 if ($obj eq "");
  $$obj{mark} = 1;
  return $obj;
}

sub error
{
  my $format = shift @_;
  printf STDERR "GtkDefs.pm: $format\n", @_;
}


########################################################################
package GtkDefs::Function;
BEGIN { @GtkDefs::Function::ISA=qw(FunctionBase); }

#  class Function : FunctionBase
#
#    {
#       string name; e.g. gtk_accelerator_valid
#       string c_name;
#       string class e.g. GtkButton 
#
#       string rettype;
#       string array param_types;
#       string array param_names;
#
#       string entity_type. e.g. method or signal
#
#       bool varargs;
#       bool mark;
#
#    }

# "new" can't have prototype
sub new
{
  my ($def) = @_;
  my $whole = $def;
  my $self = {};
  bless $self;

  $def =~ s/^\(//;
  $def =~ s/\)$//;
  $def =~ s/^\s*define-(\S+)\s+(\S+)\s*//;
  $$self{entity_type} = $1;
  $$self{name} = $2;
  $$self{name} =~ s/-/_/g; # change - to _

  # init variables
  $$self{mark} = 0;
  $$self{rettype} = "none";
  $$self{param_types} = [];
  $$self{param_names} = [];
  $$self{class} = "";

  # snarf down lisp fields
  $$self{c_name} = $1     if ($def=~s/\(c-name "(\S+)"\)//);
  $$self{class} = $1      if ($def=~s/\(of-object "(\S+)"\)//);

  if ($def =~ s/\(return-type "(\S+)"\)//)
  {
    $$self{rettype} = $1;
    $$self{rettype} =~ s/-/ /g; #e.g. replace const-gchar* with const gchar*. Otherwise it will be used in code.
  }

  $$self{varargs} = 1     if ($def=~s/\(varargs\s+#t\)//);
  $$self{rettype} = "void"  if ($$self{rettype} eq "none");

  # methods have a parameter not stated in the defs file
  if ($$self{entity_type} eq "method")
  {
    push( @{$$self{param_types}}, "$$self{class}*" );
    push( @{$$self{param_names}}, "self" );
  }

  # parameters are compound lisp statement
  if ($def =~ s/\(parameters(( '\("\S+" "\S+"\))*) \)//)
  {
    $self->parse_param($1);
  }

  # is-constructor-of:
  if ($def =~ s/\(is-constructor-of "(\S+)"\)//)
  {
    #Ignore them.
  }
  
  # of-object
  if ($def =~ s/\(of-object "(\S+)"\)//)
  {
    #Ignore them.
  }

  GtkDefs::error("Unhandled function parameter ($def) in $$self{c_name}\n")
    if ($def !~ /^\s*$/);

  return $self;
}

sub parse_param($$)
{
  my ($self, $param) = @_;

  # break up the parameter statements
  foreach (split(/\s*'*[()]\s*/, $param))
  {
    next if ($_ eq "");
    if (/^"(\S+)" "(\S+)"$/)
    {
      my ($p1, $p2) = ($1,$2);
      $p1 =~ s/-/ /;
      push( @{$$self{param_types}}, $p1);
      push( @{$$self{param_names}}, $p2);
    }
    else
    {
      GtkDefs::error("Unknown parameter statement ($_) in $$self{c_name}\n");
    }
  }
}


# $string get_return_type_for_methods().
# Changes gchar* (not const-gchar*) to return-gchar* so that _CONVERT knows that it needs to be freed.
sub get_return_type_for_methods($)
{
  my ($self) = @_;

  my $rettype = $$self{rettype};
  if($rettype eq "gchar*" || $rettype eq "char*")
  {
    $rettype = "return-" . $rettype;
  }

  return $rettype;
}

sub get_param_names
{
  my ($self) = @_;
  return @$self{param_names};
}

######################################################################
package GtkDefs::Signal;
BEGIN { @GtkDefs::Signal::ISA=qw(GtkDefs::Function); }

#  class Signal : Function
#    {
#       string name; e.g. gtk_accelerator_valid
#       string class e.g. GtkButton ( == of-object.)
#
#       string rettype;
#
#       string when. e.g. first, last, or both.
#       string entity_type. e.g. method or signal
#    }

# "new" can't have prototype
sub new
{
  my ($def) = @_;

  my $whole = $def;
  my $self = {};
  bless $self;

  #Remove first and last braces:
  $def =~ s/^\(//;
  $def =~ s/\)$//;

  $def =~ s/^\s*define-(\S+)\s+(\S+)\s*//;
  $$self{entity_type} = $1;
  $$self{name} = $2;
  $$self{name} =~ s/-/_/g; #change - to _

  # init variables
  $$self{mark}=0;
  $$self{rettype} = "none";
  $$self{param_types} = [];
  $$self{param_names} = [];
  $$self{when} = "";
  $$self{class} = "";

  # snarf down lisp fields
  if($def =~ s/\(of-object "(\S+)"\)//)
  {
    $$self{class} = $1;
  }
  else
  {
    GtkDefs::error("define-signal/define-vfunc without of-object (entity type: $$self{entity_type}): $whole");
  }

  if($def =~ s/\(return-type "(\S+)"\)//)
  {
    $$self{rettype} = $1;
    $$self{rettype} =~ s/-/ /g; #e.g. replace const-gchar* with const gchar*. Otherwise it will be used in code.
  }

  if($def =~ s/\(when "(\S+)"\)//)
  {
    $$self{when} = $1;
  }

  if($$self{rettype} eq "none")
  {
    $$self{rettype} = "void"
  }

  # signals always have a parameter
  push(@{$$self{param_types}}, "$$self{class}*");
  push(@{$$self{param_names}}, "self");

  # parameters are compound lisp statement
  if ($def =~ s/\(parameters(( '\("\S+" "\S+"\))+) \)//)
  {
    $self->parse_param($1);
  }

  if ($def!~/^\s*$/)
  {
	  GtkDefs::error("Unhandled signal/vfunc def ($def) in $$self{class}::$$self{name}");
  }

  return $self;
}

# bool has_same_types($objFunction)
# Compares return types and argument types
sub has_same_types($$)
{
  my ($self, $objFuncOther) = @_;

  #Compare return types:
  if($self->types_are_equal($$self{rettype}, $$objFuncOther{rettype}) ne 1)
  {
    # printf("debug: different return types: %s, %s\n", $$self{rettype}, $$objFuncOther{rettype});
    return 0; #Different types found.
  }

  #Compare arguement types:
  my $i = 0;
  my $param_types = $$self{param_types};
  my $param_types_other = $$objFuncOther{param_types};
  for ($i = 1; $i < $#$param_types + 1; $i++)
  {
    my $type_a = $$param_types[$i];
    my $type_b = $$param_types_other[$i-1];

    if($self->types_are_equal($type_a, $type_b) ne 1)
    {
      # printf("debug: different arg types: %s, %s\n", $type_a, $type_b);
      return 0; #Different types found.
    }
  }

  return 1; #They must all be the same for it to get this far.
}

# bool types_are_equal($a, $b)
# Compares types, ignoring gint/int differences, etc.
sub types_are_equal($$$)
{
  #TODO: Proper method of getting a normalized type name.

  my ($self, $type_a, $type_b) = @_;

  if($type_a ne $type_b)
  {
    #Try adding g to one of them:
    if( ("g" . $type_a) ne $type_b )
    {
      #Try adding g to the other one:
      if( $type_a ne ("g" . $type_b) )
      {
        #After all these checks it's still not equal:
        return 0; #not equal.
      }
    }
  }

  # printf("DEBUG: types are equal: %s, %s\n", $$type_a, $$type_b);
  return 1; #They must be the same for it to get this far.
}

1; # indicate proper module load.
