/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_element_import_handler_h__
#define __ardour_element_import_handler_h__

#include <string>
#include <list>
#include <set>

#include <boost/shared_ptr.hpp>

#include "ardour/libardour_visibility.h"
#include "pbd/libpbd_visibility.h"

class XMLTree;

namespace ARDOUR {

class Session;
class ElementImporter;

/// Virtual interface class for element import handlers
class LIBARDOUR_API ElementImportHandler
{
  public:
	typedef boost::shared_ptr<ElementImporter> ElementPtr;
	typedef std::list<ElementPtr> ElementList;

	/** ElementImportHandler constructor
	 * The constructor should find everything from the XML Tree it can handle
	 * and create respective Elements stored in elements.
	 *
	 * @param source XML tree to be parsed
	 * @see elements
	 */
	ElementImportHandler (XMLTree const & source, ARDOUR::Session & session)
		: source (source), session (session) { }

	virtual ~ElementImportHandler ();

	/** Gets a textual representation of the element type
	 * @return textual representation of element type
	 */
	virtual std::string get_info () const = 0;

	/// Elements this handler handles
	ElementList elements;

	/* For checking duplicates names against queued elements */

	/** Checks whether or not an element with some name is queued or not
	 * @param name name to check
	 * @return true if name is not used
	 */
	bool check_name (const std::string & name) const;

	/// Adds name to the list of used names
	void add_name (std::string name);

	/// Removes name from the list of used names
	void remove_name (const std::string & name);

	/// Checks wheter or not all elements can be imported cleanly
	static bool dirty () { return _dirty; }

	/// Sets handler dirty
	static void set_dirty () { _dirty = true; }

	/// Checks wheter or not all elements were imported cleanly
	static bool errors () { return _errors; }

	/// Sets handler dirty
	static void set_errors () { _errors = true; }

  protected:
	/// Source session XML tree
	XMLTree const &   source;

	/// Destination session
	ARDOUR::Session & session;

	/// Session XML readability
	static bool _dirty;

	/// Errors post initialization
	static bool _errors;

  private:
	/// Set of names for duplicate checking
	std::set<std::string> names;
};

} // namespace ARDOUR

#endif
