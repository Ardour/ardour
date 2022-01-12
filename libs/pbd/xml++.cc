/* xml++.cc
 * libxml++ and this file are copyright (C) 2000 by Ari Johnson, and
 * are covered by the GNU Lesser General Public License, which should be
 * included with libxml++ as the file COPYING.
 * Modified for Ardour and released under the same terms.
 */

#include <string.h>
#include <iostream>

#include "pbd/xml++.h"

#include <libxml/debugXML.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

xmlChar* xml_version = xmlCharStrdup("1.0");

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

	/* Calling this prevents libxml2 from treating whitespace as active
	   nodes. It needs to be called before we create a parser context.
	*/
	xmlKeepBlanksDefault(0);

	/* create a parser context */
	xmlParserCtxtPtr ctxt = xmlNewParserCtxt();
	if (ctxt == NULL) {
		return false;
	}

	/* parse the file, activating the DTD validation option */
	if (validate) {
		_doc = xmlCtxtReadFile(ctxt, _filename.c_str(), NULL, XML_PARSE_DTDVALID);
	} else {
		_doc = xmlCtxtReadFile(ctxt, _filename.c_str(), NULL, XML_PARSE_HUGE);
	}

	/* check if parsing suceeded */
	if (_doc == NULL) {
		xmlFreeParserCtxt(ctxt);
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
	xmlFreeParserCtxt(ctxt);

	return true;
}

bool
XMLTree::read_buffer (char const* buffer, bool to_tree_doc)
{
	xmlDocPtr doc;

	_filename = "";

	delete _root;
	_root = 0;

	xmlKeepBlanksDefault(0);

	doc = xmlParseMemory (buffer, ::strlen(buffer));
	if (!doc) {
		return false;
	}

	_root = readnode(xmlDocGetRootElement(doc));
	if (to_tree_doc) {
		if (_doc) {
			xmlFreeDoc (_doc);
		}
		_doc = doc;
	} else {
		xmlFreeDoc (doc);
	}

	return true;
}


bool
XMLTree::write() const
{
	xmlDocPtr doc;
	XMLNodeList children;
	int result;

	xmlKeepBlanksDefault(0);
	doc = xmlNewDoc(xml_version);
	xmlSetDocCompressMode(doc, _compression);
	writenode(doc, _root, doc->children, 1);
	result = xmlSaveFormatFileEnc(_filename.c_str(), doc, "UTF-8", 1);
#ifndef NDEBUG
	if (result == -1) {
		xmlErrorPtr xerr = xmlGetLastError ();
		if (!xerr) {
			std::cerr << "unknown XML error during xmlSaveFormatFileEnc()." << std::endl;
		} else {
			std::cerr << "xmlSaveFormatFileEnc: error"
				<< " domain: " << xerr->domain
				<< " code: " << xerr->code
				<< " msg: " << xerr->message
				<< std::endl;
		}
	}
#endif
	xmlFreeDoc(doc);

	if (result == -1) {
		return false;
	}

	return true;
}

void
XMLTree::debug(FILE* out) const
{
#ifdef LIBXML_DEBUG_ENABLED
	xmlDocPtr doc;
	XMLNodeList children;

	xmlKeepBlanksDefault(0);
	doc = xmlNewDoc(xml_version);
	xmlSetDocCompressMode(doc, _compression);
	writenode(doc, _root, doc->children, 1);
	xmlDebugDumpDocument (out, doc);
	xmlFreeDoc(doc);
#endif
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
	doc = xmlNewDoc(xml_version);
	xmlSetDocCompressMode(doc, _compression);
	writenode(doc, _root, doc->children, 1);
	xmlDocDumpMemory(doc, (xmlChar **) & ptr, &len);
	xmlFreeDoc(doc);

	retval = ptr;

	free(ptr);

	return retval;
}

static const int PROPERTY_RESERVE_COUNT = 16;

XMLNode::XMLNode(const string& n)
	: _name(n)
	, _is_content(false)
{
	_proplist.reserve (PROPERTY_RESERVE_COUNT);
}

XMLNode::XMLNode(const string& n, const string& c)
	: _name(n)
	, _is_content(true)
	, _content(c)
{
	_proplist.reserve (PROPERTY_RESERVE_COUNT);
}

