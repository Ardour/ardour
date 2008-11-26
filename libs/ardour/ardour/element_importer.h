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

#ifndef __ardour_element_importer_h__
#define __ardour_element_importer_h__

#include <string>
#include <utility>

#include <sigc++/signal.h>

#include <ardour/types.h>

using std::string;

class XMLTree;
namespace ARDOUR {

class Session;

/// Virtual interface class for element importers
class ElementImporter
{
  public:

	ElementImporter (XMLTree const & source, ARDOUR::Session & session);
	virtual ~ElementImporter ();
	
	/** Returns the element name
	 * @return the name of the element
	 */
	virtual string get_name () const { return name; };
	
	/** Gets a textual representation of the element
	 * @return a textual representation on this specific element
	 */
	virtual string get_info () const = 0;
	
	/** Prepares to move element
	 *
	 * @return whther or not the element could be prepared for moving
	 */
	bool prepare_move ();
	
	/** Cancels moving of element
	 * If the element has been set to be moved, this cancels the move.
	 */
	void cancel_move ();
	
	/// Moves the element to the taget session
	void move ();
	
	/// Check if element is broken. Cannot be moved if broken.
	bool broken () { return _broken; }
	
	/// Signal that requests for anew name
	static sigc::signal <std::pair<bool, string>, string, string> Rename;
	
	/// Signal for ok/cancel prompting
	static sigc::signal <bool, string> Prompt;
	
  protected:

	/** Moves the element to the taget session
	 * In addition to actually adding the element to the session
	 * changing ids, renaming files etc. should be taken care of.
	 */
	virtual void _move () = 0;

	/** Should take care of all tasks that need to be done 
	 * before moving the element. This includes prompting
	 * the user for more information if necessary.
	 *
	 * @return whether or not the element can be moved
	 */
	virtual bool _prepare_move () = 0;
	
	/// Cancel move 
	virtual void _cancel_move () = 0;

	/// Source XML-tree
	XMLTree const & source;
	
	/// Target session
	ARDOUR::Session & session;
	
	/// Ture if the element has been prepared and queued for importing
	bool queued () { return _queued; }
	
	/// Name of element
	string  name;
	
	/// The sample rate of the session from which we are importing
	nframes_t sample_rate;
	
	/// Converts smpte time to a string
	string smpte_to_string(SMPTE::Time & time) const;
	
	/// Converts samples so that times match the sessions sample rate
	nframes_t rate_convert_samples (nframes_t samples) const;
	
	/// Converts samples so that times match the sessions sample rate (for straight use in XML)
	string rate_convert_samples (string const & samples) const;
	
	/// Set element broken
	void set_broken () { _broken = true; }
	
  private:
	bool _queued;
	bool _broken;
};

} // namespace ARDOUR

#endif
