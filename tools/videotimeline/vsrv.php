<?php
$exe_ffmpeg='ffmpeg';
$exe_ffprobe='ffprobe';
$exe_imagemagick='convert';
$docroot='/'; # must be identical to ardour3->Edit->Preferences->Video->Docroot

$mode = '';
if (isset($_SERVER['PATH_INFO'])) {
	switch($_SERVER['PATH_INFO']) {
		case '/status/':
		case '/status':
			echo 'status: ok, online.';
			exit;
			break;
		case '/info/':
		case '/info':
			$mode='info';
			break;
		default:
			break;
	}
}

$infile='/tmp/test.avi';
$w=80; $h=60; $f=0;
$fr=0; $df=0;
$so=0; $ar=4.0/3.0;
$fmt='rgb';

if (isset($_REQUEST['format'])) {
	switch ($_REQUEST['format']) {
	case 'jpeg':
	case 'jpg':
		$fmt='jpeg';
		break;
	case 'png':
		$fmt='png';
		break;
	case 'rgba':
		$fmt='rgba';
		break;
	case 'rgb':
		$fmt='rgb';
		break;
	default:
		break;
	}
}

if (isset($_REQUEST['w']))
  $w=intval(rawurldecode($_REQUEST['w']));
if (isset($_REQUEST['h']))
  $h=intval(rawurldecode($_REQUEST['h']));
if (isset($_REQUEST['frame']))
  $f=intval(rawurldecode($_REQUEST['frame']));
if (isset($_REQUEST['file']))
  $infile=rawurldecode($_REQUEST['file']);

if (!is_readable($docroot.$infile)) {
	header('HTTP/1.0 404 Not Found', true, 404);
	exit;
}

$fn=escapeshellarg($docroot.$infile);

#$fr=`$exe_ffprobe $fn 2>&1 | awk '/Video:/{printf "%f\\n", $11}'`;
$nfo=shell_exec("$exe_ffprobe $fn 2>&1");
if (preg_match('@Video:.* ([0-9.]+) tbr,@m',$nfo, $m))
	$fr=floatval($m[1]);
if ($fr<1) exit;

if (preg_match('@Duration: ([0-9:.]+),@m',$nfo, $m)) {
	$d=preg_split('@[\.:]@',$m[1]);
	$dr=0;
	$dr+=intval($d[0])*3600;
	$dr+=intval($d[1])*60;
	$dr+=intval($d[2]);
	$dr+=floatval($d[3]) / pow(10,strlen($d[3]));
	$df=$fr*$dr;
}
if (preg_match('@start: ([0-9:.]+),@m',$nfo, $m)) {
	$so=floatval($m[1]);
}
if (preg_match('@DAR ([0-9:]+)\]@m',$nfo, $m)) {
	$d=explode(':',$m[1]);
	$ar=floatval($d[0]/$d[1]);
}
else if (preg_match('@Video:.* ([0-9]+x[0-9]+),@m',$nfo, $m)) {
	$d=explode('x',$m[1]);
	$ar=floatval($d[0]/$d[1]);
}

if ($mode=='info') {
	# Protocol Version number
	# FPS
	# duration (in frames)
	# start-offset (in seconds)
	# aspect-ratio
	echo "1\n$fr\n$df\n$so\n$ar\n";
	exit;
}

if ($df<1 || $f>$df ) exit;
$st=floor(1000.0*$f/$fr)/1000.0;

$wh=escapeshellarg($w.'x'.$h);
$ss=escapeshellarg($st);

header('Content-Type: image/'.$fmt);
passthru($exe_ffmpeg.' -loglevel quiet'
	      .' -i '.$fn.' -s '.$wh.' -ss '.$ss.' -vframes 1 '
        .'-f image2 -vcodec png - 2>/dev/null'
        .'| '.$exe_imagemagick.' - '.$fmt.':-');
