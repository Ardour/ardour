#include "filesystem_test.h"

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <unistd.h>
#include <stdlib.h>

#include <fcntl.h>

#ifdef COMPILER_MSVC
#include <sys/utime.h>
#else
#include <utime.h>
#endif

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glibmm/convert.h>
#include <glibmm/timer.h>

#include "pbd/file_utils.h"
#include "pbd/pathexpand.h"

#include "test_common.h"

using namespace std;
using namespace PBD;

CPPUNIT_TEST_SUITE_REGISTRATION (FilesystemTest);

namespace {

class PwdReset
{
public:

	PwdReset(const string& new_pwd)
		: m_old_pwd(Glib::get_current_dir()) {
		CPPUNIT_ASSERT (g_chdir (new_pwd.c_str()) == 0);
	}

	~PwdReset()
	{
		CPPUNIT_ASSERT (g_chdir (m_old_pwd.c_str()) == 0);
	}

private:

	string m_old_pwd;

};

} // anon

void
FilesystemTest::testPathIsWithin ()
{
#ifndef PLATFORM_WINDOWS
	string output_path = test_output_directory ("testPathIsWithin");
	PwdReset pwd_reset(output_path);

	CPPUNIT_ASSERT (g_mkdir_with_parents ("foo/bar/baz", 0755) == 0);

	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/bar/baz"), Glib::build_filename(output_path, "foo/bar/baz")));
	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/bar"),     Glib::build_filename(output_path, "foo/bar/baz")));
	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo"),         Glib::build_filename(output_path, "foo/bar/baz")));
	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/bar"),     Glib::build_filename(output_path, "foo/bar/baz")));
	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/bar"),     Glib::build_filename(output_path, "foo/bar")));

	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/bar/baz"), Glib::build_filename(output_path, "frobozz")) == false);

	int const r = symlink ("bar", "foo/jim");
	CPPUNIT_ASSERT (r == 0);

	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/jim/baz"), Glib::build_filename(output_path, "foo/bar/baz")));
	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/jim"),     Glib::build_filename(output_path, "foo/bar/baz")));
	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo"),         Glib::build_filename(output_path, "foo/bar/baz")));
	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/jim"),     Glib::build_filename(output_path, "foo/bar/baz")));
	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/jim"),     Glib::build_filename(output_path, "foo/bar")));

	CPPUNIT_ASSERT (PBD::path_is_within (Glib::build_filename(output_path, "foo/jim/baz"), Glib::build_filename(output_path, "frobozz")) == false);
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

	CPPUNIT_ASSERT (i18n_files.size() == 8);

	cerr << endl;
	cerr << "Copying " << i18n_files.size() << " test files from: "
	     << i18n_path.to_string () << endl;

	string output_dir = test_output_directory ("CopyFile");

	for (vector<string>::iterator i = i18n_files.begin(); i != i18n_files.end(); ++i) {
		string input_path = *i;
		string output_file = Glib::path_get_basename(*i);
		string output_path = Glib::build_filename (output_dir, output_file);

		cerr << "Copying test file: " << input_path
		     << " To " << output_path << endl;

		CPPUNIT_ASSERT (PBD::copy_file (input_path, output_path));
	}
}

