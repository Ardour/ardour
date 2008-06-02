package Object;

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

# class Object
# {
#   string name;
#   string module;
#   string parent;
#   string c_name;
#   string gtype_id;
# }


sub new
{
  my ($def) = @_;

  my $self = {};
  bless $self;

  $def =~ s/^\(//;
  $def =~ s/\)$//;

  # snarf down the fields
  $$self{name}     = $1 if($def =~ s/^define-object (\S+)//);
  $$self{module}   = $1 if($def =~ s/\(in-module "(\S+)"\)//);
  $$self{parent}   = $1 if($def =~ s/\(parent "(\S+)"\)//);
  $$self{c_name}   = $1 if($def =~ s/\(c-name "(\S+)"\)//);
  $$self{gtype_id} = $1 if($def =~ s/\(gtype-id "(\S+)"\)//);

  if($def !~ /^\s*$/)
  {
    GtkDefs::error("Unhandled object def ($def) in $$self{module}\::$$self{name}\n")
  }

  return $self;
}


sub dump($)
{
  my ($self) = @_;

  print "<object>\n";

  foreach(keys %$self)
    { print "  <$_ value=\"$$self{$_}\"/>\n"; }

  print "</object>\n\n";
}


1; # indicate proper module load.
