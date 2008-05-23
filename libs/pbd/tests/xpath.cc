#include "assert.h"
#include <iostream>

#include "pbd/xml++.h"

using namespace std;

int main()
{
	cout << "Test 1: RosegardenPatchFile.xml: Find all banks in the file" << endl;
	XMLTree  doc("./RosegardenPatchFile.xml");
	// "//bank" gives as last element an empty element libxml bug????
	boost::shared_ptr<XMLSharedNodeList> result = doc.root()->find("//bank[@name]");
	
	cout << "Found " << result->size() << " banks" << endl;
	assert(result->size() == 8);
	int counter = 1;
	for(XMLSharedNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		assert((*i)->name() == "bank");
		assert((*i)->property("name"));
		cout << "Found bank number " << counter++ << " with name: " << (*i)->property("name")->value() << endl;
		for(XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
			cout << "\t found program " << (*j)->property("id")->value() << 
			        " with name: " << (*j)->property("name")->value() << endl;
		}
	}
	
	cout << endl << endl << "Test 2: RosegardenPatchFile.xml: Find all programs whose program name contains 'Latin'" << endl;
	
	result = doc.root()->find("/rosegarden-data/studio/device/bank/program[contains(@name, 'Latin')]");
	assert(result->size() == 5);
	
	for(XMLSharedNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		cout << "\t found program " << (*i)->property("id")->value() << 
		        " with name: " << (*i)->property("name")->value() << endl;
	}

	cout << endl << endl << "Test 3: TestSession.ardour: find all Sources where captured-for contains the string 'Guitar'" << endl;
	
	// We have to allocate a new document here, or we get segfaults
	XMLTree doc2("./TestSession.ardour");
	result = doc2.root()->find("/Session/Sources/Source[contains(@captured-for, 'Guitar')]");
	assert(result->size() == 16);
	
	for(XMLSharedNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		cout << "\t found source '" << (*i)->property("name")->value() << 
		        "' with id: " << (*i)->property("id")->value() << endl;
	}
	
	cout << endl << endl << "Test 4: TestSession.ardour: Find all elements with an 'id' and 'name' attribute" << endl;
	
	result = doc2.root()->find("//*[@id and @name]");
	
	for(XMLSharedNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		assert((*i)->property("id"));
		assert((*i)->property("name"));
		cout << "\t found element '" << (*i)->name() << 
		        "' with id: "  << (*i)->property("id")->value() << 
        		"' and name: " << (*i)->property("name")->value() << endl;
	}
	
	cout << endl << endl << "Test 5: ProtoolsPatchFile.midnam: Get Banks and Patches for 'Name Set 1'" << endl;
	
	// We have to allocate a new document here, or we get segfaults
	XMLTree doc3("./ProtoolsPatchFile.midnam");
	result = doc3.root()->find("/MIDINameDocument/MasterDeviceNames/ChannelNameSet[@Name='Name Set 1']/PatchBank");
	assert(result->size() == 16);
	
	for(XMLSharedNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		cout << "\t found Patchbank " << (*i)->property("Name")->value() << endl;
		boost::shared_ptr<XMLSharedNodeList> patches = (*i)->find("//Patch[@Name]");
		for(XMLSharedNodeList::const_iterator p = patches->begin(); p != patches->end(); ++p) {
			cout << "\t\t found patch number " << (*p)->property("Number")->value() 
			     << " with name: " << (*p)->property("Name")->value()  << endl;
		}
	}

	cout << endl << endl << "Test 5: ProtoolsPatchFile.midnam: Find attribute nodes" << endl;
	result = doc3.root()->find("//@Value");
	
	for(XMLSharedNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		boost::shared_ptr<XMLNode> node = (*i);
		cout << "\t found attribute node: " << node->name()  
		     << " value: " << node->attribute_value() << endl;
	}	
}
