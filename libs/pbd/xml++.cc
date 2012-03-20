/* xml++.cc
 * libxml++ and this file are copyright (C) 2000 by Ari Johnson, and
 * are covered by the GNU Lesser General Public License, which should be
 * included with libxml++ as the file COPYING.
 * Modified for Ardour and released under the same terms.
 */

#include <iostream>
#include "pbd/xml++.h"
#include <libxml/debugXML.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#define XML_VERSION "1.0"

using namespace std;

static XMLNode*           readnode(xmlNodePtr);
static void               writenode(xmlDocPtr, XMLNode*, xmlNodePtr, int);
static XMLSharedNodeList* find_impl(xmlXPathContext* ctxt, const string& xpath);

XMLTree::XMLTree()
	: _filename()
	, _root(0)
	, _doc (0)
	, _compression(0)
{
}

XMLTree::XMLTree(const string& fn, bool validate)
	: _filename(fn)
	, _root(0)
	, _doc (0)
	, _compression(0)
{
	read_internal(validate);
}

XMLTree::XMLTree(const XMLTree* from)
	: _filename(from->filename())
	, _root(new XMLNode(*from->root()))
	, _doc (xmlCopyDoc (from->_doc, 1))
	, _compression(from->compression())
{
	
}

XMLTree::~XMLTree()
{
	delete _root;

	if (_doc) {
		xmlFreeDoc (_doc);
	}
}

int
XMLTree::set_compression(int c)
{
	if (c > 9) {
		c = 9;
	} else if (c < 0) {
		c = 0;
	}

	_compression = c;

	return _compression;
}

bool
XMLTree::read_internal(bool validate)
{
	//shouldnt be used anywhere ATM, remove if so!
	assert(!validate);

	delete _root;
	_root = 0;

	if (_doc) {
		xmlFreeDoc (_doc);
		_doc = 0;
	}

	xmlParserCtxtPtr ctxt = NULL; /* the parser context */

	xmlKeepBlanksDefault(0);
	/* parse the file, activating the DTD validation option */
	if (validate) {
		/* create a parser context */
		ctxt = xmlNewParserCtxt();
		if (ctxt == NULL) {
			return false;
		}
		_doc = xmlCtxtReadFile(ctxt, _filename.c_str(), NULL, XML_PARSE_DTDVALID);
	} else {
		_doc = xmlParseFile(_filename.c_str());
	}
	
	/* check if parsing suceeded */
	if (_doc == NULL) {
		if (validate) {
			xmlFreeParserCtxt(ctxt);
		}
		return false;
	} else {
		/* check if validation suceeded */
		if (validate && ctxt->valid == 0) {
			xmlFreeParserCtxt(ctxt);
			throw XMLException("Failed to validate document " + _filename);
		}
	}

	_root = readnode(xmlDocGetRootElement(_doc));

	/* free up the parser context */
	if (validate) {
		xmlFreeParserCtxt(ctxt);
	}
	
	return true;
}

bool
XMLTree::read_buffer(const string& buffer)
{
	xmlDocPtr doc;

	_filename = "";

	delete _root;
	_root = 0;

	doc = xmlParseMemory((char*)buffer.c_str(), buffer.length());
	if (!doc) {
		return false;
	}

	_root = readnode(xmlDocGetRootElement(doc));
	xmlFreeDoc(doc);

	return true;
}


bool
XMLTree::write() const
{
	xmlDocPtr doc;
	XMLNodeList children;
	int result;

	xmlKeepBlanksDefault(0);
	doc = xmlNewDoc((xmlChar*) XML_VERSION);
	xmlSetDocCompressMode(doc, _compression);
	writenode(doc, _root, doc->children, 1);
	result = xmlSaveFormatFileEnc(_filename.c_str(), doc, "UTF-8", 1);
	xmlFreeDoc(doc);

	if (result == -1) {
		return false;
	}

	return true;
}

