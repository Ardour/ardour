/*
    Copyright (C) 2006 Paul Davis

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

#ifndef __ardour_freeze_operation_h__
#define __ardour_freeze_operation_h__

#include "ardour/freezable.h"
#include "ardour/route.h"
#include "ardour/operation.h"

#include "ardour/track.h"
#include "ardour/audio_track.h"
#include "ardour/midi_track.h"

namespace ARDOUR {

  /**
   * Represents the Ardour freeze operation to be applied on an object. 
   * @see Operation
   */
  class FreezeOperation : public Operation
  {

  public:
    /**
     * @return the only one FreezeOperation object
     */ 
    static FreezeOperation* get_instance();

    void apply(AudioTrack* track);
    void apply(MidiTrack* track);

    void disapply(AudioTrack* track);
    void disapply(MidiTrack* track);

  private:
    static FreezeOperation* _instance; /** Singleton instance */

    FreezeOperation();
    ~FreezeOperation();

  };

}; /* namespace ARDOUR*/

#endif /* __ardour_freeze_operation_h__ */
