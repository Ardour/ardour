#include "xml_test.h"

#include <glib.h>
#include <pbd/gstdio_compat.h>

#include <unistd.h>
#include <stdlib.h>

#ifdef PLATFORM_WINDOWS
#include <fcntl.h>
#endif

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glibmm/convert.h>

#include <libxml/xpath.h>

#include "pbd/file_utils.h"

#include "test_common.h"

using namespace std;
using namespace PBD;

CPPUNIT_TEST_SUITE_REGISTRATION (XMLTest);

namespace {

xmlChar* xml_version = xmlCharStrdup("1.0");

bool
write_xml(const string& filename)
{
	xmlDocPtr doc;
	int result;

	xmlKeepBlanksDefault(0);
	doc = xmlNewDoc(xml_version);

	result = xmlSaveFormatFileEnc(filename.c_str(), doc, "UTF-8", 1);

	xmlFreeDoc(doc);

	if (result == -1) {
		return false;
	}
	return true;
}

}

void
XMLTest::testXMLFilenameEncoding ()
{
	vector<string> i18n_files;

	Searchpath i18n_path(test_search_path());
	i18n_path.add_subdirectory_to_paths("i18n_test");

	PBD::find_files_matching_pattern (i18n_files, i18n_path, "*.tst");

	CPPUNIT_ASSERT (i18n_files.size() == 8);

	string output_dir = test_output_directory ("XMLFilenameEncodingUTF8");

	// This is testing that libxml expects the filename encoding to be utf-8
	// on Windows and that writing the xml files should be successful for all
	// the filenames in the test data set but it should also work for other
	// platforms as well
	for (vector<string>::iterator i = i18n_files.begin (); i != i18n_files.end ();
	     ++i) {
		string input_path = *i;
		string output_filename = Glib::path_get_basename (input_path);
		string output_path = Glib::build_filename (output_dir, output_filename);

		CPPUNIT_ASSERT (write_xml (output_path));
	}
}
