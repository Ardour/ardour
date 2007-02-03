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
#include <sigc++/signal_base.h>

namespace sigc {
namespace internal {

signal_impl::signal_impl()
: ref_count_(0), exec_count_(0), deferred_(0)
{}

// only MSVC needs this to guarantee that all new/delete are executed from the DLL module
#ifdef SIGC_NEW_DELETE_IN_LIBRARY_ONLY
void* signal_impl::operator new(size_t size_)
{
  return malloc(size_);
}

void signal_impl::operator delete(void* p)
{
  free(p);
}
#endif

void signal_impl::clear()
{
  slots_.clear();
}

signal_impl::size_type signal_impl::size() const
{
  return slots_.size();
}

signal_impl::iterator_type signal_impl::connect(const slot_base& slot_)
{
  return insert(slots_.end(), slot_);
}

signal_impl::iterator_type signal_impl::erase(iterator_type i)
{
  return slots_.erase(i);
}
    
signal_impl::iterator_type signal_impl::insert(signal_impl::iterator_type i, const slot_base& slot_)
{
  iterator_type temp = slots_.insert(i, slot_);
  temp->set_parent(this, &notify);
  return temp;
}

void signal_impl::sweep()
{
  deferred_ = false;
  iterator_type i = slots_.begin();
  while (i != slots_.end())
    if ((*i).empty())
      i = slots_.erase(i);
    else
      ++i;
}

void* signal_impl::notify(void* d)
{
  signal_impl* self = reinterpret_cast<signal_impl*>(d);
  if (self->exec_count_ == 0)
    self->sweep();
  else                       // This is occuring during signal emission.
    self->deferred_ = true;  // => sweep() will be called from ~signal_exec().
  return 0;                  // This is safer because we don't have to care about our iterators in emit().
}

} /* namespace internal */

signal_base::signal_base()
: impl_(0)
{}

signal_base::signal_base(const signal_base& src)
: trackable(),
  impl_(src.impl())
{
  impl_->reference();
}

signal_base::~signal_base()
{
  if (impl_)
    impl_->unreference();
}

void signal_base::clear()
{
  if (impl_)
    impl_->clear();
}

signal_base::size_type signal_base::size() const
{
  return (impl_ ? impl_->size() : 0);
}

signal_base::iterator_type signal_base::connect(const slot_base& slot_)
{
  return impl()->connect(slot_);
}

signal_base::iterator_type signal_base::insert(iterator_type i, const slot_base& slot_)
{
  return impl()->insert(i, slot_);
}

signal_base::iterator_type signal_base::erase(iterator_type i)
{
  return impl()->erase(i);
}

signal_base& signal_base::operator = (const signal_base& src)
{
  if (impl_) impl_->unreference();
  impl_ = src.impl();
  impl_->reference();
  return *this;
}

internal::signal_impl* signal_base::impl() const
{
  if (!impl_) {
    impl_ = new internal::signal_impl;
    impl_->reference();  // start with a reference count of 1
  }
  return impl_;
}

} /* sigc */
