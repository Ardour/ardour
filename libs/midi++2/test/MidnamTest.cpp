#include "MidnamTest.hpp"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/xml++.h"
#include "pbd/file_utils.h"
#include "pbd/compose.h"
#include "midi++/midnam_patch.h"

using namespace std;
using namespace PBD;
using namespace MIDI::Name;

CPPUNIT_TEST_SUITE_REGISTRATION( MidnamTest );

PBD::Searchpath
test_search_path ()
{
#ifdef PLATFORM_WINDOWS
	if (!getenv("MIDIPP_TEST_PATH")) {
		std::vector<std::string> path_tok;
		path_tok.push_back (g_win32_get_package_installation_directory_of_module(NULL));
		path_tok.push_back ("share");
		path_tok.push_back ("ardour6"); // XXX lwrcase_dirname
		path_tok.push_back ("patchfiles");
		return Glib::build_filename (path_tok);
	}
#endif
	return Glib::getenv("MIDIPP_TEST_PATH");
}

void
MidnamTest::protools_patchfile_test()
{
    std::string test_file_path;

    CPPUNIT_ASSERT(find_file (test_search_path (), "Roland_SC_88_Pro.midnam", test_file_path));
    XMLTree xmldoc(test_file_path);
    boost::shared_ptr<XMLSharedNodeList> result = xmldoc.find(
            "//MIDINameDocument");
    CPPUNIT_ASSERT(result->size() == 1);

    result = xmldoc.find("//ChannelNameSet");
    CPPUNIT_ASSERT(result->size() == 2);

    MIDINameDocument doc(test_file_path);
    CPPUNIT_ASSERT(doc.all_models().size() == 1);
    CPPUNIT_ASSERT(doc.author().find("Mark of the Unicorn") == 0);

    const string model = *doc.all_models().begin();
    CPPUNIT_ASSERT_EQUAL(string("SC-88 Pro"), model);
    boost::shared_ptr<MasterDeviceNames> masterDeviceNames =
            doc.master_device_names_by_model().find(model)->second;
    CPPUNIT_ASSERT_EQUAL(string("Roland"), masterDeviceNames->manufacturer());

    string modename = masterDeviceNames->custom_device_mode_names().front();
    CPPUNIT_ASSERT_EQUAL(string("Default"), modename);

    boost::shared_ptr<CustomDeviceMode> mode =
            masterDeviceNames->custom_device_mode_by_name(modename);

    CPPUNIT_ASSERT_EQUAL(modename, mode->name());

    string ns1 = string("Name Set 1");
    string ns2 = string("Name Set 2");

    for (uint8_t i = 0; i <= 15; i++) {
        if (i != 9)
            CPPUNIT_ASSERT_EQUAL(ns1,
                    mode->channel_name_set_name_by_channel(i));
        else
            CPPUNIT_ASSERT_EQUAL(ns2,
                    mode->channel_name_set_name_by_channel(i));
    }

    boost::shared_ptr<ChannelNameSet> nameSet1 =
            masterDeviceNames->channel_name_set_by_channel(modename, 0);
    boost::shared_ptr<ChannelNameSet> nameSet2 =
            masterDeviceNames->channel_name_set_by_channel(modename, 9);

    CPPUNIT_ASSERT_EQUAL(ns1, nameSet1->name());
    CPPUNIT_ASSERT_EQUAL(ns2, nameSet2->name());

    const ChannelNameSet::PatchBanks& banks1 = nameSet1->patch_banks();
    const ChannelNameSet::PatchBanks& banks2 = nameSet2->patch_banks();
    CPPUNIT_ASSERT(banks1.size() == 16);
    CPPUNIT_ASSERT(banks2.size() == 1);

    boost::shared_ptr<PatchBank> bank = banks1.front();
    CPPUNIT_ASSERT_EQUAL(string("Piano"), bank->name());
    const PatchNameList& plist1 = bank->patch_name_list();
    CPPUNIT_ASSERT(plist1.size() == 110);

    bank = banks2.front();
    CPPUNIT_ASSERT_EQUAL(string("Drum sets"), bank->name());
    const PatchNameList& plist2 = bank->patch_name_list();
    CPPUNIT_ASSERT(plist2.size() == 49);
}