void
FilesystemTest::testOpenFileUTF8Filename ()
{
	vector<string> i18n_files;

	Searchpath i18n_path (test_search_path ());
	i18n_path.add_subdirectory_to_paths ("i18n_test");

	PBD::find_files_matching_pattern (i18n_files, i18n_path, "*.tst");

	CPPUNIT_ASSERT (i18n_files.size () == 8);

	cerr << endl;
	cerr << "Opening " << i18n_files.size ()
	     << " test files from: " << i18n_path.to_string () << endl;

	// check that g_open will successfully open all the test files
	for (vector<string>::iterator i = i18n_files.begin (); i != i18n_files.end ();
	     ++i) {
		string input_path = *i;

		cerr << "Opening file: " << input_path << " with g_open" << endl;

		int fdgo = g_open (input_path.c_str(), O_RDONLY, 0444);

		CPPUNIT_ASSERT (fdgo != -1);

		if (fdgo >= 0) {
			::close (fdgo);
		}
	}

#ifdef PLATFORM_WINDOWS
	// This test is here to prove and remind us that using Glib::locale_from_utf8
	// to convert a utf-8 encoded file path for use with ::open will not work
	// for all file paths.
	//
	// It may be possible to convert a string that is utf-8 encoded that will not
	// work with ::open(on windows) to a string that will work with ::open using
	// Glib::locale_from_utf8 string if all the characters that are contained
	// in the utf-8 string can be found/mapped in the system code page.
	//
	// European locales that only have a small amount of extra characters with
	// accents/umlauts I'm guessing will be more likely succeed but CJK locales
	// will almost certainly fail

	bool conversion_failed = false;

	for (vector<string>::iterator i = i18n_files.begin (); i != i18n_files.end ();
	     ++i) {
		string input_path = *i;
		cerr << "Opening file: " << input_path << " with locale_from_utf8 and ::open "
		     << endl;
		string converted_input_path;
		int fdo;

		try {
			// this will fail for utf8 that contains characters that aren't
			// representable in the system code page
			converted_input_path = Glib::locale_from_utf8 (input_path);
			// conversion succeeded so we expect ::open to be successful if the
			// current C library locale is the same as the system locale, which
			// it should be as we haven't changed it.
			fdo = ::open (converted_input_path.c_str (), O_RDONLY, 0444);
			CPPUNIT_ASSERT (fdo != -1);

			if (converted_input_path != input_path) {
				cerr << "Character set conversion succeeded and strings differ for input "
				        "string: " << input_path << endl;
				// file path must have contained non-ASCII characters that were mapped
				// from the system code page so we would expect the original
				// utf-8 file path to fail with ::open
				int fd2 = ::open (input_path.c_str (), O_RDONLY, 0444);
				CPPUNIT_ASSERT (fd2 == -1);
			}

		} catch (const Glib::ConvertError& err) {
			cerr << "Character set conversion failed: " << err.what () << endl;
			// I am confident that on Windows with the test data that no locale will
			// have a system code page containing all the characters required
			// and conversion will fail for at least one of the filenames
			conversion_failed = true;
			// CPPUNIT_ASSERT (err.code() == ?);

			// conversion failed so we expect the original utf-8 string to fail
			// with ::open on Windows as the file path will not exist
			fdo = ::open (input_path.c_str (), O_RDONLY, 0444);
			CPPUNIT_ASSERT (fdo == -1);
		}

		if (fdo >= 0) {
			::close (fdo);
		}
	}
	// we expect at least one conversion failure with the filename test data
	CPPUNIT_ASSERT (conversion_failed);
#endif
}

void
FilesystemTest::testFindFilesMatchingPattern ()
{
	vector<string> patch_files;

	PBD::find_files_matching_pattern (patch_files, test_search_path (), "*PatchFile*");

	CPPUNIT_ASSERT(test_search_path ().size() == 1);

	CPPUNIT_ASSERT(patch_files.size() == 2);
}

