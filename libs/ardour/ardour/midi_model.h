/*
 * Copyright (C) 2007-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_midi_model_h__
#define __ardour_midi_model_h__

#include <deque>
#include <map>
#include <queue>
#include <utility>

#include <boost/utility.hpp>
#include <glibmm/threads.h>

#include "pbd/command.h"

#include "ardour/libardour_visibility.h"
#include "ardour/automation_list.h"
#include "ardour/automatable_sequence.h"
#include "ardour/source.h"
#include "ardour/types.h"
#include "ardour/types.h"
#include "ardour/variant.h"

#include "evoral/Note.h"
#include "evoral/Sequence.h"

namespace ARDOUR {

class Session;
class MidiSource;

/** This is a higher level (than MidiBuffer) model of MIDI data, with separate
 * representations for notes (instead of just unassociated note on/off events)
 * and controller data.  Controller data is represented as part of the
 * Automatable base (i.e. in a map of AutomationList, keyed by Parameter).
 * Because of this MIDI controllers and automatable controllers/widgets/etc
 * are easily interchangeable.
 */
class LIBARDOUR_API MidiModel : public AutomatableSequence<Temporal::Beats> {
public:
	typedef Temporal::Beats TimeType;

	MidiModel (MidiSource&);

	class LIBARDOUR_API DiffCommand : public PBD::Command {
	public:

		DiffCommand (std::shared_ptr<MidiModel> m, const std::string& name);

		const std::string& name () const { return _name; }

		virtual void operator() () = 0;
		virtual void undo () = 0;

		virtual int set_state (const XMLNode&, int version) = 0;
		virtual XMLNode & get_state () const = 0;

		std::shared_ptr<MidiModel> model() const { return _model; }

	protected:
		std::shared_ptr<MidiModel> _model;
		const std::string            _name;

	};

	class LIBARDOUR_API NoteDiffCommand : public DiffCommand {
	public:

		NoteDiffCommand (std::shared_ptr<MidiModel> m, const std::string& name) : DiffCommand (m, name) {}
		NoteDiffCommand (std::shared_ptr<MidiModel> m, const XMLNode& node);

		enum Property {
			NoteNumber,
			Velocity,
			StartTime,
			Length,
			Channel
		};

		void operator() ();
		void undo ();

		int set_state (const XMLNode&, int version);
		XMLNode & get_state () const;

		void add (const NotePtr note);
		void remove (const NotePtr note);
		void side_effect_remove (const NotePtr note);

		void change (const NotePtr note, Property prop, uint8_t new_value) {
			change(note, prop, Variant(new_value));
		}

		void change (const NotePtr note, Property prop, TimeType new_time) {
			change(note, prop, Variant(new_time));
		}

		void change (const NotePtr note, Property prop, const Variant& new_value);

		bool adds_or_removes() const {
			return !_added_notes.empty() || !_removed_notes.empty();
		}

		NoteDiffCommand& operator+= (const NoteDiffCommand& other);

		static Variant get_value (const NotePtr note, Property prop);

		static Variant::Type value_type (Property prop);

		struct NoteChange {
			NoteDiffCommand::Property property;
			NotePtr note;
			uint32_t note_id;
			Variant old_value;
			Variant new_value;
		};

		typedef std::list<NoteChange>                                    ChangeList;
		typedef std::list< std::shared_ptr< Evoral::Note<TimeType> > > NoteList;

		const ChangeList& changes()       const { return _changes; }
		const NoteList&   added_notes()   const { return _added_notes; }
		const NoteList&   removed_notes() const { return _removed_notes; }

	private:
		ChangeList _changes;
		NoteList   _added_notes;
		NoteList   _removed_notes;

		std::set<NotePtr> side_effect_removals;

		XMLNode &marshal_change(const NoteChange&) const;
		NoteChange unmarshal_change(XMLNode *xml_note);

		XMLNode &marshal_note(const NotePtr note) const;
		NotePtr unmarshal_note(XMLNode *xml_note);
	};

	/* Currently this class only supports changes of sys-ex time, but could be expanded */
	class LIBARDOUR_API SysExDiffCommand : public DiffCommand {
	public:
		SysExDiffCommand (std::shared_ptr<MidiModel> m, const XMLNode& node);

		enum Property {
			Time,
		};

		int set_state (const XMLNode&, int version);
		XMLNode & get_state () const;

		void remove (SysExPtr sysex);
		void operator() ();
		void undo ();

		void change (std::shared_ptr<Evoral::Event<TimeType> >, TimeType);

	private:
		struct Change {
			Change () : sysex_id (0) {}
			std::shared_ptr<Evoral::Event<TimeType> > sysex;
			gint sysex_id;
			SysExDiffCommand::Property property;
			TimeType old_time;
			TimeType new_time;
		};

		typedef std::list<Change> ChangeList;
		ChangeList _changes;

		std::list<SysExPtr> _removed;

		XMLNode & marshal_change (const Change &) const;
		Change unmarshal_change (XMLNode *);
	};

	class LIBARDOUR_API PatchChangeDiffCommand : public DiffCommand {
	public:
		PatchChangeDiffCommand (std::shared_ptr<MidiModel>, const std::string &);
		PatchChangeDiffCommand (std::shared_ptr<MidiModel>, const XMLNode &);

		int set_state (const XMLNode &, int version);
		XMLNode & get_state () const;

		void operator() ();
		void undo ();

		void add (PatchChangePtr);
		void remove (PatchChangePtr);
		void change_time (PatchChangePtr, TimeType);
		void change_channel (PatchChangePtr, uint8_t);
		void change_program (PatchChangePtr, uint8_t);
		void change_bank (PatchChangePtr, int);

