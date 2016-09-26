#include "xml_test.h"

#include <glib.h>
#include "pbd/gstdio_compat.h"
#include "pbd/xml++.h"

#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>

#ifdef PLATFORM_WINDOWS
#include <fcntl.h>
#endif

#include <sstream>

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>
#include <glibmm/convert.h>

#include <libxml/xpath.h>

#include "pbd/file_utils.h"
#include "pbd/timing.h"

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


static const char * const root_node_name = "Session";
static const char * const child_node_name = "Child";
static const char * const grandchild_node_name = "GrandChild";
static const char * const great_grandchild_node_name = "GreatGrandChild";

std::vector<std::pair<std::string, std::string> >
get_test_properties ()
{
	std::vector<std::pair<std::string, std::string> > props;

	props.push_back (std::make_pair ("id", "1234567890"));
	props.push_back (std::make_pair ("name", "Awesome Name"));
	props.push_back (std::make_pair ("type", "Human"));
	props.push_back (std::make_pair ("flags", "MustExist,IsGodLike,HasFabulousHair"));
	props.push_back (std::make_pair ("muted", "no"));
	props.push_back (std::make_pair ("opaque", "yes"));
	props.push_back (std::make_pair ("locked", "false"));
	props.push_back (std::make_pair ("automatic", "true"));
	props.push_back (std::make_pair ("whole-file", "yes"));
	props.push_back (std::make_pair ("external", "false"));
	props.push_back (std::make_pair ("hidden", "no"));
	props.push_back (std::make_pair ("start", "123456789098"));
	props.push_back (std::make_pair ("length", "123456789"));
	props.push_back (std::make_pair ("stretch", "1"));
	props.push_back (std::make_pair ("shift", "1"));
	props.push_back (std::make_pair ("master-source-0", "12345"));
	props.push_back (std::make_pair ("master-source-1", "54321"));
	props.push_back (std::make_pair ("source-0", "123"));
	props.push_back (std::make_pair ("source-1", "321"));
	props.push_back (std::make_pair ("default-fade-in", "yes"));
	props.push_back (std::make_pair ("default-fade-out", "no"));
	props.push_back (std::make_pair ("fade-in-active", "no"));
	props.push_back (std::make_pair ("fade-out-active", "yes"));
	props.push_back (std::make_pair ("channels", "2"));
	props.push_back (std::make_pair ("beat", "0"));
	props.push_back (std::make_pair ("pulse", "1.3333333"));
	props.push_back (std::make_pair ("sync-position", "4321"));
	props.push_back (std::make_pair ("ancestral-start", "987654321"));
	props.push_back (std::make_pair ("ancestral-length", "12345678"));

	return props;
}

static const std::vector<std::pair<std::string, std::string> > properties = get_test_properties ();

static
std::string
get_event_content (uint32_t lines)
{
	stringstream sstr;
	sstr.precision (17);
	for (uint32_t i = 0; i < lines; ++i) {
		sstr << 0.12345678901234567;
		sstr << ' ';
		sstr << -0.9876543210987654;
		sstr << '\n';
	}
	return sstr.str();
}

struct NodeOptions
{

	NodeOptions (const std::string& p_node_name, uint32_t p_node_count,
	             uint32_t p_node_property_count, const std::string& p_node_content = std::string())
	    : node_name (p_node_name)
	    , node_count (p_node_count)
	    , node_property_count (p_node_property_count)
	    , node_content (p_node_content)
	{
	}

	std::string node_name;
	uint32_t node_count;
	uint32_t node_property_count;
	std::string node_content;
};

