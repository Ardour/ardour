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

#ifndef __freezable_h__
#define __freezable_h__

#include <boost/shared_ptr.hpp>
#include "ardour/interthread_info.h"
#include "ardour/playlist.h"

namespace ARDOUR {

/** 
 * Interface Freezable 
 * Allows an object (ex: Track) to be freezen
*/
class Freezable
{
 public:
  enum FreezeState {NoFreeze,
		    Frozen,
		    UnFrozen};
  
  //private: (FIXME)
  struct FreezeRecordProcessorInfo {
  FreezeRecordProcessorInfo(XMLNode& st, boost::shared_ptr<ARDOUR::Processor> proc)
  : state (st), processor (proc) {}

    XMLNode                      state;
    boost::shared_ptr<ARDOUR::Processor> processor;
    PBD::ID                      id;
  };

  struct FreezeRecord {FreezeRecord(): have_mementos(false){}
    ~FreezeRecord()
    {
      for (std::vector<FreezeRecordProcessorInfo*>::iterator i = processor_info.begin(); i != processor_info.end(); ++i) {
	delete *i;
      }
    }
    boost::shared_ptr<Playlist>        playlist;
    std::vector<FreezeRecordProcessorInfo*> processor_info;
    bool have_mementos;
    FreezeState                        state;};

  virtual FreezeState freeze_state() const = 0;
  virtual void freeze_me (InterThreadInfo&) = 0;
  virtual void unfreeze () = 0;
};

}

#endif
