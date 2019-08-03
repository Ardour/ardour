/*
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <glib.h>

#include "ardour/transform.h"
#include "ardour/midi_model.h"

namespace ARDOUR {

Transform::Transform(const Program& prog)
	: _prog(prog)
{}

Variant
Transform::Context::pop()
{
	if (stack.empty()) {
		return Variant();
	}

	const Variant top = stack.top();
	stack.pop();
	return top;
}

Variant
Transform::Value::eval(const Context& ctx) const
{
	switch (source) {
	case NOWHERE:
		return Variant();
	case THIS_NOTE:
		return MidiModel::NoteDiffCommand::get_value(ctx.this_note, prop);
	case PREV_NOTE:
		if (!ctx.prev_note) {
			return Variant();
		}
		return MidiModel::NoteDiffCommand::get_value(ctx.prev_note, prop);
	case INDEX:
		return Variant(Variant::INT, ctx.index);
	case N_NOTES:
		return Variant(Variant::INT, ctx.n_notes);
	case LITERAL:
		return value;
	case RANDOM:
		return Variant(g_random_double());
	}

	return Variant();
}

void
Transform::Operation::eval(Context& ctx) const
{
	if (op == PUSH) {
		const Variant a = arg.eval(ctx);
		if (!!a) {
			/* Argument evaluated to a value, push it to the stack.  Otherwise,
			   there was a reference to the previous note, but this is the
			   first, so skip this operation and do nothing. */
			ctx.stack.push(a);
		}
		return;
	}

	// Pop operands off the stack
	const Variant rhs = ctx.pop();
	const Variant lhs = ctx.pop();
	if (!lhs || !rhs) {
		// Stack underflow (probably previous note reference), do nothing
		return;
	}

	// We can get away with just using double math and converting twice
	double value = lhs.to_double();
	switch (op) {
	case ADD:
		value += rhs.to_double();
		break;
	case SUB:
		value -= rhs.to_double();
		break;
	case MULT:
		value *= rhs.to_double();
		break;
	case DIV:
		if (rhs.to_double() == 0.0) {
			return;  // Program will fail safely
		}
		value /= rhs.to_double();
		break;
	case MOD:
		if (rhs.to_double() == 0.0) {
			return;  // Program will fail safely
		}
		value = fmod(value, rhs.to_double());
		break;
	default: break;
	}

	// Push result on to the stack
	ctx.stack.push(Variant(lhs.type(), value));
}

Command*
Transform::operator()(boost::shared_ptr<MidiModel> model,
                      Temporal::Beats              position,
                      std::vector<Notes>&          seqs)
{
	typedef MidiModel::NoteDiffCommand Command;

	Command* cmd = new Command(model, name());

	for (std::vector<Notes>::iterator s = seqs.begin(); s != seqs.end(); ++s) {
		Context ctx;
		ctx.n_notes = (*s).size();
		for (Notes::const_iterator i = (*s).begin(); i != (*s).end(); ++i) {
			const NotePtr note = *i;

			// Clear stack and run program
			ctx.stack     = std::stack<Variant>();
			ctx.this_note = note;
			for (std::list<Operation>::const_iterator o = _prog.ops.begin();
			     o != _prog.ops.end();
			     ++o) {
				(*o).eval(ctx);
			}

			// Result is on top of the stack
			if (!ctx.stack.empty() && !!ctx.stack.top()) {
				// Get the result from the top of the stack
				Variant result = ctx.stack.top();
				if (result.type() != Command::value_type(_prog.prop)) {
					// Coerce to appropriate type
					result = Variant(Command::value_type(_prog.prop),
					                 result.to_double());
				}

				// Apply change
				cmd->change(note, _prog.prop, result);
			}
			// else error or reference to note before the first, skip

			// Move forward
			ctx.prev_note = note;
			++ctx.index;
		}
	}

	return cmd;
}

}  // namespace ARDOUR