bool
create_child_nodes (XMLNode& parent_node, std::vector<NodeOptions>::iterator begin,
                    std::vector<NodeOptions>::iterator end)
{
	if (begin == end) return true;

	NodeOptions options = *begin;

	std::vector<NodeOptions>::iterator child_node_iter = ++begin;

	for (uint32_t node_count = 0; node_count < options.node_count; ++node_count) {

		XMLNode* new_node = new XMLNode (options.node_name);

		if (!new_node) {
			return false;
		}

		for (uint32_t prop_count = 0; prop_count < options.node_property_count; ++prop_count) {
			new_node->set_property (properties[prop_count].first.c_str (), properties[prop_count].second);
		}

		if (!options.node_content.empty()) {
			XMLNode* content_node = new XMLNode ("");
			if (!content_node) {
				return false;
			}
			content_node->set_content (options.node_content);
			new_node->add_child_nocopy (*content_node);
		}

		create_child_nodes (*new_node, child_node_iter, end);

		// Terrible API "design"
		parent_node.add_child_nocopy (*new_node);
	}
	return true;
}


bool
create_xml_doc (XMLTree& xml_doc, std::vector<NodeOptions>& options)
{
	XMLNode* root = new XMLNode (root_node_name);

	if (!root) return false;

	xml_doc.set_root (root);

	return create_child_nodes (*xml_doc.root(), options.begin(), options.end());
}

static const uint32_t test_iterations = 10;

void
test_xml_document (const std::string& test_name,
                   std::vector<NodeOptions>& node_options)
{
	const string test_output_dir = test_output_directory (test_name);

	const std::string output_file_basename = Glib::build_filename (test_output_dir, test_name);

	TimingData create_timing_data, write_timing_data, read_timing_data;

	for (uint32_t iter = 0; iter < test_iterations; ++iter) {

		char buf[16];
		snprintf (buf, sizeof(buf), "%d", iter);

		const std::string output_file_path = output_file_basename + buf + ".xml";

		create_timing_data.start_timing ();

		XMLTree test_xml;

		CPPUNIT_ASSERT (create_xml_doc (test_xml, node_options));

		create_timing_data.add_elapsed ();

		write_timing_data.start_timing ();

		test_xml.write (output_file_path);

		write_timing_data.add_elapsed ();

		read_timing_data.start_timing ();

		PBD::Timing read_timing;

		XMLTree read_doc (output_file_path);

		read_timing_data.add_elapsed ();

		// check that what we have read is identical to what was written
		CPPUNIT_ASSERT (*read_doc.root() == *test_xml.root());

		// These files are too big to keep around
		CPPUNIT_ASSERT (g_remove (output_file_path.c_str ()) == 0);
	}

	std::cerr << std::endl;
	std::cerr << "   Create : " << create_timing_data.summary ();
	std::cerr << "   Write : " << write_timing_data.summary ();
	std::cerr << "   Read : " << read_timing_data.summary ();
}

void
XMLTest::testPerfSmallXMLDocument ()
{
	std::vector<NodeOptions> node_options;

	// A config like file with nodes with properties with name/value pairs
	node_options.push_back (NodeOptions (child_node_name, 256, 2));

	test_xml_document ("testPerfSmallXMLDocument", node_options);
}

void
XMLTest::testPerfMediumXMLDocument ()
{
	std::vector<NodeOptions> node_options;

	// A normal Session like size file
	node_options.push_back (NodeOptions (child_node_name, 32, 2));
	node_options.push_back (NodeOptions (grandchild_node_name, 32, 16, get_event_content (16)));
	node_options.push_back (NodeOptions (great_grandchild_node_name, 8, 8));

	test_xml_document ("testPerfMediumXMLDocument", node_options);
}

void
XMLTest::testPerfLargeXMLDocument ()
{
	std::vector<NodeOptions> node_options;

	// A large Session like size file
	node_options.push_back (NodeOptions (child_node_name, 32, 2));
	node_options.push_back (NodeOptions (grandchild_node_name, 128, 16, get_event_content (32)));
	node_options.push_back (NodeOptions (great_grandchild_node_name, 16, 8));

	test_xml_document ("testPerfLargeXMLDocument", node_options);
}
