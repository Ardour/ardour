/*
    Copyright (C) 2012 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __XML_H
#define __XML_H

/* xml++.h
 * libxml++ and this file are copyright (C) 2000 by Ari Johnson, and
 * are covered by the GNU Lesser General Public License, which should be
 * included with libxml++ as the file COPYING.
 * Modified for Ardour and released under the same terms.
 */

#include <string>
#include <list>
#include <map>
#include <cstdio>
#include <cstdarg>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <boost/shared_ptr.hpp>

#include "pbd/libpbd_visibility.h"

class XMLTree;
class XMLNode;
class XMLProperty;

typedef std::list<XMLNode *>                   XMLNodeList;
typedef std::list<boost::shared_ptr<XMLNode> > XMLSharedNodeList;
typedef XMLNodeList::iterator                  XMLNodeIterator;
typedef XMLNodeList::const_iterator            XMLNodeConstIterator;
typedef std::list<XMLProperty*>                XMLPropertyList;
typedef XMLPropertyList::iterator              XMLPropertyIterator;
typedef XMLPropertyList::const_iterator        XMLPropertyConstIterator;
typedef std::map<std::string, XMLProperty*>    XMLPropertyMap;

class LIBPBD_API XMLTree {
public:
	XMLTree();
	XMLTree(const std::string& fn, bool validate = false);
	XMLTree(const XMLTree*);
	~XMLTree();

	XMLNode* root() const         { return _root; }
	XMLNode* set_root(XMLNode* n) { return _root = n; }

	const std::string& filename() const               { return _filename; }
	const std::string& set_filename(const std::string& fn) { return _filename = fn; }

	int compression() const { return _compression; }
	int set_compression(int);

	bool read() { return read_internal(false); }
	bool read(const std::string& fn) { set_filename(fn); return read_internal(false); }
	bool read_and_validate() { return read_internal(true); }
	bool read_and_validate(const std::string& fn) { set_filename(fn); return read_internal(true); }
	bool read_buffer(const std::string&);

	bool write() const;
	bool write(const std::string& fn) { set_filename(fn); return write(); }

	void debug (FILE*) const;

	const std::string& write_buffer() const;

	boost::shared_ptr<XMLSharedNodeList> find(const std::string xpath, XMLNode* = 0) const;

private:
	bool read_internal(bool validate);
	
	std::string _filename;
	XMLNode*    _root;
	xmlDocPtr   _doc;
	int         _compression;
};

class LIBPBD_API XMLNode {
public:
	XMLNode(const std::string& name);
	XMLNode(const std::string& name, const std::string& content);
	XMLNode(const XMLNode& other);
	~XMLNode();

	XMLNode& operator= (const XMLNode& other);

	const std::string& name() const { return _name; }

	bool          is_content() const { return _is_content; }
	const std::string& content()    const { return _content; }
	const std::string& set_content(const std::string&);
	XMLNode*      add_content(const std::string& s = std::string());

	const XMLNodeList& children(const std::string& str = std::string()) const;
	XMLNode* child(const char*) const;
	XMLNode* add_child(const char *);
	XMLNode* add_child_copy(const XMLNode&);
	void     add_child_nocopy(XMLNode&);

	std::string attribute_value();

	const XMLPropertyList& properties() const { return _proplist; }
	XMLProperty*       property(const char*);
	XMLProperty*       property(const std::string&);
        const XMLProperty* property(const char* n)   const { return const_cast<XMLNode*>(this)->property(n); }
        const XMLProperty* property(const std::string& n) const { return const_cast<XMLNode*>(this)->property(n); }
	
	XMLProperty* add_property(const char* name, const std::string& value);
	XMLProperty* add_property(const char* name, const char* value = "");
	XMLProperty* add_property(const char* name, const long value);

	void remove_property(const std::string&);
	void remove_property_recursively(const std::string&);

	/** Remove all nodes with the name passed to remove_nodes */
	void remove_nodes(const std::string&);
	/** Remove and delete all nodes with the name passed to remove_nodes */
	void remove_nodes_and_delete(const std::string&);
	/** Remove and delete all nodes with property prop matching val */
	void remove_nodes_and_delete(const std::string& propname, const std::string& val);

	void dump (std::ostream &, std::string p = "") const;

private:
	std::string         _name;
	bool                _is_content;
	std::string         _content;
	XMLNodeList         _children;
	XMLPropertyList     _proplist;
	XMLPropertyMap      _propmap;
	mutable XMLNodeList _selected_children;

	void clear_lists ();
};

class LIBPBD_API XMLProperty {
public:
	XMLProperty(const std::string& n, const std::string& v = std::string());
	~XMLProperty();

	const std::string& name() const { return _name; }
	const std::string& value() const { return _value; }
	const std::string& set_value(const std::string& v) { return _value = v; }

private:
	std::string _name;
	std::string _value;
};

class LIBPBD_API XMLException: public std::exception {
public:
	explicit XMLException(const std::string msg) : _message(msg) {}
	virtual ~XMLException() throw() {}

	virtual const char* what() const throw() { return _message.c_str(); }

private:
	std::string _message;
};

#endif /* __XML_H */

