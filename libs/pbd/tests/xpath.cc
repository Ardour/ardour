#include "assert.h"
#include <iostream>

#include "pbd/xml++.h"

using namespace std;

int main()
{
	XMLTree  doc("./rosegardenpatchfile.xml");
	XMLNode* root = doc.root();
	// "//bank" gives as last element an empty element libxml bug????
	XMLNodeList* result = root->find("//bank[@name]");
	
	cerr << "Found " << result->size() << " banks" << endl;
	assert(result->size() == 8);
	int counter = 1;
	for(XMLNodeList::const_iterator i = result->begin(); i != result->end(); ++i) {
		assert((*i)->name() == "bank");
		assert((*i)->property("name"));
		cout << "Found bank number " << counter++ << " with name: " << (*i)->property("name")->value() << endl;
		for(XMLNodeList::const_iterator j = (*i)->children().begin(); j != (*i)->children().end(); ++j) {
			cout << "\t found program with name: " << (*j)->property("name")->value() << endl;
		}
	}
}
