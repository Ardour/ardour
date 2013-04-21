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

#ifndef __ardour_operation_h__
#define __ardour_operation_h__

#include <boost/shared_ptr.hpp>

#include "ardour/interthread_info.h"

namespace ARDOUR {

  class AudioTrack;
  class MidiTrack;

  /**
   * Represents an Operation to be applied on an object. 
   * It's an abstract singleton class, inspired by Visitor pattern. 
   * Each subclass has to implement the apply and disapply methods 
   * on each type they want to operate on. For example, FreezeOperation 
   * has to implement theese methods for the AudioTrack, MidiTrack and 
   * CommentTrack types because theese types of object can 
   * be frozen (they implement Freezable interface).
   * @see FreezeOperation
   */
  class Operation
  {

  public:

    /**
     * Pure abstract methods to define how the operation will be applied 
     * to each overloaded type.
     */
    virtual void apply(AudioTrack* track) = 0;
    virtual void apply(MidiTrack* track) = 0;

    /**
     * Pure abstract methods to define how the operation will be disapplied 
     * to each overloaded type.
     */
    virtual void disapply(AudioTrack* track) = 0;
    virtual void disapply(MidiTrack* track) = 0;

  protected:
    virtual ~Operation(){}

  };

}; /* namespace ARDOUR*/

#endif /* __ardour_operation_h__ */

  