void
XMLTree::debug(FILE* out) const
{
	xmlDocPtr doc;
	XMLNodeList children;

	xmlKeepBlanksDefault(0);
	doc = xmlNewDoc((xmlChar*) XML_VERSION);
	xmlSetDocCompressMode(doc, _compression);
	writenode(doc, _root, doc->children, 1);
	xmlDebugDumpDocument (out, doc);
	xmlFreeDoc(doc);
}

const string&
XMLTree::write_buffer() const
{
	static string retval;
	char* ptr;
	int len;
	xmlDocPtr doc;
	XMLNodeList children;

	xmlKeepBlanksDefault(0);
	doc = xmlNewDoc((xmlChar*) XML_VERSION);
	xmlSetDocCompressMode(doc, _compression);
	writenode(doc, _root, doc->children, 1);
	xmlDocDumpMemory(doc, (xmlChar **) & ptr, &len);
	xmlFreeDoc(doc);

	retval = ptr;

	free(ptr);

	return retval;
}

XMLNode::XMLNode(const string& n)
	: _name(n)
	, _is_content(false)
{
}

XMLNode::XMLNode(const string& n, const string& c)
	: _name(n)
	, _is_content(true)
	, _content(c)
{
}

XMLNode::XMLNode(const XMLNode& from)
{
	*this = from;
}

XMLNode::~XMLNode()
{
	clear_lists ();
}

void
XMLNode::clear_lists ()
{
	XMLNodeIterator curchild;
	XMLPropertyIterator curprop;

	_selected_children.clear ();
	_propmap.clear ();

	for (curchild = _children.begin(); curchild != _children.end();	++curchild) {
		delete *curchild;
	}

	_children.clear ();

	for (curprop = _proplist.begin(); curprop != _proplist.end(); ++curprop) {
		delete *curprop;
	}

	_proplist.clear ();
}

XMLNode& 
XMLNode::operator= (const XMLNode& from)
{
	if (&from != this) {

		XMLPropertyList props;
		XMLPropertyIterator curprop;
		XMLNodeList nodes;
		XMLNodeIterator curnode;
		
		clear_lists ();

		_name = from.name();
		set_content(from.content());
		
		props = from.properties();
		for (curprop = props.begin(); curprop != props.end(); ++curprop) {
			add_property((*curprop)->name().c_str(), (*curprop)->value());
		}
		
		nodes = from.children();
		for (curnode = nodes.begin(); curnode != nodes.end(); ++curnode) {
			add_child_copy(**curnode);
		}
	}

	return *this;
}

const string&
XMLNode::set_content(const string& c)
{
	if (c.empty()) {
		_is_content = false;
	} else {
		_is_content = true;
	}

	_content = c;

	return _content;
}

XMLNode*
XMLNode::child (const char* name) const
{
	/* returns first child matching name */

	XMLNodeConstIterator cur;

	if (name == 0) {
		return 0;
	}

	for (cur = _children.begin(); cur != _children.end(); ++cur) {
		if ((*cur)->name() == name) {
			return *cur;
		}
	}

	return 0;
}

const XMLNodeList&
XMLNode::children(const string& n) const
{
	/* returns all children matching name */

	XMLNodeConstIterator cur;

	if (n.empty()) {
		return _children;
	}

	_selected_children.clear();

	for (cur = _children.begin(); cur != _children.end(); ++cur) {
		if ((*cur)->name() == n) {
			_selected_children.insert(_selected_children.end(), *cur);
		}
	}

	return _selected_children;
}

XMLNode*
XMLNode::add_child(const char* n)
{
	return add_child_copy(XMLNode (n));
}

void
XMLNode::add_child_nocopy(XMLNode& n)
{
	_children.insert(_children.end(), &n);
}

XMLNode*
XMLNode::add_child_copy(const XMLNode& n)
{
	XMLNode *copy = new XMLNode(n);
	_children.insert(_children.end(), copy);
	return copy;
}

