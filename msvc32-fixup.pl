#! e:/program files/perl/bin/perl.exe
#  version info can be found in 'TO BE CONFIRMED'

require "../gnu-windows/src/local-paths.lib";

$msvc_mixbus_version = "3.00.0";
$major = 3;
$minor = 0;
$micro = 0;
$interface_age = 0;
$binary_age = 3000;
$current_minus_age = 0;
$exec_prefix = "lib";
$dll_suffix = "32";

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
	    s/\@LT_CURRENT_MINUS_AGE@/$current_minus_age/g;
	    s/\@VERSION@/$msvc_mixbus_version/g;
	    s/\@DLL_SUFFIX\@/$dll_suffix/g;
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

my $command=join(' ',@ARGV);
if ($command eq -buildall) {
	process_file ("MSVCardour3/MSVCMixbus3.vsprops");
	process_file ("MSVCMixbus3/MSVCMixbus3.vsprops");
	process_file ("icons/win32/msvc_resources.rc");
}
