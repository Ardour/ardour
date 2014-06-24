#include "filesystem_test.h"

#include <unistd.h>
#include <stdlib.h>

#include <glibmm/miscutils.h>

#include "pbd/file_utils.h"

#include "test_common.h"

using namespace std;

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
FilesystemTest::testCopyFile ()
{
	std::string testdata_path;
	CPPUNIT_ASSERT (find_file (test_search_path (), "RosegardenPatchFile.xml", testdata_path));

	std::string output_path = test_output_directory ("CopyFile");

	output_path = Glib::build_filename (output_path, "RosegardenPatchFile.xml");

	cerr << "CopyFile test output path: " << output_path << endl;

	CPPUNIT_ASSERT (PBD::copy_file (testdata_path, output_path));
}