boost::shared_ptr<XMLSharedNodeList>
XMLTree::find(const string xpath, XMLNode* node) const
{
	xmlXPathContext* ctxt;
	xmlDocPtr doc = 0;

	if (node) {
		doc = xmlNewDoc((xmlChar*) XML_VERSION);
		writenode(doc, node, doc->children, 1);
		ctxt = xmlXPathNewContext(doc);
	} else {
		ctxt = xmlXPathNewContext(_doc);
	}
	
	boost::shared_ptr<XMLSharedNodeList> result =
		boost::shared_ptr<XMLSharedNodeList>(find_impl(ctxt, xpath));
	
	xmlXPathFreeContext(ctxt);
	if (doc) {
		xmlFreeDoc (doc);
	}

	return result;
}

std::string
XMLNode::attribute_value()
{
	XMLNodeList children = this->children();
	assert(!_is_content);
	assert(children.size() == 1);
	XMLNode* child = *(children.begin());
	assert(child->is_content());
	return child->content();
}

XMLNode*
XMLNode::add_content(const string& c)
{
	return add_child_copy(XMLNode (string(), c));
}

XMLProperty*
XMLNode::property(const char* n)
{
	string ns(n);
	map<string,XMLProperty*>::iterator iter;

	if ((iter = _propmap.find(ns)) != _propmap.end()) {
		return iter->second;
	}

	return 0;
}

XMLProperty*
XMLNode::property(const string& ns)
{
	map<string,XMLProperty*>::iterator iter;

	if ((iter = _propmap.find(ns)) != _propmap.end()) {
		return iter->second;
	}

	return 0;
}

XMLProperty*
XMLNode::add_property(const char* n, const string& v)
{
	string ns(n);
        map<string,XMLProperty*>::iterator iter;
	
        if ((iter = _propmap.find(ns)) != _propmap.end()) {
                iter->second->set_value (v);
                return iter->second;
	}

	XMLProperty* tmp = new XMLProperty(ns, v);

	if (!tmp) {
		return 0;
	}

	_propmap[tmp->name()] = tmp;
	_proplist.insert(_proplist.end(), tmp);

	return tmp;
}

XMLProperty*
XMLNode::add_property(const char* n, const char* v)
{
	string vs(v);
	return add_property(n, vs);
}

XMLProperty*
XMLNode::add_property(const char* name, const long value)
{
	char str[64];
	snprintf(str, sizeof(str), "%ld", value);
	return add_property(name, str);
}

void
XMLNode::remove_property(const string& n)
{
	if (_propmap.find(n) != _propmap.end()) {
		XMLProperty* p = _propmap[n];
		_proplist.remove (p);
		delete p;
		_propmap.erase(n);
	}
}

/** Remove any property with the given name from this node and its children */
void
XMLNode::remove_property_recursively(const string& n)
{
	remove_property (n);
	for (XMLNodeIterator i = _children.begin(); i != _children.end(); ++i) {
		(*i)->remove_property_recursively (n);
	}
}

void
XMLNode::remove_nodes(const string& n)
{
	XMLNodeIterator i = _children.begin();
	XMLNodeIterator tmp;

	while (i != _children.end()) {
		tmp = i;
		++tmp;
		if ((*i)->name() == n) {
			_children.erase (i);
		}
		i = tmp;
	}
}

void
XMLNode::remove_nodes_and_delete(const string& n)
{
	XMLNodeIterator i = _children.begin();
	XMLNodeIterator tmp;

	while (i != _children.end()) {
		tmp = i;
		++tmp;
		if ((*i)->name() == n) {
			delete *i;
			_children.erase (i);
		}
		i = tmp;
	}
}

void
XMLNode::remove_nodes_and_delete(const string& propname, const string& val)
{
	XMLNodeIterator i = _children.begin();
	XMLNodeIterator tmp;
	XMLProperty* prop;

	while (i != _children.end()) {
		tmp = i;
		++tmp;

		prop = (*i)->property(propname);
		if (prop && prop->value() == val) {
			delete *i;
			_children.erase(i);
		}

		i = tmp;
	}
}

