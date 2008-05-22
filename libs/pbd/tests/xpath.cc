#include "assert.h"
#include <iostream>

#include "pbd/xml++.h"

using namespace std;

int main()
{
	cout << "Test 1: Find all banks in the file" << endl;
	XMLTree  doc("./rosegardenpatchfile.xml");
	// "//bank" gives as last element an empty element libxml bug????
	XMLNodeList* result = doc.root()->find("//bank[@name]");
	
	cout << "Found " << result->size() << " banks" << endl;
	assert(result->size() == 8);
	int counter = 1;
	for(XMLNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		assert((*i)->name() == "bank");
		assert((*i)->property("name"));
		cout << "Found bank number " << counter++ << " with name: " << (*i)->property("name")->value() << endl;
		for(XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
			cout << "\t found program " << (*j)->property("id")->value() << 
			        " with name: " << (*j)->property("name")->value() << endl;
		}
		
		delete *i;
	}
	
	delete result;
	
	cout << endl << endl << "Test 2: Find all programs whose program name contains 'Latin'" << endl;
	
	result = doc.root()->find("/rosegarden-data/studio/device/bank/program[contains(@name, 'Latin')]");
	assert(result->size() == 5);
	
	for(XMLNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		cout << "\t found program " << (*i)->property("id")->value() << 
		        " with name: " << (*i)->property("name")->value() << endl;
		
		delete *i;
	}
	
	delete result;

	cout << endl << endl << "Test 3: find all Sources where captured-for contains the string 'Guitar'" << endl;
	
	XMLTree doc2("./TestSession.ardour");
	result = doc2.root()->find("/Session/Sources/Source[contains(@captured-for, 'Guitar')]");
	assert(result->size() == 16);
	
	for(XMLNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		cout << "\t found source '" << (*i)->property("name")->value() << 
		        "' with id: " << (*i)->property("id")->value() << endl;
		
		delete *i;
	}
	
	delete result;
	
	cout << endl << endl << "Test 4: Find all elements with an 'id' and 'name' attribute" << endl;
	
	result = doc2.root()->find("//*[@id and @name]");
	
	for(XMLNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		assert((*i)->property("id"));
		assert((*i)->property("name"));
		cout << "\t found element '" << (*i)->name() << 
		        "' with id: "  << (*i)->property("id")->value() << 
        		"' and name: " << (*i)->property("name")->value() << endl;
		
		delete *i;
	}
	
	delete result;
}
