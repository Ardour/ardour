package Function;

use strict;
use warnings;
use Util;
use FunctionBase;

BEGIN {
     use Exporter   ();
     our ($VERSION, @ISA, @EXPORT, @EXPORT_OK, %EXPORT_TAGS);

     # set the version for version checking
     $VERSION     = 1.00;
     @ISA         = qw(FunctionBase);
     @EXPORT      = qw(&func1 &func2 &func4);
     %EXPORT_TAGS = ( );
     # your exported package globals go here,
     # as well as any optionally exported functions
     @EXPORT_OK   = qw($Var1 %Hashit &func3);
     }
our @EXPORT_OK;

##################################################
### Function
# Commonly used algorithm for parsing a function declaration into
# its component pieces
#
#  class Function : FunctionBase
#    {
#       string rettype;
#       bool const;
#       bool static;
#       string name; e.g. gtk_accelerator_valid
#       string c_name;
#       string array param_type;
#       string array param_name;
#       string array param_default_value;
#       string in_module; e.g. Gtk
#       string signal_when. e.g. first, last, or both.
#       string class e.g. GtkButton ( == of-object. Useful for signal because their names are not unique.
#       string entity_type. e.g. method or signal
#    }

sub new_empty()
{
  my $self = {};
  bless $self;

  return $self;
}

# $objFunction new($function_declaration, $objWrapParser)
sub new($$)
{
  #Parse a function/method declaration.
  #e.g. guint gtk_something_set_thing(guint a, const gchar* something)

  my ($line, $objWrapParser) = @_;

  my $self = {};
  bless $self;

  #Initialize member data:
  $$self{rettype} = "";
  $$self{rettype_needs_ref} = 0; #Often the gtk function doesn't do an extra ref for the receiver.
  $$self{const} = 0;
  $$self{name} = "";
  $$self{param_types} = [];
  $$self{param_names} = [];
  $$self{param_default_values} = [];
  $$self{in_module} = "";
  $$self{class} = "";
  $$self{entity_type} = "method";

  $line =~ s/^\s+//;  # Remove leading whitespace.
  $line =~ s/\s+/ /g; # Compress white space.

  if ($line =~ /^static\s+([^()]+)\s+(\S+)\s*\((.*)\)\s*$/)
  {
    $$self{rettype} = $1;
    $$self{name} = $2;
    $$self{c_name} = $2;
    $self->parse_param($3);
    $$self{static} = 1;
  }
  elsif ($line =~ /^([^()]+)\s+(\S+)\s*\((.*)\)\s*(const)*$/)
  {
    no warnings qw(uninitialized); # disable the uninitialize warning for $4
    $$self{rettype} = $1;
    $$self{name} = $2;
    $$self{c_name} = $2;
    $self->parse_param($3);
    $$self{const} = ($4 eq "const");
  }
  else
  {
    $objWrapParser->error("fail to parse $line\n");
  }

  return $self;
}


# $objFunction new_ctor($function_declaration, $objWrapParser)
# Like new(), but the function_declaration doesn't need a return type.
sub new_ctor($$)
{
  #Parse a function/method declaration.
  #e.g. guint gtk_something_set_thing(guint a, const gchar* something)

  my ($line, $objWrapParser) = @_;

  my $self = {};
  bless $self;

  #Initialize member data:
  $$self{rettype} = "";
  $$self{rettype_needs_ref} = 0;
  $$self{const} = 0;
  $$self{name} = "";
  $$self{param_types} = [];
  $$self{param_names} = [];
  $$self{param_default_values} = [];
  $$self{in_module} = "";
  $$self{class} = "";
  $$self{entity_type} = "method";

  $line =~ s/^\s+//;  # Remove leading whitespace.
  $line =~ s/\s+/ /g; # Compress white space.

  if ($line =~ /^(\S+)\s*\((.*)\)\s*/)
  {
    $$self{name} = $1;
    $$self{c_name} = $2;
    $self->parse_param($2);
  }
  else
  {
    $objWrapParser->error("fail to parse $line\n");
  }

  return $self;
}

# $num num_args()
sub num_args #($)
{
  my ($self) = @_;
  my $param_types = $$self{param_types};
  return $#$param_types+1;
}

