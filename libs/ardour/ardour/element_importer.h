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

#include "pbd/signals.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

class XMLTree;
namespace ARDOUR {

class Session;
class ImportStatus;

/// Virtual interface class for element importers
class LIBARDOUR_API ElementImporter
{
  public:

	ElementImporter (XMLTree const & source, ARDOUR::Session & session);
	virtual ~ElementImporter ();

	/** Returns the element name
	 * @return the name of the element
	 */
	virtual std::string get_name () const { return name; };

	/** Gets a textual representation of the element
	 * @return a textual representation on this specific element
	 */
	virtual std::string get_info () const = 0;

	/** Gets import status, if applicable. */
	virtual ImportStatus * get_import_status () { return 0; }

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
	static PBD::Signal2<std::pair<bool, std::string>,std::string, std::string> Rename;

	/// Signal for ok/cancel prompting
	static PBD::Signal1<bool,std::string> Prompt;

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
	std::string  name;

	/// The sample rate of the session from which we are importing
	framecnt_t sample_rate;

	/// Converts timecode time to a string
	std::string timecode_to_string (Timecode::Time & time) const;

	/// Converts samples so that times match the sessions sample rate
	framecnt_t rate_convert_samples (framecnt_t samples) const;

	/// Converts samples so that times match the sessions sample rate (for straight use in XML)
	std::string rate_convert_samples (std::string const & samples) const;

	/// Set element broken
	void set_broken () { _broken = true; }

  private:
	bool _queued;
	bool _broken;
};

} // namespace ARDOUR

#endif
