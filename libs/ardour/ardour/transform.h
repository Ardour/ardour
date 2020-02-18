/*
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_transform_h__
#define __ardour_transform_h__

#include <stack>
#include <string>

#include "ardour/libardour_visibility.h"
#include "ardour/midi_model.h"
#include "ardour/midi_operator.h"
#include "ardour/types.h"
#include "ardour/variant.h"

namespace ARDOUR {

/** Transform notes with a user-defined transformation.
 *
 * This is essentially an interpreter for a simple concatenative note
 * transformation language (as an AST only, no source code).  A "program"
 * calculates a note property value from operations on literal values, and/or
 * values from the current or previous note in the sequence.  This allows
 * simple things like "set all notes' velocity to 64" or transitions over time
 * like "set velocity to the previous note's velocity + 10".
 *
 * The language is forth-like: everything is on a stack, operations pop their
 * arguments from the stack and push their result back on to it.
 *
 * This is a sweet spot between simplicity and power, it should be simple to
 * use this (with perhaps some minor extensions) to do most "linear-ish"
 * transformations, though it could be extended to have random access
 * and more special values as the need arises.
 */
class LIBARDOUR_API Transform : public MidiOperator {
public:
	typedef Evoral::Sequence<Temporal::Beats>::NotePtr   NotePtr;
	typedef Evoral::Sequence<Temporal::Beats>::Notes     Notes;
	typedef ARDOUR::MidiModel::NoteDiffCommand::Property Property;

	/** Context while iterating over notes during transformation. */
	struct Context {
		Context() : index(0) {}

		Variant pop();

		std::stack<Variant> stack;      ///< The stack of everything
		size_t              index;      ///< Index of current note
		size_t              n_notes;    ///< Total number of notes to process
		NotePtr             prev_note;  ///< Previous note
		NotePtr             this_note;  ///< Current note
	};

	/** Value in a transformation expression. */
	struct Value {
		/** Value source.  Some of these would be better modeled as properties,
		    like note.index or sequence.size, but until the sequence stuff is
		    more fundamentally property based, we special-case them here. */
		enum Source {
			NOWHERE,    ///< Null
			THIS_NOTE,  ///< Value from this note
			PREV_NOTE,  ///< Value from the previous note
			INDEX,      ///< Index of the current note
			N_NOTES,    ///< Total number of notes to process
			LITERAL,    ///< Given literal value
			RANDOM      ///< Random normal
		};

		Value()                 : source(NOWHERE) {}
		Value(Source s)         : source(s) {}
		Value(const Variant& v) : source(LITERAL), value(v) {}
		Value(double v)         : source(LITERAL), value(Variant(v)) {}

		/** Calculate and return value. */
		Variant eval(const Context& context) const;

		Source   source;  ///< Source of value
		Variant  value;   ///< Value for LITERAL
		Property prop;    ///< Property for all other sources
	};

	/** An operation to transform the running result.
	 *
	 * All operations except PUSH take their arguments from the stack, and put
	 * the result back on the stack.
	 */
	struct Operation {
		enum Operator {
			PUSH,  ///< Push argument to the stack
			ADD,   ///< Add top two values
			SUB,   ///< Subtract top from second-top
			MULT,  ///< Multiply top two values
			DIV,   ///< Divide second-top by top
			MOD    ///< Modulus (division remainder)
		};

		Operation(Operator o, const Value& a=Value()) : op(o), arg(a) {}

		/** Apply operation. */
		void eval(Context& context) const;

		Operator op;
		Value    arg;
	};

	/** A transformation program.
	 *
	 * A program is a list of operations to calculate the target property's
	 * final value.  The first operation must be a PUSH to seed the stack.
	 */
	struct Program {
		Property             prop;  ///< Property to calculate
		std::list<Operation> ops;   ///< List of operations
	};

	Transform(const Program& prog);

	Command* operator()(boost::shared_ptr<ARDOUR::MidiModel> model,
	                    Temporal::Beats                      position,
	                    std::vector<Notes>&                  seqs);

	std::string name() const { return std::string ("transform"); }

private:
	const Program _prog;
};

} /* namespace */

#endif /* __ardour_transform_h__ */
