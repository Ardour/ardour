#!/usr/bin/env perl
# Ardour session synthesizer
# (c)Sampo Savolainen 2007-2008
#
# GPL
# This reads an Ardour session file and creates zero-signal source files
# for each missing source file. The length of each file is determined
# by how far regions using that source file go into the sample data.

use FindBin '$Bin';
use lib "$Bin";
use XML::Parser::PerlSAX;
use XML::Handler::XMLWriter;
use IO::Handle;

use ARDOUR::SourceInfoLoader;

my $usage = "usage: synthesize_sources.pl samplerate [session name, the name must match the directory and the .ardour file in it] [options: -sine for 440hz sine waves in wave files]\n";

my ($samplerate, $sessionName, @options) = @ARGV;

if ( ! -d $sessionName || ! -f $sessionName."/".$sessionName.".ardour" ) {
	print $usage;
	exit;
}

my $waveType = "silent";

foreach my $o (@options) {
	if ($o eq "-sine") {
		$waveType = "sine";
	} elsif ($o eq "-silent") {
		$waveType = "silent";
	} else {
		print "unknown parameter ".$o."\n";
		print $usage;
		exit;

	}
	
}

my $sessionFile = $sessionName."/".$sessionName.".ardour";


my $handler = new ARDOUR::SourceInfoLoader($sessionName);

my $parser = XML::Parser::PerlSAX->new( Handler => $handler );

$parser->parse(Source => { SystemId => $sessionFile });

if ( ! -d $sessionName."/interchange" ) {
	mkdir $sessionName."/interchange/" || die "couldn't create ".$sessionName."/interchange";
}

if ( ! -d $sessionName."/interchange/".$sessionName ) {
	mkdir $sessionName."/interchange/".$sessionName || die "couldn't create ".$sessionName."/interchange/".$sessionName;
}

if ( ! -d $sessionName."/interchange/".$sessionName."/audiofiles" ) {
	mkdir $sessionName."/interchange/".$sessionName."/audiofiles" || die "couldn't create ".$sessionName."/interchange/".$sessionName."/audiofiles";
}

if ( ! -d $sessionName."/peaks") {
	mkdir $sessionName."/peaks/" || die "couldn't create ".$sessionName."/peaks";
}

my $audioFileDirectory = $sessionName."/interchange/".$sessionName."/audiofiles";

my %sources = %{$handler->{Sources}};

foreach my $tmp (keys %sources) {
	
	print "Generating ".$audioFileDirectory."/".$sources{$tmp}->{name}."\n";

	my @cmd = 
              ("sox", 
	       "-t", "raw",        # /dev/zero is raw :)
	       "-r", $samplerate,  # set sample rate
	       "-c", "1",	   # 1 channel
	       "-b", "8",	   # input in 8 bit chunks
	       "-s",               # signed
	       "/dev/zero",        # input signal

	       "-b", "16",	   # input in 16 bit chunks
	       "-t", "wav",        # format wav
	       $audioFileDirectory."/".$sources{$tmp}->{name}, # filename
	       "trim", "0", $sources{$tmp}->{calculated_length}."s" # trim silence to wanted sample amount
	       );

	if ($waveType eq "sine") {
		@cmd = (@cmd, "synth","sin","%0", "vol", "0.2", "fade","q","0.01s", $sources{$tmp}->{calculated_length}."s" , "0.01s");
	}

	system(@cmd);
}



