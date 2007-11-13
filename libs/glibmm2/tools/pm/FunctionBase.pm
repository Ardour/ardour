package FunctionBase;

use strict;
use warnings;
use Util;

BEGIN {
     use Exporter   ();
     our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

     # set the version for version checking
     $VERSION     = 1.00;
     @ISA         = qw(Exporter);
     @EXPORT      = qw(&func1 &func2 &func4);
     %EXPORT_TAGS = ( );
     # your exported package globals go here,
     # as well as any optionally exported functions
     @EXPORT_OK   = qw($Var1 %Hashit &func3);
     }
our @EXPORT_OK;

##################################################
### FunctionBase
# Contains data and methods used by both Function (C++ declarations) and GtkDefs::Function (C defs descriptions)
# Note that GtkDefs::Signal inherits from GtkDefs::Function so it get these methods too.
#
#  class Function : FunctionBase
#    {
#       string array param_types;
#       string array param_names;
#       string array param_documentation;
#       string return_documention;
#    }


# $string args_types_only($)
# comma-delimited argument types.
sub args_types_only($)
{
  my ($self) = @_;

  my $param_types = $$self{param_types};
  return join(", ", @$param_types);
}

# $string args_names_only($)
sub args_names_only($)
{
  my ($self) = @_;

  my $param_names = $$self{param_names};
  return join(", ", @$param_names);
}

# $string args_types_and_names($)
sub args_types_and_names($)
{
  my ($self) = @_;

  my $i;

  my $param_names = $$self{param_names};
  my $param_types = $$self{param_types};
  my @out;

  #debugging:
  #if($#$param_types)
  #{
  #  return "NOARGS";
  #}

  for ($i = 0; $i < $#$param_types + 1; $i++)
  {
    my $str = sprintf("%s %s", $$param_types[$i], $$param_names[$i]);
    push(@out, $str);
  }

  my $result =  join(", ", @out);
  return $result;
}

# $string args_names_only_without_object($)
sub args_names_only_without_object2($)
{
  my ($self) = @_;

  my $param_names = $$self{param_names};

  my $result = "";
  my $bInclude = 0; #Ignore the first (object) arg.
  foreach (@{$param_names})
  {
    # Add comma if there was an arg before this one:
    if( $result ne "")
    {
      $result .= ", ";
    }

    # Append this arg if it's not the first one:
    if($bInclude)
    {
      $result .= $_;
    }

    $bInclude = 1;
  }

  return $result;
}

# $string args_types_and_names_without_object($)
sub args_types_and_names_without_object($)
{
  my ($self) = @_;

  my $param_names = $$self{param_names};
  my $param_types = $$self{param_types};
  my $i = 0;
  my @out;

  for ($i = 1; $i < $#$param_types + 1; $i++) #Ignore the first arg.
  {
    my $str = sprintf("%s %s", $$param_types[$i], $$param_names[$i]);
    push(@out, $str);
  }

  return join(", ", @out);
}

# $string args_names_only_without_object($)
sub args_names_only_without_object($)
{
  my ($self) = @_;

  my $param_names = $$self{param_names};

  my $result = "";
  my $bInclude = 0; #Ignore the first (object) arg.
  foreach (@{$param_names})
  {
    # Add comma if there was an arg before this one:
    if( $result ne "")
    {
      $result .= ", ";
    }

    # Append this arg if it's not the first one:
    if($bInclude)
    {
      $result .= $_;
    }

    $bInclude = 1;
  }

  return $result;
}

sub dump($)
{
  my ($self) = @_;

  my $param_types = $$self{param_types};
  my $param_names = $$self{param_names};

  print "<function>\n";
  foreach (keys %$self)
  {
    print "  <$_ value=\"$$self{$_}\"/>\n" if (!ref $$self{$_} && $$self{$_} ne "");
  }

  if (scalar(@$param_types)>0)
  {
    print "  <parameters>\n";

    for (my $i = 0; $i < scalar(@$param_types); $i++)
    {
      print "    \"$$param_types[$i]\" \"$$param_names[$i]\" \n";
    }

    print "  </parameters>\n";
  }

  print "</function>\n\n";
}

sub args_types_and_names_with_default_values($)
{
  my ($self) = @_;

  my $i;

  my $param_names = $$self{param_names};
  my $param_types = $$self{param_types};
  my $param_default_values = $$self{param_default_values};
  my @out;
  
  for ($i = 0; $i < $#$param_types + 1; $i++)
  {
    my $str = sprintf("%s %s", $$param_types[$i], $$param_names[$i]);

    if(defined($$param_default_values[$i]))
    {
      if($$param_default_values[$i] ne "")
      {
        $str .= " = " . $$param_default_values[$i];
      }
    }

    push(@out, $str);
  }

  return join(", ", @out);
}

1; # indicate proper module load.

