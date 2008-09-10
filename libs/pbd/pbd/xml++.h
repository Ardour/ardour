/* xml++.h
 * libxml++ and this file are copyright (C) 2000 by Ari Johnson, and
 * are covered by the GNU Lesser General Public License, which should be
 * included with libxml++ as the file COPYING.
 */

#include <string>
#include <list>
#include <map>
#include <cstdio>
#include <cstdarg>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <boost/shared_ptr.hpp>

#ifndef __XML_H
#define __XML_H

using std::string;
using std::map;
using std::list;

class XMLTree;
class XMLNode;
class XMLProperty;

typedef list<XMLNode *> XMLNodeList;
typedef list<boost::shared_ptr<XMLNode> > XMLSharedNodeList;
typedef XMLNodeList::iterator XMLNodeIterator;
typedef XMLNodeList::const_iterator XMLNodeConstIterator;
typedef list<XMLProperty*> XMLPropertyList;
typedef XMLPropertyList::iterator XMLPropertyIterator;
typedef XMLPropertyList::const_iterator XMLPropertyConstIterator;
typedef map<string, XMLProperty*> XMLPropertyMap;

class XMLTree {
private:
  string _filename;
  XMLNode *_root;
  int _compression;
  bool read_internal(bool validate);
  
public:
  XMLTree();
  XMLTree(const string &fn, bool validate = false);
  XMLTree(const XMLTree *);
  ~XMLTree();

  XMLNode *root() const { return _root; };
  XMLNode *set_root(XMLNode *n) { return _root = n; };

  const string & filename() const { return _filename; };
  const string & set_filename(const string &fn) { return _filename = fn; };

  int compression() const { return _compression; };
  int set_compression(int);

  bool read() { return read_internal(false); };
  bool read(const string &fn) { set_filename(fn); return read_internal(false); };
  bool read_and_validate() { return read_internal(true); };
  bool read_and_validate(const string &fn) { set_filename(fn); return read_internal(true); };
  bool read_buffer(const string &);

  bool write() const;
  bool write(const string &fn) { set_filename(fn); return write(); };

  void debug (FILE*) const;

  const string & write_buffer() const;
};

class XMLNode {
private:
  string _name;
  bool _is_content;
  string _content;
  XMLNodeList _children;
  XMLPropertyList _proplist;
  XMLPropertyMap _propmap;
  mutable XMLNodeList _selected_children;

public:
  XMLNode(const string& name);
  XMLNode(const string& name, const string& content);
  XMLNode(const XMLNode& other);
  ~XMLNode();

  const string name() const { return _name; };

  bool is_content() const { return _is_content; };
  const string & content() const { return _content; };
  const string & set_content (const string &);
  XMLNode *add_content(const string & = string());

  const XMLNodeList & children (const string& str = string()) const;
  XMLNode *add_child (const char *);
  XMLNode *add_child_copy (const XMLNode&);
  XMLNode *child (const char*) const;
  void add_child_nocopy (XMLNode&);
  
  boost::shared_ptr<XMLSharedNodeList> find(const std::string xpath) const;
  std::string attribute_value();
  
  const XMLPropertyList & properties() const { return _proplist; };
  XMLProperty *property(const char * );
  XMLProperty *property(const std::string&);
  const XMLProperty *property(const char * n) const
	{ return ((XMLNode *) this)->property(n); };
  const XMLProperty *property(const std::string& ns) const
	{ return ((XMLNode *) this)->property(ns); };
  XMLProperty *add_property(const char *name, const string& value);
  XMLProperty *add_property(const char *name, const char *value = "");
  XMLProperty *add_property(const char *name, const long value);

  void remove_property(const string &);

  /** Remove all nodes with the name passed to remove_nodes */
  void remove_nodes(const string &);
  /** Remove and delete all nodes with the name passed to remove_nodes */
  void remove_nodes_and_delete(const string &);
  /** Remove and delete all nodes with property prop matching val */
  void remove_nodes_and_delete(const string& propname, const string& val);
};

class XMLProperty {
private:
  string _name;
  string _value;

public:
  XMLProperty(const string &n, const string &v = string());
  ~XMLProperty();

  const string & name() const { return _name; };
  const string & value() const { return _value; };
  const string & set_value(const string &v) { return _value = v; };
};

class XMLException: public std::exception
{
public:
  explicit XMLException(const string message)
  : message_(message)
  {
  }
  
  virtual ~XMLException() throw() {};

  virtual const char* what() const throw() { return message_.c_str(); }
  virtual void Raise() const { throw *this; }
  virtual exception * Clone() const { return new exception(*this); }

private:
  string message_;
};

#endif /* __XML_H */

