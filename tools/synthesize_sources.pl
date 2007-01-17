#!/usr/bin/env perl
# Ardour session synthesizer
# (c)Sampo Savolainen 2007
#
# GPL
# This reads an Ardour session file and creates zero-signal source files
# for each missing source file. The length of each file is determined
# by how far regions using that source file go into the sample data.

use XML::Parser::PerlSAX;
use XML::Handler::XMLWriter;
use IO::Handle;

use ARDOUR::SourceInfoLoader;


my ($samplerate, $sessionName) = @ARGV;

if ( ! -d $sessionName || ! -f $sessionName."/".$sessionName.".ardour" ) {
	print "usage: synthesize_sources.pl samplerate [session name, the name must match the directory and the .ardour file in it]\n";
	exit;
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
	
	print "Generating ".$audioFileDirectory."/".$sources{$tmp}->{name}.".wav\n";

	system("sox", 
	       "-t", "raw",        # /dev/zero is raw :)
	       "-r", $samplerate,  # set sample rate
	       "-c", "1",	   # 1 channel
	       "-b",		   # input in bytes
	       "-s",               # signed
	       "/dev/zero",        # input signal

	       "-w",               # output 16 bit
	       "-t", "wav",        # format wav
	       $audioFileDirectory."/".$sources{$tmp}->{name}, # filename
	       "trim", "0", $sources{$tmp}->{calculated_length}."s" # trim silence to wanted sample amount
	       );


}