XMLNode::XMLNode(const XMLNode& from)
{
	_proplist.reserve (PROPERTY_RESERVE_COUNT);
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

	for (curchild = _children.begin(); curchild != _children.end(); ++curchild) {
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
	if (&from == this) {
		return *this;
	}

	clear_lists ();

	_name = from.name ();
	set_content (from.content ());

	const XMLPropertyList& props = from.properties ();

	for (XMLPropertyConstIterator prop_iter = props.begin (); prop_iter != props.end (); ++prop_iter) {
		set_property ((*prop_iter)->name ().c_str (), (*prop_iter)->value ());
	}

	const XMLNodeList& nodes = from.children ();
	for (XMLNodeConstIterator child_iter = nodes.begin (); child_iter != nodes.end (); ++child_iter) {
		add_child_copy (**child_iter);
	}

	return *this;
}

bool
XMLNode::operator== (const XMLNode& other) const
{
	if (is_content () != other.is_content ()) {
		return false;
	}

	if (is_content ()) {
		if (content () != other.content ()) {
			return false;
		}
	} else {
		if (name () != other.name ()) {
			return false;
		}
	}

	XMLPropertyList const& other_properties = other.properties ();

	if (_proplist.size () != other_properties.size ()) {
		return false;
	}

	XMLPropertyConstIterator our_prop_iter = _proplist.begin();
	XMLPropertyConstIterator other_prop_iter = other_properties.begin();

	while (our_prop_iter != _proplist.end ()) {
		XMLProperty const* our_prop = *our_prop_iter;
		XMLProperty const* other_prop = *other_prop_iter;
		if (our_prop->name () != other_prop->name () || our_prop->value () != other_prop->value ()) {
			return false;
		}
		++our_prop_iter;
		++other_prop_iter;
	}

	XMLNodeList const& other_children = other.children();

	if (_children.size() != other_children.size()) {
		return false;
	}

	XMLNodeConstIterator our_child_iter = _children.begin ();
	XMLNodeConstIterator other_child_iter = other_children.begin ();

	while (our_child_iter != _children.end()) {
		XMLNode const* our_child = *our_child_iter;
		XMLNode const* other_child = *other_child_iter;

		if (*our_child != *other_child) {
			return false;
		}
		++our_child_iter;
		++other_child_iter;
	}
	return true;
}

bool
XMLNode::operator!= (const XMLNode& other) const
{
	return !(*this == other);
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

/* Return the content of the first content child
 *
 * `<node>Foo</node>`.
 * the `node` is not a content node, but has a child-node `text`.
 *
 * This method effectively is identical to
 * return this->child("text")->content()
 */
const string&
XMLNode::child_content() const
{
	static const string empty = "";
	for (XMLNodeList::const_iterator n = children ().begin (); n != children ().end (); ++n) {
		if ((*n)->is_content ()) {
			return (*n)->content ();
		}
	}
	return empty;
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
		doc = xmlNewDoc(xml_version);
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
	if (_is_content)
		throw XMLException("XMLNode: attribute_value failed (is_content) for requested node: " + name());

	if (children.size() != 1)
		throw XMLException("XMLNode: attribute_value failed (children.size != 1) for requested node: " + name());

	XMLNode* child = *(children.begin());
	if (!child->is_content())
		throw XMLException("XMLNode: attribute_value failed (!child->is_content()) for requested node: " + name());

	return child->content();
}

XMLNode*
XMLNode::add_content(const string& c)
{
	if (c.empty ()) {
		/* this would add a "</>" child, leading to invalid XML.
		 * Also in XML, empty string content is equivalent to no content.
		 */
		return NULL;
	}
	return add_child_copy(XMLNode (string(), c));
}

XMLProperty const *
XMLNode::property(const char* name) const
{
	XMLPropertyConstIterator iter = _proplist.begin();

	while (iter != _proplist.end()) {
		if ((*iter)->name() == name) {
			return *iter;
		}
		++iter;
	}

	return 0;
}

XMLProperty const *
XMLNode::property(const string& name) const
{
	XMLPropertyConstIterator iter = _proplist.begin();

	while (iter != _proplist.end()) {
		if ((*iter)->name() == name) {
			return *iter;
		}
		++iter;
	}
	return 0;
}

XMLProperty *
XMLNode::property(const char* name)
{
	XMLPropertyIterator iter = _proplist.begin();

	while (iter != _proplist.end()) {
		if ((*iter)->name() == name) {
			return *iter;
		}
		++iter;
	}
	return 0;
}

XMLProperty *
XMLNode::property(const string& name)
{
	XMLPropertyIterator iter = _proplist.begin();

	while (iter != _proplist.end()) {
		if ((*iter)->name() == name) {
			return *iter;
		}
		++iter;
	}

	return 0;
}

bool
XMLNode::has_property_with_value (const string& name, const string& value) const
{
	XMLPropertyConstIterator iter = _proplist.begin();

	while (iter != _proplist.end()) {
		if ((*iter)->name() == name && (*iter)->value() == value) {
			return true;
		}
		++iter;
	}
	return false;
}

bool
XMLNode::set_property(const char* name, const string& value)
{
	XMLPropertyIterator iter = _proplist.begin();

	while (iter != _proplist.end()) {
		if ((*iter)->name() == name) {
			(*iter)->set_value (value);
			return *iter;
		}
		++iter;
	}

	XMLProperty* new_property = new XMLProperty(name, value);

	if (!new_property) {
		return 0;
	}

	_proplist.insert(_proplist.end(), new_property);

	return new_property;
}

bool
XMLNode::get_property(const char* name, std::string& value) const
{
	XMLProperty const* const prop = property (name);
	if (!prop)
		return false;

	value = prop->value ();

	return true;
}

void
XMLNode::remove_property(const string& name)
{
	XMLPropertyIterator iter = _proplist.begin();

	while (iter != _proplist.end()) {
		if ((*iter)->name() == name) {
			XMLProperty* property = *iter;
			_proplist.erase (iter);
			delete property;
			break;
		}
		++iter;
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
	while (i != _children.end()) {
		if ((*i)->name() == n) {
			i = _children.erase (i);
		} else {
			++i;
		}
	}
}

void
XMLNode::remove_nodes_and_delete(const string& n)
{
	XMLNodeIterator i = _children.begin();

	while (i != _children.end()) {
		if ((*i)->name() == n) {
			delete *i;
			i = _children.erase (i);
		} else {
			++i;
		}
	}
}

void
XMLNode::remove_nodes_and_delete(const string& propname, const string& val)
{
	XMLNodeIterator i = _children.begin();
	XMLProperty const * prop;

	while (i != _children.end()) {
		prop = (*i)->property(propname);
		if (prop && prop->value() == val) {
			delete *i;
			i = _children.erase(i);
		} else {
			++i;
		}
	}
}

void
XMLNode::remove_node_and_delete(const string& n, const string& propname, const string& val)
{
	for (XMLNodeIterator i = _children.begin(); i != _children.end(); ++i) {
		if ((*i)->name() == n) {
			XMLProperty const * prop = (*i)->property (propname);
			if (prop && prop->value() == val) {
				delete *i;
				_children.erase (i);
				break;
			}
		}
	}
}

XMLProperty::XMLProperty(const string& n, const string& v)
	: _name(n)
	, _value(v)
{
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
		name = (const char*)node->name;
	}

	tmp = new XMLNode(name);

	for (attr = node->properties; attr; attr = attr->next) {
		content = "";
		if (attr->children) {
			content = (char*)attr->children->content;
		}
		tmp->set_property((const char*)attr->name, content);
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
	xmlNodePtr node;

	if (root) {
		node = doc->children = xmlNewDocNode(doc, 0, (const xmlChar*) n->name().c_str(), 0);
	} else {
		node = xmlNewChild(p, 0, (const xmlChar*) n->name().c_str(), 0);
	}

	if (n->is_content()) {
		node->type = XML_TEXT_NODE;
		xmlNodeSetContentLen(node, (const xmlChar*)n->content().c_str(), n->content().length());
	}

	const XMLPropertyList& props = n->properties();

	for (XMLPropertyConstIterator prop_iter = props.begin (); prop_iter != props.end ();
	     ++prop_iter) {
		xmlSetProp (node, (const xmlChar*)(*prop_iter)->name ().c_str (),
		            (const xmlChar*)(*prop_iter)->value ().c_str ());
	}

	const XMLNodeList& children = n->children ();
	for (XMLNodeConstIterator child_iter = children.begin (); child_iter != children.end ();
	     ++child_iter) {
		writenode (doc, *child_iter, node);
	}
}

static XMLSharedNodeList* find_impl(xmlXPathContext* ctxt, const string& xpath)
{
	xmlXPathObject* result = xmlXPathEval((const xmlChar*)xpath.c_str(), ctxt);

	if (!result) {
		xmlFreeDoc(ctxt->doc);
		xmlXPathFreeContext(ctxt);

		throw XMLException("Invalid XPath: " + xpath);
	}

	if (result->type != XPATH_NODESET) {
		xmlXPathFreeObject(result);
		xmlFreeDoc(ctxt->doc);
		xmlXPathFreeContext(ctxt);

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
	if (_is_content) {
		s << p << "  " << content() << "\n";
	} else {
		s << p << "<" << _name;
		for (XMLPropertyList::const_iterator i = _proplist.begin(); i != _proplist.end(); ++i) {
			s << " " << (*i)->name() << "=\"" << (*i)->value() << "\"";
		}
		s << ">\n";

		for (XMLNodeList::const_iterator i = _children.begin(); i != _children.end(); ++i) {
			(*i)->dump (s, p + "  ");
		}

		s << p << "</" << _name << ">\n";
	}
}
