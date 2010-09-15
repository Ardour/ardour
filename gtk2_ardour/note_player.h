#ifndef __gtk2_ardour_note_player_h__
#define __gtk2_ardour_note_player_h__

#include <vector>
#include <boost/shared_ptr.hpp>
#include <sigc++/trackable.h>

#include "evoral/Note.hpp"

namespace ARDOUR {
        class MidiTrack;
}

class NotePlayer : public sigc::trackable {
  public:
        typedef Evoral::Note<Evoral::MusicalTime> NoteType;

        NotePlayer (boost::shared_ptr<ARDOUR::MidiTrack>);
        ~NotePlayer () {}

        void add (boost::shared_ptr<NoteType>);
        void play ();
        void off ();

        static bool _off (NotePlayer*);

  private:
        typedef std::vector<boost::shared_ptr<NoteType> > NoteList;

        boost::shared_ptr<ARDOUR::MidiTrack> track;
        NoteList notes;
};

#endif /* __gtk2_ardour_note_player_h__ */
