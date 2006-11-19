/* xml++.cc
 * libxml++ and this file are copyright (C) 2000 by Ari Johnson, and
 * are covered by the GNU Lesser General Public License, which should be
 * included with libxml++ as the file COPYING.
 */

#include <pbd/xml++.h>
#include <libxml/debugXML.h>

static XMLNode *readnode(xmlNodePtr);
static void writenode(xmlDocPtr, XMLNode *, xmlNodePtr, int);

XMLTree::XMLTree() 
	: _filename(), 
	_root(0), 
	_compression(0)
{ 
}

XMLTree::XMLTree(const string &fn)
	: _filename(fn), 
	_root(0), 
	_compression(0)
{ 
	read(); 
}

XMLTree::XMLTree(const XMLTree * from)
{
	_filename = from->filename();
	_root = new XMLNode(*from->root());
	_compression = from->compression();
}

XMLTree::~XMLTree()
{
	if (_root) {
		delete _root;
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
XMLTree::read(void)
{
	xmlDocPtr doc;
	
	if (_root) {
		delete _root;
		_root = 0;
	}
	
	xmlKeepBlanksDefault(0);
	
	doc = xmlParseFile(_filename.c_str());
	if (!doc) {
	 	return false;
	}
	
	_root = readnode(xmlDocGetRootElement(doc));
	xmlFreeDoc(doc);
	
	return true;
}

bool 
XMLTree::read_buffer(const string & buffer)
{
	xmlDocPtr doc;
	
	_filename = "";
	
	if (_root) {
		delete _root;
		_root = 0;
	}
	
	doc = xmlParseMemory((char *) buffer.c_str(), buffer.length());
	if (!doc) {
		return false;
	}
	
	_root = readnode(xmlDocGetRootElement(doc));
	xmlFreeDoc(doc);
	
	return true;
}

bool 
XMLTree::write(void) const
{
	xmlDocPtr doc;
	XMLNodeList children;
	int result;
	
	xmlKeepBlanksDefault(0);
	doc = xmlNewDoc((xmlChar *) "1.0");
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
    doc = xmlNewDoc((xmlChar *) "1.0");
    xmlSetDocCompressMode(doc, _compression);
    writenode(doc, _root, doc->children, 1);
    xmlDebugDumpDocument (out, doc);
    xmlFreeDoc(doc);
}

const string & 
XMLTree::write_buffer(void) const
{
	static string retval;
	char *ptr;
	int len;
	xmlDocPtr doc;
	XMLNodeList children;
	
	xmlKeepBlanksDefault(0);
	doc = xmlNewDoc((xmlChar *) "1.0");
	xmlSetDocCompressMode(doc, _compression);
	writenode(doc, _root, doc->children, 1);
	xmlDocDumpMemory(doc, (xmlChar **) & ptr, &len);
	xmlFreeDoc(doc);
	
	retval = ptr;
	
	free(ptr);
	
	return retval;
}

XMLNode::XMLNode(const string & n)
	:  _name(n), _is_content(false), _content(string())
{
}

XMLNode::XMLNode(const string & n, const string & c)
	:_name(n), _is_content(true), _content(c)
{
}

XMLNode::XMLNode(const XMLNode& from)
{
	XMLPropertyList props;
	XMLPropertyIterator curprop;
	XMLNodeList nodes;
	XMLNodeIterator curnode;
	
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

XMLNode::~XMLNode()
{
	XMLNodeIterator curchild;
	XMLPropertyIterator curprop;
	
	for (curchild = _children.begin(); curchild != _children.end();	++curchild) {
		delete *curchild;
	}
	    
	for (curprop = _proplist.begin(); curprop != _proplist.end(); ++curprop) {
		delete *curprop;
	}
}

const string & 
XMLNode::set_content(const string & c)
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
XMLNode::child (const char *name) const
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

const XMLNodeList & 
XMLNode::children(const string& n) const
{
	/* returns all children matching name */

	static XMLNodeList retval;
	XMLNodeConstIterator cur;
	
	if (n.empty()) {
		return _children;
	}
	    
	retval.erase(retval.begin(), retval.end());
	
	for (cur = _children.begin(); cur != _children.end(); ++cur) {
		if ((*cur)->name() == n) {
			retval.insert(retval.end(), *cur);
		}
	}
	    
	return retval;
}

XMLNode *
XMLNode::add_child(const char * n)
{
	return add_child_copy(XMLNode (n));
}

void
XMLNode::add_child_nocopy (XMLNode& n)
{
	_children.insert(_children.end(), &n);
}

XMLNode *
XMLNode::add_child_copy(const XMLNode& n)
{
	XMLNode *copy = new XMLNode (n);
	_children.insert(_children.end(), copy);
	return copy;
}

XMLNode *
XMLNode::add_content(const string & c)
{
	return add_child_copy(XMLNode (string(), c));
}

XMLProperty *
XMLNode::property(const char * n)
{
	string ns(n);
	if (_propmap.find(ns) == _propmap.end()) {
		return 0;
	}
	
	return _propmap[ns];
}

XMLProperty *
XMLNode::add_property(const char * n, const string & v)
{
	string ns(n);
	if(_propmap.find(ns) != _propmap.end()){
		remove_property(ns);
	}

	XMLProperty *tmp = new XMLProperty(ns, v);

	if (!tmp) {
		return 0;
	}

	_propmap[tmp->name()] = tmp;
	_proplist.insert(_proplist.end(), tmp);

	return tmp;
}

XMLProperty *
XMLNode::add_property(const char * n, const char * v)
{
	string vs(v);
	return add_property(n, vs);
}

void 
XMLNode::remove_property(const string & n)
{
	if (_propmap.find(n) != _propmap.end()) {
		_proplist.remove(_propmap[n]);
		_propmap.erase(n);
	}
}

void 
XMLNode::remove_nodes(const string & n)
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
XMLNode::remove_nodes_and_delete(const string & n)
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

XMLProperty::XMLProperty(const string &n, const string &v)
	: _name(n), 
	_value(v) 
{ 
}

XMLProperty::~XMLProperty()
{
}

static XMLNode *
readnode(xmlNodePtr node)
{
	string name, content;
	xmlNodePtr child;
	XMLNode *tmp;
	xmlAttrPtr attr;
	
	if (node->name) {
		name = (char *) node->name;
	}
	
	tmp = new XMLNode(name);
	
	for (attr = node->properties; attr; attr = attr->next) {
		content = "";
		if (attr->children) {
			content = (char *) attr->children->content;
		}
		tmp->add_property((char *) attr->name, content);
	}
	
	if (node->content) {
		tmp->set_content((char *) node->content);
	} else {
		tmp->set_content(string());
	}
	
	for (child = node->children; child; child = child->next) {
		tmp->add_child_nocopy (*readnode(child));
	}
	
	return tmp;
}

static void 
writenode(xmlDocPtr doc, XMLNode * n, xmlNodePtr p, int root = 0)
{
	XMLPropertyList props;
	XMLPropertyIterator curprop;
	XMLNodeList children;
	XMLNodeIterator curchild;
	xmlNodePtr node;
	
	if (root) {
		node = doc->children = xmlNewDocNode(doc, 0, (xmlChar *) n->name().c_str(), 0);
	} else {
		node = xmlNewChild(p, 0, (xmlChar *) n->name().c_str(), 0);
	}
	    
	if (n->is_content()) {
		node->type = XML_TEXT_NODE;
		xmlNodeSetContentLen(node, (const xmlChar *) n->content().c_str(), n->content().length());
	}
	
	props = n->properties();
	for (curprop = props.begin(); curprop != props.end(); ++curprop) {
		xmlSetProp(node, (xmlChar *) (*curprop)->name().c_str(), (xmlChar *) (*curprop)->value().c_str());
	}
	    
	children = n->children();
	for (curchild = children.begin(); curchild != children.end(); ++curchild) {
		writenode(doc, *curchild, node);
	}
}
