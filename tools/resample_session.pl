#!/usr/bin/env perl
# Ardour session resampler, version 0.1
# (c)Sampo Savolainen 2005-2007
#
# Licensed under the GPL 
#
# Copies the session to another directory and changes it's sampling rate in all respects.
# The frames in .ardour and .automation files are converted according to the conversion ration.
# The peakfiles and dead_sounds aren't copied. Only "identified" files are copied, instant.xml's
# or .bak's aren't copied either.

use FindBin '$Bin';
use lib "$Bin";
use XML::Parser::PerlSAX;
use XML::Handler::XMLWriter;
use IO::Handle;

use File::Basename;

use ARDOUR::SessionSRHandler;
# Let's hope this is never needed
#use ARDOUR::AutomationSRConverter;


if ( ! -f "/usr/bin/sndfile-resample" &&
     ! -f "/usr/local/bin/sndfile-resample") {
	print "You don't have sndfile-resample installed. This script will not work without it, please install it and try again\n";
	exit;
}

my ($sourceDirectory, $destDirectory, $sourceSR, $destSR) = @ARGV;

if ((@ARGV+0) ne 4 ||
    ($sourceSR+0) eq 0 ||
	($destSR+0)   eq 0) {
	print "usage: ardour_sr.pl [ardour session directory] [destination directory] [original samplerate] [new samplerate]\n";
	exit;
}

if ( ! -d $sourceDirectory) {
	print $sourceDirectory.": directory does not exist!\n";
	exit;
}

if ( -d $destDirectory) {
	print $destDirectory.": directory exists!\n";
	exit;
}

print "Checking source and destination directories\n";

my @sounds;
my @dead_sounds;
my @dot_ardour;
my @automation;


my $version_099x = 0;

# Read the names of all audio files in /sounds/
if ( -d $sourceDirectory."/sounds/") {
	$version_099x = 1;

	opendir(SOUNDS,$sourceDirectory."/sounds/") || die ($sourceDirectory.": not a valid session, no sounds/ directory");
	while ( my $file=readdir(SOUNDS) ) {
		if ( -f $sourceDirectory."/sounds/".$file ) {
			push(@sounds,$file);
		}
	}

} else {
	my $dirname = $sourceDirectory."/interchange/".basename($sourceDirectory)."/audiofiles/";
	opendir(SOUNDS,$dirname) || die ($sourceDirectory.": not a valid session, no sounds/ directory");
	while ( my $file=readdir(SOUNDS) ) {
		if ( -f $dirname.$file ) {
			push(@sounds,$file);
		}
	}

}
close(SOUNDS);

# Read the names of all audio files in /dead_sounds/
opendir(DEAD_SOUNDS,$sourceDirectory."/dead_sounds/") || die ($sourceDirectory.": not a valid session, no dead_sounds/ directory");
while ( my $file=readdir(DEAD_SOUNDS) ) {
	if ( -f $sourceDirectory."/dead_sounds/".$file ) {
		push(@dead_sounds,$file);
	}
}
close(DEAD_SOUNDS);

# Read the names of all .ardour files in /
opendir(DOT_ARDOUR,$sourceDirectory) || die ($sourceDirectory.": could not open!");
while ( my $file=readdir(DOT_ARDOUR) ) {
	if ( -f $sourceDirectory."/".$file && 
	     index($file,".ardour") eq (length($file)-7)) {
		push(@dot_ardour,$file);
	}
}
close(DOT_ARDOUR);

if ( -d $sourceDirectory."/automation/") {
	# Read the names of all automation files in /automation/
	opendir(AUTOMATION,$sourceDirectory."/automation/") || die ($sourceDirectory."/automation: could not open!");
	while ( my $file=readdir(AUTOMATION) ) {
		if ( -f $sourceDirectory."/automation/".$file && 
		     index($file,".automation") eq (length($file)-11)) {
			push(@automation,$file);
		}
	}
	close(AUTOMATION);
}

# Check for /peaks/
if ( ! -d $sourceDirectory."/peaks" ) {
	print $sourceDirectory.": not a valid session, no peaks/ directory\n";
	exit;
}

##########################################
# Checks are done, let's go!

print "Converting session\n";
mkdir $destDirectory;


# Run all .ardour files through the SAX parser and write the results in the destination
# directory.

foreach my $xml (@dot_ardour) {
	print "Doing samplerate conversion to ".$xml."...";
	open(OUTFILE,">".$destDirectory."/".$xml);
	my $output = new IO::Handle;
	$output->fdopen(fileno(OUTFILE),"w");
	
	my $handler = ARDOUR::SessionSRHandler->new($sourceSR,$destSR,$output);

	my $parser = XML::Parser::PerlSAX->new( Handler => $handler );

    $parser->parse(Source => { SystemId => $sourceDirectory."/".$xml });

	$output->close();
	close(OUTFILE);
	print " done\n";
}

# This code is needed for 0.99.x sessions, thus the code is still here.
#
#mkdir $destDirectory."/automation";
#
#foreach my $file (@automation) {
#	print "Converting automation file ".$file."...";
#	open(INFILE,$sourceDirectory."/automation/".$file) || die "could not open source automation file $file!";
#	open(OUTFILE,">".$destDirectory."/automation/".$file) || die "could not open destination automation file $file!";
#	my $input=new IO::Handle;
#	my $output=new IO::Handle;
#
#	$input->fdopen(fileno(INFILE),"r");
#	$output->fdopen(fileno(OUTFILE),"w");
#
#	my $converter = ARDOUR::AutomationSRConverter->new($input,$output,$sourceSR,$destSR);
#	$converter->convert;
#
#	$input->close;
#	$output->close;
#	close(INFILE);
#	close(OUTFILE);
#	print " done\n";
#}


if ($version_099x eq 1) {
	mkdir $destDirectory."/sounds";
	foreach my $sound (@sounds) {
		my @params=("-to", $destSR,
	        	    "-c",  "0",
			    $sourceDirectory."/sounds/".$sound,
			    $destDirectory."/sounds/".$sound);
		system("sndfile-resample",@params);
	}
} else {
	my $srcSndDir = $sourceDirectory."/interchange/".basename($sourceDirectory)."/audiofiles/";
	
	my $dstSndDir = $destDirectory."/interchange/";
	mkdir $dstSndDir;

	$dstSndDir .= basename($sourceDirectory)."/";
	mkdir $dstSndDir;

	$dstSndDir .= "audiofiles/";
	mkdir $dstSndDir;
	
	foreach my $sound (@sounds) {
		my @params=("-to", $destSR,
	        	    "-c",  "0",
			    $srcSndDir."/".$sound,
			    $dstSndDir."/".$sound);
		system("sndfile-resample",@params);
	}
}

mkdir $destDirectory."/dead_sounds";
mkdir $destDirectory."/peaks";


