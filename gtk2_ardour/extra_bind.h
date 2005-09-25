/*
    Copyright (C) 2002 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_extra_bind_h__
#define __ardour_extra_bind_h__

#include <sigc++/adaptor.h>
#include <sigc++/scope.h>

namespace sigc
{

/****************************************************************
 ***** Adaptor Bind Slot 0 arguments, 3 hidden arguments
 ****************************************************************/
template <class R,
   class C1, class C2, class C3>
struct AdaptorBindSlot0_3: public AdaptorSlot_
  {
#ifdef SIGC_CXX_PARTIAL_SPEC
   typedef R RType;
#else
   typedef typename Trait<R>::type RType;
#endif
   typedef Slot0<R> SlotType;
   typedef Slot3<R,C1,C2,C3> InSlotType;

   struct Node:public AdaptorNode
     {
      C1 c1_;
      C2 c2_;
      C3 c3_;	 
     };

   typedef CallDataObj2<typename SlotType::Func,Node> CallData;

   static RType callback(void* d)
     {
      CallData* data=(CallData*)d;
      Node* node=data->obj;
      return ((typename InSlotType::Callback&)(node->data_))(
                                     node->c1_,
				     node->c2_,
				     node->c3_);
     }
   static SlotData* create(SlotData *s,C1 c1, C2 c2, C3 c3)
     {
      SlotData* tmp=(SlotData*)s;
      Node *node=new Node();
      copy_callback(tmp,node);
      node->c1_=c1; 
      node->c2_=c2; 
      node->c3_=c3; 
      CallData &data=reinterpret_cast<CallData&>(tmp->data_);
      data.callback=&callback;
      data.obj=node;
      return tmp;
     }
  };


#ifndef SIGC_CXX_VOID_RETURN
#ifdef SIGC_CXX_PARTIAL_SPEC
template <
   class C1,C2,C3>
struct AdaptorBindSlot0_3
   <void,
    C1,C2,C3> : public AdaptorSlot_
  {
   typedef void RType;
   typedef Slot0<void> SlotType;
   typedef Slot3<void,C1,C2,C3> InSlotType;

   struct Node:public AdaptorNode
     {
      C1 c1_;
      C2 c2_;
      C3 c3_;
     };

   typedef CallDataObj2<typename SlotType::Func,Node> CallData;

   static RType callback(void* d)
     {
      CallData* data=(CallData*)d;
      Node* node=data->obj;
       ((typename InSlotType::Callback&)(node->data_))(
                                     node->c1_,
                                     node->c2_,
                                     node->c3);
     }
   static SlotData* create(SlotData *s,C1 c1, C2 c2, C3 c3)
     {
      SlotData* tmp=(SlotData*)s;
      Node *node=new Node();
      copy_callback(tmp,node);
      node->c1_=c1; 
      node->c2_=c2; 
      node->c3_=c3; 
      CallData &data=reinterpret_cast<CallData&>(tmp->data_);
      data.callback=&callback;
      data.obj=node;
      return tmp;
     }
  };

#endif
#endif

template <class C1, class C2, class C3,
    class R>
Slot0<R>
  bind(const Slot3<R,C1,C2,C3> &s,
       C1 c1, C2 c2, C3 c3)
  {return AdaptorBindSlot0_3<R,
           C1,C2,C3>::create(s.data(),c1,c2,c3);
  }

} /* namespace */

#endif /* __ardour_extra_bind_h__ */
