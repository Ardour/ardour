#include <string>
#include <vector>

#include "pbd/command.h"
#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/history_owner.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/undo.h"

using namespace std;
using namespace PBD;

HistoryOwner::HistoryOwner (std::string const & str)
	: _name (str)
	, _current_trans (nullptr)
{
}

HistoryOwner::~HistoryOwner()
{
	delete _current_trans;
}

void
HistoryOwner::add_commands (vector<Command*> const & cmds)
{
	for (vector<Command*>::const_iterator i = cmds.begin(); i != cmds.end(); ++i) {
		add_command (*i);
	}
}

void
HistoryOwner::add_command (Command* const cmd)
{
	assert (_current_trans);
	if (!_current_trans) {
		error << "Attempted to add an UNDO command without a current transaction.  ignoring command (" << cmd->name() <<  ")" << endl;
		return;
	}
	DEBUG_TRACE (DEBUG::UndoHistory,
	    string_compose ("Current Undo Transaction %1, adding command: %2\n",
	                    _current_trans->name (),
	                    cmd->name ()));
	_current_trans->add_command (cmd);
}

PBD::StatefulDiffCommand*
HistoryOwner::add_stateful_diff_command (std::shared_ptr<PBD::StatefulDestructible> sfd)
{
	PBD::StatefulDiffCommand* cmd = new PBD::StatefulDiffCommand (sfd);
	add_command (cmd);
	return cmd;
}

void
HistoryOwner::begin_reversible_command (const string& name)
{
	begin_reversible_command (g_quark_from_string (name.c_str ()));
}

/** Begin a reversible command using a GQuark to identify it.
 *  begin_reversible_command() and commit_reversible_command() calls may be nested,
 *  but there must be as many begin...()s as there are commit...()s.
 */
void
HistoryOwner::begin_reversible_command (GQuark q)
{
	if (_current_trans) {
#ifndef NDEBUG
		cerr << "An UNDO transaction was started while a prior command was underway. Aborting command (" << g_quark_to_string (q) << ") and prior (" << _current_trans->name() << ")" << "\n";
#else
		PBD::warning << "An UNDO transaction was started while a prior command was underway. Aborting command (" << g_quark_to_string (q) << ") and prior (" << _current_trans->name() << ")" << endmsg;
#endif
		abort_reversible_command();
		assert (false);
		return;
	}

	/* If nested begin/commit pairs are used, we create just one UndoTransaction
	   to hold all the commands that are committed.  This keeps the order of
	   commands correct in the history.
	*/

	if (_current_trans == 0) {
		DEBUG_TRACE (DEBUG::UndoHistory, string_compose ("%2 Begin Reversible Command, new transaction: %1\n", g_quark_to_string (q), _name));

		/* start a new transaction */
		assert (_current_trans_quarks.empty ());
		_current_trans = new UndoTransaction();
		_current_trans->set_name (g_quark_to_string (q));
	} else {
		DEBUG_TRACE (DEBUG::UndoHistory, string_compose ("%2 Begin Reversible Command, current transaction: %1\n", _current_trans->name (), _name));
	}

	_current_trans_quarks.push_front (q);
}

void
HistoryOwner::abort_reversible_command ()
{
	if (!_current_trans) {
		return;
	}
	DEBUG_TRACE (DEBUG::UndoHistory, string_compose ("%2 Abort Reversible Command: %1\n", _current_trans->name (), _name));
	_current_trans->clear();
	delete _current_trans;
	_current_trans = nullptr;
	_current_trans_quarks.clear();
}

bool
HistoryOwner::abort_empty_reversible_command ()
{
	if (!collected_undo_commands ()) {
		abort_reversible_command ();
		return true;
	}
	return false;
}

void
HistoryOwner::commit_reversible_command (Command *cmd)
{
	assert (_current_trans);
	assert (!_current_trans_quarks.empty ());
	if (!_current_trans) {
		return;
	}

	struct timeval now;

	if (cmd) {
		DEBUG_TRACE (DEBUG::UndoHistory,
		    string_compose ("%3 Current Undo Transaction %1, adding command: %2\n",
		                    _current_trans->name (),
		                    cmd->name (), _name));
		_current_trans->add_command (cmd);
	}

	DEBUG_TRACE (DEBUG::UndoHistory,
	    string_compose ("%2 Commit Reversible Command, current transaction: %1\n",
	                    _current_trans->name (), _name));

	_current_trans_quarks.pop_front ();

	if (!_current_trans_quarks.empty ()) {
		DEBUG_TRACE (DEBUG::UndoHistory,
		    string_compose ("%2 Commit Reversible Command, transaction is not "
		                    "top-level, current transaction: %1\n",
		                    _current_trans->name (), _name));
		/* the transaction we're committing is not the top-level one */
		return;
	}

	if (_current_trans->empty()) {
		/* no commands were added to the transaction, so just get rid of it */
		DEBUG_TRACE (DEBUG::UndoHistory,
		    string_compose ("%2 Commit Reversible Command, No commands were "
		                    "added to current transaction: %1\n",
		                    _current_trans->name (), _name));
		delete _current_trans;
		_current_trans = nullptr;
		return;
	}

	gettimeofday (&now, 0);
	_current_trans->set_timestamp (now);

	DEBUG_TRACE (DEBUG::UndoHistory,
	             string_compose ("%2 Commit Reversible Command, add to history %1\n",
	                             _current_trans->name (), _name));
	_history.add (_current_trans);
	_current_trans = nullptr;
}

bool
HistoryOwner::operation_in_progress (GQuark op) const
{
	return (find (_current_trans_quarks.begin(), _current_trans_quarks.end(), op) != _current_trans_quarks.end());
}

