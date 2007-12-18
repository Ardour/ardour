package Property;

use strict;
use warnings;

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

# class Property
#    {
#       string name;
#       string class;
#       string type;
#       bool readable;
#       bool writable;
#       bool construct_only;
#       string docs;
#    }


sub new
{
  my ($def) = @_;
  my $self = {};
  bless $self;

  $def=~s/^\(//;
  $def=~s/\)$//;
  # snarf down the fields
  $$self{mark} = 0;
  $$self{name} = $1                     if ($def =~ s/^define-property (\S+)//);
  $$self{class} = $1                    if ($def =~ s/\(of-object "(\S+)"\)//);
  $$self{type} = $1                     if ($def =~ s/\(prop-type "(\S+)"\)//);
  $$self{readable} = ($1 eq "#t")       if ($def =~ s/\(readable (\S+)\)//);
  $$self{writable} = ($1 eq "#t")       if ($def =~ s/\(writable (\S+)\)//);
  $$self{construct_only} = ($1 eq "#t") if ($def =~ s/\(construct-only (\S+)\)//);
  
  # Property documentation:
  my $propertydocs = $1                     if ($def =~ s/\(docs "([^"]*)"\)//);
  # Add a full-stop if there is not one already:
  if(defined($propertydocs))
  {
    my $docslen = length($propertydocs);
    if($docslen)
    {
      if( !(substr($propertydocs, $docslen - 1, 1) eq ".") )
      {
        $propertydocs = $propertydocs . ".";
      }
    }
  }
  
  $$self{docs} = $propertydocs;
  

  $$self{name} =~ s/-/_/g; # change - to _

  GtkDefs::error("Unhandled property def ($def) in $$self{class}\::$$self{name}\n")
    if ($def !~ /^\s*$/);

  return $self;
}

sub dump($)
{
  my ($self) = @_;

  print "<property>\n";

  foreach (keys %$self)
  { print "  <$_ value=\"$$self{$_}\"/>\n"; }

  print "</property>\n\n";
}

sub get_construct_only($)
{
  my ($self) = @_;
  return $$self{construct_only};
}

sub get_type($)
{
  my ($self) = @_;
  return $$self{type};
}

sub get_readable($)
{
  my ($self) = @_;
  return $$self{readable};
}

sub get_writable($)
{
  my ($self) = @_;
  return $$self{writable};
}

sub get_docs($)
{
  my ($self) = @_;
  return $$self{docs};
}


1; # indicate proper module load.