string
create_test_directory (std::string test_dir)
{
	vector<string> test_files;
	vector<string> i18n_files;

	Searchpath spath(test_search_path());
	PBD::get_files (test_files, spath);

	spath.add_subdirectory_to_paths("i18n_test");

	PBD::get_files (i18n_files, spath);

	string output_dir = test_output_directory (test_dir);

	CPPUNIT_ASSERT (test_search_path().size () != 0);

	string test_dir_path = test_search_path()[0];

	cerr << endl;
	cerr << "Copying " << test_files.size() << " test files from: "
	     << test_dir_path << " to " << output_dir << endl;

	CPPUNIT_ASSERT (test_files.size() != 0);

	PBD::copy_files (test_dir_path, output_dir);

	vector<string> copied_files;

	PBD::get_files (copied_files, output_dir);

	CPPUNIT_ASSERT (copied_files.size() == test_files.size());

	string subdir_path = Glib::build_filename (output_dir, "subdir");

	CPPUNIT_ASSERT (g_mkdir_with_parents (subdir_path.c_str(), 0755) == 0);

	cerr << endl;
	cerr << "Copying " << i18n_files.size() << " i18n test files to: "
	     << subdir_path << endl;

	for (vector<string>::iterator i = i18n_files.begin(); i != i18n_files.end(); ++i) {
		string input_filepath = *i;
		string output_filename = Glib::path_get_basename(*i);
		string output_filepath = Glib::build_filename (subdir_path, output_filename);

		CPPUNIT_ASSERT (PBD::copy_file (input_filepath, output_filepath));
	}

	copied_files.clear();
	PBD::get_files (copied_files, subdir_path);

	CPPUNIT_ASSERT (copied_files.size() == i18n_files.size());

	return output_dir;
}

void
FilesystemTest::testClearDirectory ()
{
	string output_dir_path = create_test_directory ("ClearDirectory");

	vector<string> files_in_output_dir;

	PBD::get_paths (files_in_output_dir, output_dir_path, true, true);

	size_t removed_file_size = 0;
	vector<string> removed_files;

	CPPUNIT_ASSERT (PBD::clear_directory (output_dir_path, &removed_file_size, &removed_files) ==0);

	cerr << "Removed " << removed_files.size() << " files of total size: "
	     << removed_file_size << endl;

	CPPUNIT_ASSERT (removed_files.size () == files_in_output_dir.size ());

	string subdir_path = Glib::build_filename (output_dir_path, "subdir");

	// make sure the directory structure is still there
	CPPUNIT_ASSERT (Glib::file_test (subdir_path, Glib::FILE_TEST_IS_DIR));
}

void
FilesystemTest::testRemoveDirectory ()
{
	string output_dir_path = create_test_directory ("RemoveDirectory");

	vector<string> files_in_output_dir;

	PBD::get_paths (files_in_output_dir, output_dir_path, false, true);

	CPPUNIT_ASSERT (files_in_output_dir.size () != 0);

	PBD::remove_directory (output_dir_path);

	CPPUNIT_ASSERT (!Glib::file_test (output_dir_path, Glib::FILE_TEST_EXISTS));
}

void
FilesystemTest::testCanonicalPathASCII ()
{
	string top_dir = test_output_directory ("testCanonicalPathASCII");
	PwdReset pwd_reset(top_dir);

	string pwd = Glib::get_current_dir ();

	CPPUNIT_ASSERT (!pwd.empty());
	CPPUNIT_ASSERT (pwd == top_dir);

	string relative_path = ".";
	string canonical_path = PBD::canonical_path (relative_path);

	CPPUNIT_ASSERT (pwd == canonical_path);

	const std::string dir1 = Glib::build_filename (top_dir, "dir1");
	const std::string dir2 = Glib::build_filename (top_dir, "dir2");

	CPPUNIT_ASSERT (g_mkdir (dir1.c_str(), 0755) == 0);
	CPPUNIT_ASSERT (g_mkdir (dir2.c_str(), 0755) == 0);

	CPPUNIT_ASSERT (Glib::file_test (dir1, Glib::FILE_TEST_IS_DIR));
	CPPUNIT_ASSERT (Glib::file_test (dir2, Glib::FILE_TEST_IS_DIR));

	relative_path = Glib::build_filename (".", "dir1", "..", "dir2");
	canonical_path = PBD::canonical_path (relative_path);
	string absolute_path = Glib::build_filename (top_dir, "dir2");

	CPPUNIT_ASSERT (canonical_path == absolute_path);
}

