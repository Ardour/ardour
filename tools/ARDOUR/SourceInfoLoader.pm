package ARDOUR::SourceInfoLoader;


use XML::Handler::Subs;

@ISA = qw( XML::Handler::Subs );

$VERSION = 1.0;


sub new {
	my ($type, $sessionName) = @_;

	my $self = $type->SUPER::new();
	
	$self->{SessionName} = $sessionName;
	$self->{InRegions} = 0;
	%self->{Sources} = {};

	
	return $self;
}

sub start_element {
	my $self = shift;
	my $element = shift;

	my $atts = $element->{Attributes};

	if ( $element->{Name} eq "Source") {
		if ( ! -f "interchange/".$sessionName."/audiofiles/".$atts->{name}) {
			$atts->{calculated_length} = 1;
			$self->{Sources}->{$atts->{id}} = $atts;
		}
	}

	
	if ( $self->{InRegions} eq 1 && $element->{Name} eq "Region") {
		#print "Looking at region ".$atts->{id}."\n";
		my $num = 0;

		my $region_length = $atts->{length};
		while ( $atts->{"source-".$num} ne "" ) {

			if ($region_length > $self->{Sources}->{$atts->{"source-".$num}}->{calculated_length} ) {
				$self->{Sources}->{$atts->{"source-".$num}}->{calculated_length} = $region_length;
			}

			$num++;
		}
	}

	if ( $element->{Name} eq "Regions") {
		$self->{InRegions} = 1;
		#print "In regions\n";
	}


}

sub end_element {
	my $self = shift;
	my $element = shift;

	if ( $element->{Name} eq "Regions") {
		$self->{InRegions} = 0;
		#print "Out of regions\n";
	}

}

1;



