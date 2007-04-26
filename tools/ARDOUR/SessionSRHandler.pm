package ARDOUR::SessionSRHandler;


use XML::Handler::XMLWriter;
use POSIX qw(floor);

@ISA = qw( XML::Handler::XMLWriter );

$VERSION = 0.1;

# This table maps the "names of XML elements" to lists of "names of attributes" which should
# be converted according to the SR change.
# TODO: The table is a bit dirty, i have to figure out how to do it cleanly
my $conversion_table = {
	"Location"  => { "end" => 0, "start" => 0 },
	"Region"    => { "length" => 0, "start" => 0, "position" => 0, "sync-position" => 0 }, 
	"Crossfade" => { "left" => 0, "right" => 0 }
	};


sub new {
    my ($type, $original_sr, $new_sr, $output) = @_;

	#my $self = XML::Handler::XMLWriter->new( { Output => $output } );
	
	my $self = $type->SUPER::new( Output => $output );
	
	$self->{Debug} = 0;
	$self->{Ratio} = ($new_sr+0)/($original_sr+0);
	$self->{OriginalSR} = $original_sr;
	$self->{TargetSR} = $new_sr;
	$self->{Output} = $output;
	
	$self->{InEvents} = 0;

	return $self;
}

sub start_element {
	my $self = shift;
	my $element = shift;

 	my $debug = $self->{Debug};

	my $atts = $element->{Attributes};

	my $convert_attributes = $conversion_table->{$element->{Name}};

	foreach my $cAtt (keys %$convert_attributes) {
		$atts->{$cAtt} = sprintf("%.0f", $atts->{$cAtt} * $self->{Ratio});
		$debug = 0;
	}

	if ($debug eq 0) {
		$self->SUPER::start_element($element, @_);
	}

	if ($element->{Name} eq "events") {
		$self->{InEvents} = 1;
	}
}

sub end_element {
	my $self = shift;
	my $element = shift;

	if ($self->{Debug} eq 0) {
		$self->SUPER::end_element($element,@_);
	}

	if ($element->{Name} eq "events") {
		$self->{InEvents} = 0;
	}
}

sub start_document {
	my $self = shift;

	$self->SUPER::start_document(@_);
	
	$self->{Output}->print("<!-- Sample rate converted from $self->{OriginalSR}hz to $self->{TargetSR}hz -->\n");
}

sub end_document {
	my $self = shift;

	$self->SUPER::end_document(@_);
}

sub characters {
	my $self = shift;
	my $c = shift;

	if ($self->{InEvents} > 0) {
		my $converted = "";

		foreach my $foo (split(' ',$c->{Data})) {
			if ($self->{InEvents} eq 1) {
				$converted .= floor($foo * $self->{Ratio})." ";
				$self->{InEvents} = 2;
			} else {
				$converted .= $foo." ";
				$self->{InEvents} = 1;
			}
		}

		$c->{Data} = $converted;
	}


	if ($self->{Debug} eq 0) {
		$self->SUPER::characters($c, @_);
	}

}

1;