XMLProperty::XMLProperty(const string& n, const string& v)
	: _name(n)
	, _value(v)
{
	// Normalize property name (replace '_' with '-' as old session are inconsistent)
	for (size_t i = 0; i < _name.length(); ++i) {
		if (_name[i] == '_') {
			_name[i] = '-';
		}
	}
}

XMLProperty::~XMLProperty()
{
}

static XMLNode*
readnode(xmlNodePtr node)
{
	string name, content;
	xmlNodePtr child;
	XMLNode* tmp;
	xmlAttrPtr attr;

	if (node->name) {
		name = (char*)node->name;
	}

	tmp = new XMLNode(name);

	for (attr = node->properties; attr; attr = attr->next) {
		content = "";
		if (attr->children) {
			content = (char*)attr->children->content;
		}
		tmp->add_property((char*)attr->name, content);
	}

	if (node->content) {
		tmp->set_content((char*)node->content);
	} else {
		tmp->set_content(string());
	}

	for (child = node->children; child; child = child->next) {
		tmp->add_child_nocopy (*readnode(child));
	}

	return tmp;
}

static void
writenode(xmlDocPtr doc, XMLNode* n, xmlNodePtr p, int root = 0)
{
	XMLPropertyList props;
	XMLPropertyIterator curprop;
	XMLNodeList children;
	XMLNodeIterator curchild;
	xmlNodePtr node;

	if (root) {
		node = doc->children = xmlNewDocNode(doc, 0, (xmlChar*) n->name().c_str(), 0);
	} else {
		node = xmlNewChild(p, 0, (xmlChar*) n->name().c_str(), 0);
	}

	if (n->is_content()) {
		node->type = XML_TEXT_NODE;
		xmlNodeSetContentLen(node, (const xmlChar*)n->content().c_str(), n->content().length());
	}

	props = n->properties();
	for (curprop = props.begin(); curprop != props.end(); ++curprop) {
		xmlSetProp(node, (xmlChar*) (*curprop)->name().c_str(), (xmlChar*) (*curprop)->value().c_str());
	}

	children = n->children();
	for (curchild = children.begin(); curchild != children.end(); ++curchild) {
		writenode(doc, *curchild, node);
	}
}

static XMLSharedNodeList* find_impl(xmlXPathContext* ctxt, const string& xpath)
{
	xmlXPathObject* result = xmlXPathEval((const xmlChar*)xpath.c_str(), ctxt);

	if (!result) {
		xmlXPathFreeContext(ctxt);
		xmlFreeDoc(ctxt->doc);

		throw XMLException("Invalid XPath: " + xpath);
	}

	if (result->type != XPATH_NODESET) {
		xmlXPathFreeObject(result);
		xmlXPathFreeContext(ctxt);
		xmlFreeDoc(ctxt->doc);

		throw XMLException("Only nodeset result types are supported.");
	}

	xmlNodeSet* nodeset = result->nodesetval;
	XMLSharedNodeList* nodes = new XMLSharedNodeList();
	if (nodeset) {
		for (int i = 0; i < nodeset->nodeNr; ++i) {
			XMLNode* node = readnode(nodeset->nodeTab[i]);
			nodes->push_back(boost::shared_ptr<XMLNode>(node));
		}
	} else {
		// return empty set
	}

	xmlXPathFreeObject(result);

	return nodes;
}

/** Dump a node, its properties and children to a stream */
void
XMLNode::dump (ostream& s, string p) const
{
	s << p << _name << " ";
	for (XMLPropertyList::const_iterator i = _proplist.begin(); i != _proplist.end(); ++i) {
		s << (*i)->name() << "=" << (*i)->value() << " ";
	}
	s << "\n";
	
	for (XMLNodeList::const_iterator i = _children.begin(); i != _children.end(); ++i) {
		(*i)->dump (s, p + "  ");
	}
}