# parses C++ parameter lists.
# forms a list of types, names, and initial values
#  (we don't currently use values)
sub parse_param($$)
{
  my ($self, $line) = @_;


  my $type = "";
  my $name = "";
  my $value = "";
  my $id = 0;
  my $has_value = 0;

  my $param_types = $$self{param_types};
  my $param_names = $$self{param_names};
  my $param_default_values = $$self{param_default_values};

  # clean up space and handle empty case
  $line = string_trim($line);
  $line =~ s/\s+/ /g; # Compress whitespace.
  return if ($line =~ /^$/);

  # parse through argument list
  my @str = ();
  my $par = 0;
  foreach (split(/(const )|([,=&*()])|(<[^,]*>)|(\s+)/, $line)) #special characters OR <something> OR whitespace.
  {
    next if ( !defined($_) or $_ eq "" );
      
    if ( $_ eq "(" ) #Detect the opening bracket.
    {
       push(@str, $_);
       $par++; #Increment the number of parameters.
       next;
    }
    elsif ( $_ eq ")" )
    {
       push(@str, $_);
       $par--; #Decrement the number of parameters.
       next;
    }
    elsif ( $par || /^(const )|(<[^,]*>)|([*&])|(\s+)/ ) #TODO: What's happening here?
    {
      push(@str, $_); #This looks like part of the type, so we store it.
      next;
    }
    elsif ( $_ eq "=" ) #Default value
    {
      $type = join("", @str); #The type is everything before the = character.
      @str = (); #Wipe it so that it will only contain the default value, which comes next.
      $has_value = 1;
      next;
    }
    elsif ( $_ eq "," ) #The end of one parameter:
    {
      if ($has_value)
      {
        $value = join("", @str); # If there's a default value, then it's the part before the next ",".
      }
      else
      {
        $type = join("", @str);
      }

      if ($name eq "")
      {
        $name = sprintf("p%s", $#$param_types + 2)
      }

      $type = string_trim($type);

      push(@$param_types, $type);
      push(@$param_names, $name);
      push(@$param_default_values, $value);
      
      #Clear variables, ready for the next parameter.
      @str = ();
      $type= "";
      $value = "";
      $has_value = 0;
      $name = "";

      $id = 0;

      next;
    }

    if ($has_value)
    {
      push(@str, $_);
      next;
    }

    $id++;
    $name = $_ if ($id == 2);
    push(@str, $_) if ($id == 1);

    if ($id > 2)
    {
      print STDERR "Can't parse $line.\n";
      print STDERR "  arg type so far: $type\n";
      print STDERR "  arg name so far: $name\n";
      print STDERR "  arg default value so far: $value\n";
    }
  }

  # handle last argument  (There's no , at the end.)
  if ($has_value)
  {
    $value = join("", @str);
  }
  else
  {
    $type = join("", @str);
  }

  if ($name eq "")
  {
    $name = sprintf("p%s", $#$param_types + 2)
  }

  $type = string_trim($type);

  push(@$param_types, $type);
  push(@$param_names, $name);
  push(@$param_default_values, $value);
}

# add_parameter_autoname($, $type, $name)
# Adds e.g "sometype somename"
sub add_parameter_autoname($$)
{
  my ($self, $type) = @_;

  add_parameter($self, $type, "");
}

# add_parameter($, $type, $name)
# Adds e.g GtkSomething* p1"
sub add_parameter($$$)
{
  my ($self, $type, $name) = @_;
  $type = string_unquote($type);
  $type =~ s/-/ /g;

  my $param_names = $$self{param_names};

  if ($name eq "")
  {
    $name = sprintf("p%s", $#$param_names + 2);
  }

  push(@$param_names, $name);

  my $param_types = $$self{param_types};

  push(@$param_types, $type);

  return $self;
}

# $string get_refdoc_comment()
# Generate a readable prototype for signals.
sub get_refdoc_comment($)
{
  my ($self) = @_;

  my $str = "  /**\n";

  $str .= "   * \@par Prototype:\n";
  $str .= "   * <tt>$$self{rettype} on_my_\%$$self{name}(";

  my $param_names = $$self{param_names};
  my $param_types = $$self{param_types};
  my $num_params  = scalar(@$param_types);

  # List the parameters:
  for(my $i = 0; $i < $num_params; ++$i)
  {
    $str .= $$param_types[$i] . ' ' . $$param_names[$i];
    $str .= ", " if($i < $num_params - 1);
  }

  $str .= ")</tt>\n";
  $str .= "   */";

  return $str;
}

sub get_is_const($)
{
  my ($self) = @_;

  return $$self{const};
}

1; # indicate proper module load.

