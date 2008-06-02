package Enum;

use strict;
use warnings;

BEGIN {
     use Exporter   ();
     our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

     # set the version for version checking
     $VERSION     = 1.00;
     @ISA         = qw(Exporter);
     @EXPORT      = ( );
     %EXPORT_TAGS = ( );
     # your exported package globals go here,
     # as well as any optionally exported functions
     @EXPORT_OK   = ( );
     }
our @EXPORT_OK;

# class Enum
#    {
#       bool flags;
#       string type;
#       string module;
#       string c_type;
#
#       string array elem_names;
#       string array elem_values;
#
#       bool mark;
#    }


sub new
{
  my ($def) = @_;
  my $self = {};
  bless $self;

  $def =~ s/^\(//;
  $def =~ s/\)$//;

  $$self{mark}  = 0;
  $$self{flags} = 0;

  $$self{elem_names}  = [];
  $$self{elem_values} = [];

  # snarf down the fields

  if($def =~ s/^define-(enum|flags)-extended (\S+)//)
  {
    $$self{type} = $2;
    $$self{flags} = 1 if($1 eq "flags");
  }

  $$self{module} = $1 if($def =~ s/\(in-module "(\S+)"\)//);
  $$self{c_type} = $1 if($def =~ s/\(c-name "(\S+)"\)//);

  # values are compound lisp statement
  if($def =~ s/\(values((?: '\("\S+" "\S+" "[^"]+"\))*) \)//)
  {
    $self->parse_values($1);
  }

  if($def !~ /^\s*$/)
  {
    GtkDefs::error("Unhandled enum def ($def) in $$self{module}\::$$self{type}\n")
  }

  # this should never happen
  warn if(scalar(@{$$self{elem_names}}) != scalar(@{$$self{elem_values}}));

  return $self;
}

sub parse_values($$)
{
  my ($self, $value) = @_;

  my $elem_names  = [];
  my $elem_values = [];
  my $common_prefix = undef;

  # break up the value statements
  foreach(split(/\s*'*[()]\s*/, $value))
  {
    next if($_ eq "");

    if(/^"\S+" "(\S+)" "([^"]+)"$/)
    {
      my ($name, $value) = ($1, $2);

      # detect whether there is module prefix common to all names, e.g. GTK_
      my $prefix = $1 if ($name =~ /^([^_]+_)/);

      if (not defined($common_prefix))
      {
        $common_prefix = $prefix;
      }
      elsif ($prefix ne $common_prefix)
      {
        $common_prefix = "";
      }

      push(@$elem_names, $name);
      push(@$elem_values, $value);
    }
    else
    {
      GtkDefs::error("Unknown value statement ($_) in $$self{c_type}\n");
    }
  }

  if ($common_prefix)
  {
    # cut off the module prefix, e.g. GTK_
    s/^$common_prefix// foreach (@$elem_names);
  }

  $$self{elem_names}  = $elem_names;
  $$self{elem_values} = $elem_values;
}

sub beautify_values($)
{
  my ($self) = @_;

  return if($$self{flags});

  my $elem_names  = $$self{elem_names};
  my $elem_values = $$self{elem_values};

  my $num_elements = scalar(@$elem_values);
  return if($num_elements == 0);

  my $first = $$elem_values[0];
  return if($first !~ /^-?[0-9]+$/);

  my $prev = $first;

  # Continuous?  (Aliases to prior enum values are allowed.)
  foreach my $value (@$elem_values)
  {
    return if(($value < $first) || ($value > $prev + 1));
    $prev = $value;
  }

  # This point is reached only if the values are a continuous range.
  # 1) Let's kill all the superfluous values, for better readability.
  # 2) Substitute aliases to prior enum values.

  my %aliases = ();

  for(my $i = 0; $i < $num_elements; ++$i)
  {
    my $value = \$$elem_values[$i];
    my $alias = \$aliases{$$value};

    if(defined($$alias))
    {
      $$value = $$alias;
    }
    else
    {
      $$alias = $$elem_names[$i];
      $$value = "" unless($first != 0 && $$value == $first);
    }
  }
}

sub build_element_list($$$$)
{
  my ($self, $ref_flags, $ref_no_gtype, $indent) = @_;

  my @subst_in  = [];
  my @subst_out = [];

  # Build a list of custom substitutions, and recognize some flags too.

  foreach(@$ref_flags)
  {
    if(/^\s*(NO_GTYPE)\s*$/)
    {
      $$ref_no_gtype = $1;
    }
    elsif(/^\s*(get_type_func=)(\s*)\s*$/)
    {
      my $part1 = $1;
      my $part2 = $2;
    }
    elsif(/^\s*s#([^#]+)#([^#]*)#\s*$/)
    {
      push(@subst_in,  $1);
      push(@subst_out, $2);
    }
    elsif($_ !~ /^\s*$/)
    {
      return undef;
    }
  }

  my $elem_names  = $$self{elem_names};
  my $elem_values = $$self{elem_values};

  my $num_elements = scalar(@$elem_names);
  my $elements = "";

  for(my $i = 0; $i < $num_elements; ++$i)
  {
    my $name  = $$elem_names[$i];
    my $value = $$elem_values[$i];

    for(my $ii = 0; $ii < scalar(@subst_in); ++$ii)
    {
      $name  =~ s/${subst_in[$ii]}/${subst_out[$ii]}/;
      $value =~ s/${subst_in[$ii]}/${subst_out[$ii]}/;
    }

    $elements .= "${indent}${name}";
    $elements .= " = ${value}" if($value ne "");
    $elements .= ",\n" if($i < $num_elements - 1);
  }

  return $elements;
}

sub dump($)
{
  my ($self) = @_;

  print "<enum module=\"$$self{module}\" type=\"$$self{type}\" flags=$$self{flags}>\n";

  my $elem_names  = $$self{elem_names};
  my $elem_values = $$self{elem_values};

  for(my $i = 0; $i < scalar(@$elem_names); ++$i)
  {
    print "  <element name=\"$$elem_names[$i]\"  value=\"$$elem_values[$i]\"/>\n";
  }

  print "</enum>\n\n";
}

1; # indicate proper module load.
