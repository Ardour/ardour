#include "filesystem_test.h"

#include <unistd.h>
#include <stdlib.h>

#include <glibmm/miscutils.h>

#include "pbd/file_utils.h"

#include "test_common.h"

using namespace std;
using namespace PBD;

CPPUNIT_TEST_SUITE_REGISTRATION (FilesystemTest);

void
FilesystemTest::testPathIsWithin ()
{
#ifndef PLATFORM_WINDOWS
	system ("rm -r foo");
	CPPUNIT_ASSERT (g_mkdir_with_parents ("foo/bar/baz", 0755) == 0);

	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar/baz", "foo/bar/baz"));
	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar", "foo/bar/baz"));
	CPPUNIT_ASSERT (PBD::path_is_within ("foo", "foo/bar/baz"));
	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar", "foo/bar/baz"));
	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar", "foo/bar"));

	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar/baz", "frobozz") == false);

	int const r = symlink ("bar", "foo/jim");
	CPPUNIT_ASSERT (r == 0);

	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar/baz", "foo/bar/baz"));
	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar", "foo/bar/baz"));
	CPPUNIT_ASSERT (PBD::path_is_within ("foo", "foo/bar/baz"));
	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar", "foo/bar/baz"));
	CPPUNIT_ASSERT (PBD::path_is_within ("foo/bar", "foo/bar"));

	CPPUNIT_ASSERT (PBD::path_is_within ("foo/jim/baz", "frobozz") == false);
#endif
}

void
FilesystemTest::testCopyFileASCIIFilename ()
{
	string testdata_path;
	CPPUNIT_ASSERT (find_file (test_search_path (), "RosegardenPatchFile.xml", testdata_path));

	string output_path = test_output_directory ("CopyFile");

	output_path = Glib::build_filename (output_path, "RosegardenPatchFile.xml");

	cerr << endl;
	cerr << "CopyFile test output path: " << output_path << endl;

	CPPUNIT_ASSERT (PBD::copy_file (testdata_path, output_path));
}

void
FilesystemTest::testCopyFileUTF8Filename ()
{
	vector<string> i18n_files;

	Searchpath i18n_path(test_search_path());
	i18n_path.add_subdirectory_to_paths("i18n_test");

	PBD::find_files_matching_pattern (i18n_files, i18n_path, "*.tst");

	cerr << endl;
	cerr << "Copying " << i18n_files.size() << " test files from: "
	     << i18n_path.to_string () << endl;

	for (vector<string>::iterator i = i18n_files.begin(); i != i18n_files.end(); ++i) {
		string input_path = *i;
		string output_file = Glib::path_get_basename(*i);
		string output_path = test_output_directory ("CopyFile");
		output_path = Glib::build_filename (output_path, output_file);

		cerr << "Copying test file: " << input_path
		     << " To " << output_path << endl;

		CPPUNIT_ASSERT (PBD::copy_file (input_path, output_path));
	}
}
