#! e:/program files/perl/bin/perl.exe
#  version info can be found in 'TO BE CONFIRMED'

require "../gnu-windows/src/local-paths.lib";

$msvc_mixbus_version = "5.00.0";
$major = 5;
$minor = 0;
$micro = 0;
$interface_age = 0;
$scanner_app_version = "2.00.0";
$scanner_major = 2;
$scanner_minor = 0;
$scanner_micro = 0;
$scanner_interface_age = 0;
$binary_age = 5000;
$current_minus_age = 0;
$exec_prefix = "lib";
$dll_suffix = "32";
$lib_ext = ".dll";
$monospace = "ArdourMono";
$font_small = 8.5;
$font_smaller = 8;
$font_normal = 9;
$font_big = 11;
$font_large = 14.5;
$font_larger = 19;
$font_huger = 27;
$font_massive = 48;

sub process_file
{
        my $outfilename = shift;
	my $infilename = $outfilename . ".in";
	
	open (INPUT, "< $infilename") || exit 1;
	open (OUTPUT, "> $outfilename") || exit 1;
	
	while (<INPUT>) {
	    s/\@GLIB_API_VERSION@/$glib_api_version/g;
	    s/\@GTK_API_VERSION@/$gtk_api_version/g;
	    s/\@ATK_API_VERSION@/$atk_api_version/g;
	    s/\@PANGO_API_VERSION@/$pango_api_version/g;
	    s/\@GDK_PIXBUF_API_VERSION@/$gdk_pixbuf_api_version/g;
	    s/\@MSVC_MIXBUS_VERSION@/$msvc_mixbus_version/g;
	    s/\@MSVC_MIXBUS_MAJOR\@/$major/g;
	    s/\@MSVC_MIXBUS_MINOR\@/$minor/g;
	    s/\@MSVC_MIXBUS_MICRO\@/$micro/g;
	    s/\@MSVC_MIXBUS_INTERFACE_AGE\@/$interface_age/g;
	    s/\@MSVC_SCANNER_APP_VERSION@/$scanner_app_version/g;
	    s/\@MSVC_SCANNER_APP_MAJOR\@/$scanner_major/g;
	    s/\@MSVC_SCANNER_APP_MINOR\@/$scanner_minor/g;
	    s/\@MSVC_SCANNER_APP_MICRO\@/$scanner_micro/g;
	    s/\@MSVC_SCANNER_APP_INTERFACE_AGE\@/$scanner_interface_age/g;
	    s/\@LT_CURRENT_MINUS_AGE@/$current_minus_age/g;
	    s/\@VERSION@/$msvc_mixbus_version/g;
	    s/\@DLL_SUFFIX\@/$dll_suffix/g;
	    s/\@LIB_EXT\@/$lib_ext/g;
	    s/\@MONOSPACE\@/$monospace/g;
	    s/\@FONT_SMALL\@/$font_small/g;
	    s/\@FONT_SMALLER\@/$font_smaller/g;
	    s/\@FONT_NORMAL\@/$font_normal/g;
	    s/\@FONT_BIG\@/$font_big/g;
	    s/\@FONT_LARGE\@/$font_large/g;
	    s/\@FONT_LARGER\@/$font_larger/g;
	    s/\@FONT_HUGER\@/$font_huger/g;
	    s/\@FONT_MASSIVE\@/$font_massive/g;
	    s/\@GETTEXT_PACKAGE\@/$gettext_package/g;
	    s/\@PERL_PATH@/$perl_path/g;
	    s/\@PackagerFolderLocal@/$packager_folder_local/g;
	    s/\@JackBuildRootFolder@/$jack_build_root_folder/g;
	    s/\@GlibBuildRootFolder@/$glib_build_root_folder/g;
	    s/\@GdkPixbufBuildRootFolder@/$gdk_pixbuf_build_root_folder/g;
	    s/\@GtkBuildProjectFolder@/$gtk_build_project_folder/g;
	    s/\@GenericIncludeFolder@/$generic_include_folder/g;
	    s/\@GenericLibraryFolder@/$generic_library_folder/g;
	    s/\@GenericWin32LibraryFolder@/$generic_win32_library_folder/g;
	    s/\@GenericWin32BinaryFolder@/$generic_win32_binary_folder/g;
	    s/\@Debug32TestSuiteFolder@/$debug32_testsuite_folder/g;
	    s/\@Release32TestSuiteFolder@/$release32_testsuite_folder/g;
	    s/\@Debug32TargetFolder@/$debug32_target_folder/g;
	    s/\@Release32TargetFolder@/$release32_target_folder/g;
	    s/\@Debug32PixbufLoadersFolder@/$debug32_pixbuf_loaders_folder/g;
	    s/\@Release32PixbufLoadersFolder@/$release32_pixbuf_loaders_folder/g;
	    s/\@TargetSxSFolder@/$target_sxs_folder/g;
	    s/\@prefix@/$prefix/g;
	    s/\@exec_prefix@/$exec_prefix/g;
	    s/\@includedir@/$generic_include_folder/g;
	    s/\@libdir@/$generic_library_folder/g;
	    print OUTPUT;
	}
}

process_file ("libs/plugins/reasonablesynth.lv2/manifest.ttl");
process_file ("gtk2_ardour/default_ui_config");

my $command=join(' ',@ARGV);
if ($command eq -buildall) {
	process_file ("MSVCardour3/MSVCArdour5.vsprops");
	process_file ("MSVCMixbus3/MSVCMixbus5.vsprops");
	process_file ("MSVCvst_scan/vst_scan.rc");
	process_file ("MSVCvst_scan/vst3_scan.rc");
	process_file ("gtk2_ardour/win32/msvc_resources.rc");
}