		enum Property {
			Time,
			Channel,
			Program,
			Bank
		};

	private:
		struct Change {
			PatchChangePtr patch;
			Property       property;
			gint           patch_id;
			TimeType       old_time;
			union {
				uint8_t    old_channel;
				int        old_bank;
				uint8_t    old_program;
			};
			TimeType       new_time;
			union {
				uint8_t    new_channel;
				uint8_t    new_program;
				int        new_bank;
			};

		    Change() : patch_id (-1) {}
		};

		typedef std::list<Change> ChangeList;
		ChangeList _changes;

		std::list<PatchChangePtr> _added;
		std::list<PatchChangePtr> _removed;

		XMLNode & marshal_change (const Change &) const;
		Change unmarshal_change (XMLNode *);

		XMLNode & marshal_patch_change (constPatchChangePtr) const;
		PatchChangePtr unmarshal_patch_change (XMLNode *);
	};

	void create_mapping_stash (Temporal::Beats const & offset);
	void rebuild_from_mapping_stash (Temporal::Beats const & offset);

	/** Start a new NoteDiff command.
	 *
	 * This has no side-effects on the model or Session, the returned command
	 * can be held on to for as long as the caller wishes, or discarded without
	 * formality, until apply_diff_command_* is called and ownership is taken.
	 */
	MidiModel::NoteDiffCommand* new_note_diff_command (const std::string& name = "midi edit");
	/** Start a new SysExDiff command */
	MidiModel::SysExDiffCommand* new_sysex_diff_command (const std::string& name = "midi edit");

	/** Start a new PatchChangeDiff command */
	MidiModel::PatchChangeDiffCommand* new_patch_change_diff_command (const std::string& name = "midi edit");

	/** Apply a command.
	 *
	 * Ownership of cmd is taken, it must not be deleted by the caller.
	 * This STARTS and COMMITS an undo command.
	 * The command will constitute one item on the undo stack.
	 */
	void apply_diff_command_as_commit (Session& session, PBD::Command* cmd);

	void apply_diff_command_as_commit (Session* session, PBD::Command* cmd) { if (session) { apply_diff_command_as_commit (*session, cmd); } }

	/** Add a command as part of a larger reversible transaction
	 *
	 * Ownership of cmd is taken, it must not be deleted by the caller.
	 * The command will be incorporated into the current command.
	 */
	void apply_diff_command_as_subcommand (Session& session, PBD::Command* cmd);

	/** Apply the midi diff, but without any effect on undo
	 *
	 * Ownership of cmd is not changed.
	 */
	void apply_diff_command_only (Session& session, PBD::Command* cmd);

	bool sync_to_source (const Source::WriterLock& source_lock);

	bool write_to(std::shared_ptr<MidiSource>     source,
	              const Source::WriterLock& source_lock);

	bool write_section_to(std::shared_ptr<MidiSource>     source,
	                      const Source::WriterLock& source_lock,
	                      Temporal::Beats                   begin = Temporal::Beats(),
	                      Temporal::Beats                   end   = std::numeric_limits<Temporal::Beats>::max(),
	                      bool                              offset_events = false);

	// MidiModel doesn't use the normal AutomationList serialisation code
	// since controller data is stored in the .mid
	XMLNode& get_state() const;
	int set_state(const XMLNode&) { return 0; }

	PBD::Signal0<void> ContentsChanged;
	PBD::Signal1<void, Temporal::timecnt_t> ContentsShifted;

	std::shared_ptr<Evoral::Note<TimeType> > find_note (NotePtr);
	PatchChangePtr find_patch_change (Evoral::event_id_t);
	std::shared_ptr<Evoral::Note<TimeType> > find_note (Evoral::event_id_t);
	std::shared_ptr<Evoral::Event<TimeType> > find_sysex (Evoral::event_id_t);

	InsertMergePolicy insert_merge_policy () const;
	void set_insert_merge_policy (InsertMergePolicy);

	std::shared_ptr<Evoral::Control> control_factory(const Evoral::Parameter& id);

	void insert_silence_at_start (TimeType);
	void transpose (NoteDiffCommand *, const NotePtr, int);

  protected:
	int resolve_overlaps_unlocked (const NotePtr, void* arg = 0);

  protected:
	friend class NoteDiffCommand;
	friend class SysExDiffCommand;
	friend class PatchChangeDiffCommand;

	MidiSource& midi_source() const { return _midi_source; }

  private:
	struct WriteLockImpl : public AutomatableSequence<TimeType>::WriteLockImpl {
		WriteLockImpl(Source::WriterLock* slock, Glib::Threads::RWLock& s, Glib::Threads::Mutex& c)
			: AutomatableSequence<TimeType>::WriteLockImpl(s, c)
			, source_lock (slock)
		{}
		~WriteLockImpl() {
			delete source_lock;
		}
		Source::WriterLock* source_lock;
	};

public:
	WriteLock edit_lock();

private:
	friend class DeltaCommand;

	void source_interpolation_changed (Evoral::Parameter const&, AutomationList::InterpolationStyle);
	void source_automation_state_changed (Evoral::Parameter const&, AutoState);
	void control_list_interpolation_changed (Evoral::Parameter const&, AutomationList::InterpolationStyle);
	void automation_list_automation_state_changed (Evoral::Parameter const&, AutoState);

	void control_list_marked_dirty ();

	PBD::ScopedConnectionList _midi_source_connections;

	MidiSource& _midi_source;
	InsertMergePolicy _insert_merge_policy;

	typedef std::map<void*,superclock_t> TempoMappingStash;
	TempoMappingStash tempo_mapping_stash;

};

} /* namespace ARDOUR */

#endif /* __ardour_midi_model_h__ */