void
MidnamTest::yamaha_PSRS900_patchfile_test()
{
    std::string test_file_path;

    CPPUNIT_ASSERT(find_file (test_search_path (), "Yamaha_PSR_S900.midnam", test_file_path));
    XMLTree xmldoc(test_file_path);
    boost::shared_ptr<XMLSharedNodeList> result = xmldoc.find(
            "//MIDINameDocument");
    CPPUNIT_ASSERT(result->size() == 1);

    result = xmldoc.find("//ChannelNameSet");
    CPPUNIT_ASSERT(result->size() == 3);

    MIDINameDocument doc(test_file_path);
    CPPUNIT_ASSERT(doc.all_models().size() == 1);
    CPPUNIT_ASSERT(doc.author().find("Hans Baier") == 0);

    const string model = *doc.all_models().begin();
    CPPUNIT_ASSERT_EQUAL(string("PSR-S900"), model);
    boost::shared_ptr<MasterDeviceNames> masterDeviceNames =
            doc.master_device_names_by_model().find(model)->second;
    CPPUNIT_ASSERT_EQUAL(string("Yamaha"), masterDeviceNames->manufacturer());

    const MasterDeviceNames::CustomDeviceModeNames& modes = masterDeviceNames->custom_device_mode_names();
    CPPUNIT_ASSERT(masterDeviceNames->custom_device_mode_names().size() == 3);

    string modename = modes.front();
    CPPUNIT_ASSERT_EQUAL(string("Standard"), modename);

    modename = (*(++modes.begin()));
    CPPUNIT_ASSERT_EQUAL(string("GM+XG"), modename);

    modename = modes.back();
    CPPUNIT_ASSERT_EQUAL(string("GM2"), modename);

    for (list<string>::const_iterator modename = modes.begin(); modename != modes.end(); ++modename) {
        boost::shared_ptr<CustomDeviceMode> mode =
                masterDeviceNames->custom_device_mode_by_name(*modename);

        CPPUNIT_ASSERT_EQUAL(*modename, mode->name());

        string ns = mode->name();

        if (ns != string("Standard"))
        for (uint8_t i = 0; i <= 15; i++) {
                CPPUNIT_ASSERT_EQUAL(ns,
                        mode->channel_name_set_name_by_channel(i));
                boost::shared_ptr<ChannelNameSet> nameSet =
                        masterDeviceNames->channel_name_set_by_channel(ns, 1);

                CPPUNIT_ASSERT_EQUAL(ns, nameSet->name());

                const ChannelNameSet::PatchBanks& banks1 = nameSet->patch_banks();
                CPPUNIT_ASSERT(banks1.size() > 1);

                boost::shared_ptr<PatchBank> bank = banks1.front();
                const PatchNameList& list = bank->patch_name_list();

                for(PatchNameList::const_iterator p = list.begin(); p != list.end(); ++p) {

                if (ns == string("GM+XG")) {
                    uint8_t msb = (((*p)->bank_number()) >> 7) & 0x7f;
                    CPPUNIT_ASSERT( msb == 0 || msb == 64);
                }

                if (ns == string("GM2")) {
                    CPPUNIT_ASSERT((*p)->bank_number() >= (uint16_t(120) << 7));
                }
                }
        }
    }
}

void
MidnamTest::load_all_midnams_test ()
{
    vector<std::string> result;

    PBD::find_files_matching_pattern (result, test_search_path (), "*.midnam");

    CPPUNIT_ASSERT(!result.empty());

    cout << "Loading " << result.size() << " MIDI patches from " << test_search_path ().to_string () << endl;

    for (vector<std::string>::iterator i = result.begin(); i != result.end(); ++i) {
        cout << "Processing file " << Glib::path_get_basename(*i) << endl;
        boost::shared_ptr<MIDINameDocument> document(new MIDINameDocument(*i));

        XMLTree xmldoc(*i);
        boost::shared_ptr<XMLSharedNodeList> result = xmldoc.find("//MIDINameDocument");
        CPPUNIT_ASSERT(result->size() == 1);

        result = xmldoc.find("//ExtendingDeviceNames");
        if (result->size() != 0) {
          cout << "  ... skipped (ExtendingDeviceNames)" << endl;
          continue;
        }

        result = xmldoc.find("//MasterDeviceNames");
        CPPUNIT_ASSERT(result->size() == 1);

        result = xmldoc.find("//PatchBank");
        //int banks = result->size();


        result = xmldoc.find("//CustomDeviceMode[1]");
        string deviceModeName = result->front()->property("Name")->value();

        MIDINameDocument::MasterDeviceNamesList::const_iterator device =
                    document->master_device_names_by_model().begin();

        string modename = device->second->custom_device_mode_names().front();
        boost::shared_ptr<CustomDeviceMode> mode = device->second->custom_device_mode_by_name(modename);
        CPPUNIT_ASSERT_EQUAL(deviceModeName, mode->name());

        boost::shared_ptr<ChannelNameSet> nameSet = device->second->channel_name_set_by_channel(modename, 0);
    }
}

