// -*- c++ -*-
/*
 * Copyright 2003, The libsigc++ Development Team
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <sigc++/functors/slot_base.h>

namespace sigc
{

namespace internal {

// only MSVC needs this to guarantee that all new/delete are executed from the DLL module
#ifdef SIGC_NEW_DELETE_IN_LIBRARY_ONLY
void* slot_rep::operator new(size_t size_)
{
  return malloc(size_);
}

void slot_rep::operator delete(void* p)
{
  free(p);
}
#endif

void slot_rep::disconnect()
{
  if (parent_)
  {
    call_ = 0;          // Invalidate the slot.
                        // _Must_ be done here because parent_ might defer the actual
                        // destruction of the slot_rep and try to invoke it before that point.
    void* data_ = parent_;
    parent_ = 0;        // Just a precaution.
    (cleanup_)(data_);  // Notify the parent (might lead to destruction of this!).
  }
  else
    call_ = 0;
}

//static
void* slot_rep::notify(void* data)
{
  slot_rep* self_ = reinterpret_cast<slot_rep*>(data);

  self_->call_ = 0; // Invalidate the slot.
  self_->destroy(); // Detach the stored functor from the other referred trackables and destroy it.
  self_->disconnect(); // Disconnect the slot (might lead to deletion of self_!).

  return 0;
}

} // namespace internal
  
slot_base::slot_base()
: rep_(0),
  blocked_(false)
{}

slot_base::slot_base(rep_type* rep)
: rep_(rep),
  blocked_(false)
{}

slot_base::slot_base(const slot_base& src)
: rep_(0),
  blocked_(src.blocked_)
{
  if (src.rep_)
  {
    //Check call_ so we can ignore invalidated slots.
    //Otherwise, destroyed bound reference parameters (whose destruction caused the slot's invalidation) may be used during dup().
    //Note: I'd prefer to check somewhere during dup(). murrayc.
    if (src.rep_->call_)
      rep_ = src.rep_->dup();
    else
    {
      *this = slot_base(); //Return the default invalid slot.
    }
  }
}

slot_base::~slot_base()
{
  if (rep_)
    delete rep_;
}

slot_base::operator bool() const
{
  return rep_ != 0;
}

slot_base& slot_base::operator=(const slot_base& src)
{
  if (src.rep_ == rep_) return *this;

  if (src.empty())
  {
    disconnect();
    return *this;
  }

  internal::slot_rep* new_rep_ = src.rep_->dup();

  if (rep_)               // Silently exchange the slot_rep.
  {
    new_rep_->set_parent(rep_->parent_, rep_->cleanup_);
    delete rep_;
  }

  rep_ = new_rep_;

  return *this;
}

void slot_base::set_parent(void* parent, void* (*cleanup)(void*)) const
{
  if (rep_)
    rep_->set_parent(parent, cleanup);
}

void slot_base::add_destroy_notify_callback(void* data, func_destroy_notify func) const
{
  if (rep_)
    rep_->add_destroy_notify_callback(data, func);
}

void slot_base::remove_destroy_notify_callback(void* data) const
{
  if (rep_)
    rep_->remove_destroy_notify_callback(data);
}

bool slot_base::block(bool should_block)
{
  bool old = blocked_;
  blocked_ = should_block;
  return old;
}

bool slot_base::unblock()
{
  return block(false);
}

void slot_base::disconnect()
{
  if (rep_)
    rep_->disconnect();
}


/*bool slot_base::empty() const // having this function not inline is killing performance !!!
{
  if (rep_ && !rep_->call_)
    {
      delete rep_;        // This is not strictly necessary here. I'm convinced that it is
      rep_ = 0;           // safe to wait for the destructor to delete the slot_rep. Martin.
    }
  return (rep_ == 0);
}*/

} //namespace sigc