void
FilesystemTest::testCanonicalPathUTF8 ()
{
	string top_dir = test_output_directory ("testCanonicalPathUTF8");
	PwdReset pwd_reset(top_dir);

	string pwd = Glib::get_current_dir ();

	CPPUNIT_ASSERT (!pwd.empty());
	CPPUNIT_ASSERT (pwd == top_dir);

// We are using glib for file I/O on windows and the character encoding is
// guaranteed to be utf-8 as glib does the conversion for us. This is not the
// case on linux/OS X where the encoding is probably utf-8, but it is not
// ensured so we can't really enable this part of the test for now, but it is
// only really important to test on Windows.
#ifdef PLATFORM_WINDOWS

	std::vector<std::string> utf8_strings;

	get_utf8_test_strings (utf8_strings);

	for (std::vector<std::string>::const_iterator i = utf8_strings.begin (); i != utf8_strings.end ();
	     ++i) {

		const std::string absolute_path = Glib::build_filename (top_dir, *i);
		// make a directory in the current test/working directory
		CPPUNIT_ASSERT (g_mkdir (absolute_path.c_str (), 0755) == 0);

		string relative_path = Glib::build_filename (".", *i);

		string canonical_path = PBD::canonical_path (relative_path);

		// Check that it resolved to the absolute path
		CPPUNIT_ASSERT (absolute_path == canonical_path);
	}

#endif
}

void
FilesystemTest::testTouchFile ()
{
	const string filename = "touch.me";

	const string test_dir = test_output_directory ("testTouchFile");
	const string touch_file_path = Glib::build_filename (test_dir, filename);

	CPPUNIT_ASSERT (touch_file(touch_file_path));

	CPPUNIT_ASSERT (Glib::file_test (touch_file_path, Glib::FILE_TEST_EXISTS));
}

void
FilesystemTest::testStatFile ()
{
	const string filename1 = "touch.me";
	const string filename2 = "touch.me.2";

	const string test_dir = test_output_directory ("testStatFile");

	const string path1 = Glib::build_filename (test_dir, filename1);
	const string path2 = Glib::build_filename (test_dir, filename2);

	CPPUNIT_ASSERT (touch_file(path1));

	Glib::usleep (2000000);

	CPPUNIT_ASSERT (touch_file(path2));

	GStatBuf gsb1;
	GStatBuf gsb2;

	CPPUNIT_ASSERT (g_stat (path1.c_str(), &gsb1) == 0);
	CPPUNIT_ASSERT (g_stat (path2.c_str(), &gsb2) == 0);

	cerr << endl;
	cerr << "StatFile: " << path1 << " access time: " << gsb1.st_atime << endl;
	cerr << "StatFile: " << path1 << " modification time: " << gsb1.st_mtime << endl;
	cerr << "StatFile: " << path2 << " access time: " << gsb2.st_atime << endl;
	cerr << "StatFile: " << path2 << " modification time: " << gsb2.st_mtime << endl;

	CPPUNIT_ASSERT (gsb1.st_atime == gsb1.st_mtime);
	CPPUNIT_ASSERT (gsb2.st_atime == gsb2.st_mtime);

	// at least access time works on windows(or at least on ntfs)
	CPPUNIT_ASSERT (gsb1.st_atime < gsb2.st_atime);
	CPPUNIT_ASSERT (gsb1.st_mtime < gsb2.st_mtime);

	struct utimbuf tbuf;

	tbuf.actime = gsb1.st_atime;
	tbuf.modtime = gsb1.st_mtime;

	// update the file access/modification times to be the same
	CPPUNIT_ASSERT (g_utime (path2.c_str(), &tbuf) == 0);

	CPPUNIT_ASSERT (g_stat (path2.c_str(), &gsb2) == 0);

	cerr << endl;
	cerr << "StatFile: " << path1 << " access time: " << gsb1.st_atime << endl;
	cerr << "StatFile: " << path1 << " modification time: " << gsb1.st_mtime << endl;
	cerr << "StatFile: " << path2 << " access time: " << gsb2.st_atime << endl;
	cerr << "StatFile: " << path2 << " modification time: " << gsb2.st_mtime << endl;

	CPPUNIT_ASSERT (gsb1.st_atime == gsb2.st_atime);
	CPPUNIT_ASSERT (gsb1.st_mtime == gsb2.st_mtime);
}
