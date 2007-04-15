package ARDOUR::AutomationSRConverter;

sub new {
	my ($type, $input, $output, $inputSR, $outputSR) = @_;

	my $self = bless {}, $type;

	$self->{Input}  = $input;
	$self->{Output} = $output;
	$self->{Ratio}  = ($outputSR+0) / ($inputSR+0);

	return $self;
}

sub readline {
	my ($self) = @_;

	my $buf;
	my $c='';
	
	do {
		$buf.=$c;
		$c=$self->{Input}->getc;
	} while ($c ne '' && $c ne "\n");
	
	return $buf;
}

sub writeline {
	my ($self, $line) = @_;

	$self->{Output}->print($line."\n");
}

sub convert {
	my ($self) = @_;

	my $version=$self->readline;

	if ($version ne "version 1") {
		die ("Unsupported automation version $version");
	}

	$self->writeline($version);

	my $buf = $self->readline;
	while ( $buf ne "" ) {
		if ( $buf eq "begin" ||
		     $buf eq "end") {
			$self->writeline($buf);
		} else {
			my ($type, $position, $value) = split(' ', $buf);
		
			$self->writeline($type." ".sprintf("%.0f",$position*$self->{Ratio})." ".$value);
		}

		$buf = $self->readline;
	}
}

1;
