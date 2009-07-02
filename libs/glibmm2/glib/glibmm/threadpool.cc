// -*- c++ -*-
/* $Id: threadpool.cc 779 2009-01-19 17:58:50Z murrayc $ */

/* Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/threadpool.h>
#include <glibmm/exceptionhandler.h>
#include <glib.h>
#include <list>
#include <glibmmconfig.h>

GLIBMM_USING_STD(list)


namespace Glib
{

// internal
class ThreadPool::SlotList
{
public:
  SlotList();
  ~SlotList();

  sigc::slot<void>* push(const sigc::slot<void>& slot);
  sigc::slot<void>  pop(sigc::slot<void>* slot_ptr);

  void lock_and_unlock();

private:
  Glib::Mutex                     mutex_;
  std::list< sigc::slot<void> >  list_;

  // noncopyable
  SlotList(const ThreadPool::SlotList&);
  ThreadPool::SlotList& operator=(const ThreadPool::SlotList&);
};

ThreadPool::SlotList::SlotList()
{}

ThreadPool::SlotList::~SlotList()
{}

sigc::slot<void>* ThreadPool::SlotList::push(const sigc::slot<void>& slot)
{
  Mutex::Lock lock (mutex_);

  list_.push_back(slot);
  return &list_.back();
}

sigc::slot<void> ThreadPool::SlotList::pop(sigc::slot<void>* slot_ptr)
{
  sigc::slot<void> slot;

  {
    Mutex::Lock lock (mutex_);

    std::list< sigc::slot<void> >::iterator pslot = list_.begin();
    while(pslot != list_.end() && slot_ptr != &*pslot)
      ++pslot;

    if(pslot != list_.end())
    {
      slot = *pslot;
      list_.erase(pslot);
    }
  }

  return slot;
}

void ThreadPool::SlotList::lock_and_unlock()
{
  mutex_.lock();
  mutex_.unlock();
}

} // namespace Glib


namespace
{

static void call_thread_entry_slot(void* data, void* user_data)
{
  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  try
  {
  #endif //GLIBMM_EXCEPTIONS_ENABLED
    Glib::ThreadPool::SlotList *const slot_list =
        static_cast<Glib::ThreadPool::SlotList*>(user_data);

    sigc::slot<void> slot (slot_list->pop(static_cast<sigc::slot<void>*>(data)));

    slot();
  #ifdef GLIBMM_EXCEPTIONS_ENABLED
  }
  catch(Glib::Thread::Exit&)
  {
    // Just exit from the thread.  The Thread::Exit exception
    // is our sane C++ replacement of g_thread_exit().
  }
  catch(...)
  {
    Glib::exception_handlers_invoke();
  }
  #endif //GLIBMM_EXCEPTIONS_ENABLED
}

} // anonymous namespace


namespace Glib
{

ThreadPool::ThreadPool(int max_threads, bool exclusive)
:
  gobject_   (0),
  slot_list_ (new SlotList())
{
  GError* error = 0;

  gobject_ = g_thread_pool_new(
      &call_thread_entry_slot, slot_list_, max_threads, exclusive, &error);

  if(error)
  {
    delete slot_list_;
    slot_list_ = 0;
    Glib::Error::throw_exception(error);
  }
}

ThreadPool::~ThreadPool()
{
  if(gobject_)
    g_thread_pool_free(gobject_, 1, 1);

  if(slot_list_)
  {
    slot_list_->lock_and_unlock();
    delete slot_list_;
  }
}

void ThreadPool::push(const sigc::slot<void>& slot)
{
  sigc::slot<void> *const slot_ptr = slot_list_->push(slot);

  GError* error = 0;
  g_thread_pool_push(gobject_, slot_ptr, &error);

  if(error)
  {
    slot_list_->pop(slot_ptr);
    Glib::Error::throw_exception(error);
  }
}

void ThreadPool::set_max_threads(int max_threads)
{
  GError* error = 0;
  g_thread_pool_set_max_threads(gobject_, max_threads, &error);

  if(error)
    Glib::Error::throw_exception(error);
}

int ThreadPool::get_max_threads() const
{
  return g_thread_pool_get_max_threads(gobject_);
}

unsigned int ThreadPool::get_num_threads() const
{
  return g_thread_pool_get_num_threads(gobject_);
}

unsigned int ThreadPool::unprocessed() const
{
  return g_thread_pool_unprocessed(gobject_);
}

bool ThreadPool::get_exclusive() const
{
  g_return_val_if_fail(gobject_ != 0, false);

  return gobject_->exclusive;
}

void ThreadPool::shutdown(bool immediately)
{
  if(gobject_)
  {
    g_thread_pool_free(gobject_, immediately, 1);
    gobject_ = 0;
  }

  if(slot_list_)
  {
    slot_list_->lock_and_unlock();
    delete slot_list_;
    slot_list_ = 0;
  }
}

// static
void ThreadPool::set_max_unused_threads(int max_threads)
{
  g_thread_pool_set_max_unused_threads(max_threads);
}

// static
int ThreadPool::get_max_unused_threads()
{
  return g_thread_pool_get_max_unused_threads();
}

// static
unsigned int ThreadPool::get_num_unused_threads()
{
  return g_thread_pool_get_num_unused_threads();
}

// static
void ThreadPool::stop_unused_threads()
{
  g_thread_pool_stop_unused_threads();
}

} // namespace Glib

