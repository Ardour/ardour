#pragma once

#include <string>
#include <vector>
#include <list>

#include <glib.h>

#include "pbd/undo.h"
#include "pbd/libpbd_visibility.h"

namespace PBD {
	class Command;


class LIBPBD_API HistoryOwner
{
  public:
	HistoryOwner (std::string const & name);
	virtual ~HistoryOwner();

	/** begin collecting undo information
	 *
	 * This call must always be followed by either
	 * begin_reversible_command() or commit_reversible_command()
	 *
	 * @param cmd_name human readable name for the undo operation
	 */
	void begin_reversible_command (const std::string& cmd_name);
	void begin_reversible_command (GQuark);
	/** abort an open undo command
	 * This must only be called after begin_reversible_command ()
	 */
	void abort_reversible_command ();
	/** finalize an undo command and commit pending transactions
	 *
	 * This must only be called after begin_reversible_command ()
	 * @param cmd (additional) command to add
	 */
	void commit_reversible_command (PBD::Command* cmd = 0);

	void add_command (PBD::Command *const cmd);

	/** create an StatefulDiffCommand from the given object and add it to the stack.
	 *
	 * This function must only be called after  begin_reversible_command.
	 * Failing to do so may lead to a crash.
	 *
	 * @param sfd the object to diff
	 * @returns the allocated StatefulDiffCommand (already added via add_command)
	 */
	PBD::StatefulDiffCommand* add_stateful_diff_command (std::shared_ptr<PBD::StatefulDestructible> sfd);

	/** @return The list of operations that are currently in progress */
	std::list<GQuark> const & current_operations () {
		return _current_trans_quarks;
	}

	bool operation_in_progress (GQuark) const;

	/**
	 * Test if any undo commands were added since the
	 * call to begin_reversible_command ()
	 *
	 * This is useful to determine if an undoable
	 * action was performed before adding additional
	 * information (e.g. selection changes) to the
	 * undo transaction.
	 *
	 * @return true if undo operation is valid but empty
	 */
	bool collected_undo_commands () const {
		return _current_trans && !_current_trans->empty ();
	}

	PBD::UndoTransaction* current_reversible_command() { return _current_trans; }

	/**
	 * Abort reversible command IFF no undo changes
	 * have been collected.
	 * @return true if undo operation was aborted.
	 */
	bool abort_empty_reversible_command ();

	void add_commands (std::vector<PBD::Command*> const & cmds);

	PBD::UndoHistory& undo_redo() { return _history; }

  protected:
	std::string           _name;
	PBD::UndoHistory      _history;
	/** current undo transaction, or 0 */
	PBD::UndoTransaction* _current_trans;
	/** GQuarks to describe the reversible commands that are currently in progress.
	 *  These may be nested, in which case more recently-started commands are toward
	 *  the front of the list.
	 */
	std::list<GQuark>     _current_trans_quarks;
};

} /* namespace PBD */

