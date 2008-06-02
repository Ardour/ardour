// -*- c++ -*-
/*
 * Copyright 2002, The libsigc++ Development Team
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

#ifndef _SIGC_SIGNAL_BASE_H_
#define _SIGC_SIGNAL_BASE_H_

#include <list>
#include <sigc++config.h>
#include <sigc++/type_traits.h>
#include <sigc++/trackable.h>
#include <sigc++/functors/slot.h>
#include <sigc++/functors/mem_fun.h>

namespace sigc
{

namespace internal
{

/** Implementation of the signal interface.
 * signal_impl manages a list of slots. When a slot becomes
 * invalid (because some referred object dies), notify() is executed.
 * notify() either calls sweep() directly or defers the execution of
 * sweep() when the signal is being emitted. sweep() removes all
 * invalid slot from the list.
 */
struct SIGC_API signal_impl
{
  typedef size_t size_type;
  typedef std::list<slot_base> slot_list;
  typedef slot_list::iterator       iterator_type;
  typedef slot_list::const_iterator const_iterator_type;

  signal_impl();

  // only MSVC needs this to guarantee that all new/delete are executed from the DLL module
#ifdef SIGC_NEW_DELETE_IN_LIBRARY_ONLY
  void* operator new(size_t size_);
  void operator delete(void* p);
#endif

  /// Increments the reference counter.
  inline void reference()
    { ++ref_count_; }

  /// Increments the reference and execution counter.
  inline void reference_exec()
    { ++ref_count_; ++exec_count_; }

  /** Decrements the reference counter.
   * The object is deleted when the reference counter reaches zero.
   */
  inline void unreference()
    { if (!(--ref_count_)) delete this; }

  /** Decrements the reference and execution counter.
   * Invokes sweep() if the execution counter reaches zero and the
   * removal of one or more slots has been deferred.
   */
  inline void unreference_exec()
    {
      if (!(--ref_count_)) delete this;
      else if (!(--exec_count_) && deferred_) sweep();
    }

  /** Returns whether the list of slots is empty.
   * @return @p true if the list of slots is empty.
   */
  inline bool empty() const
    { return slots_.empty(); }

  /// Empties the list of slots.
  void clear();

  /** Returns the number of slots in the list.
   * @return The number of slots in the list.
   */
  size_type size() const;

  /** Adds a slot at the bottom of the list of slots.
   * @param slot_ The slot to add to the list of slots.
   * @return An iterator pointing to the new slot in the list.
   */
  iterator_type connect(const slot_base& slot_);

  /** Adds a slot at the given position into the list of slots.
   * @param i An iterator indicating the position where @p slot_ should be inserted.
   * @param slot_ The slot to add to the list of slots.
   * @return An iterator pointing to the new slot in the list.
   */
  iterator_type insert(iterator_type i, const slot_base& slot_);

  /** Removes the slot at the given position from the list of slots.
   * @param i An iterator pointing to the slot to be removed.
   * @return An iterator pointing to the slot in the list after the one removed.
   */
  iterator_type erase(iterator_type i);

  /// Removes invalid slots from the list of slots.
  void sweep();

  /** Callback that is executed when some slot becomes invalid.
   * This callback is registered in every slot when inserted into
   * the list of slots. It is executed when a slot becomes invalid
   * because of some referred object being destroyed.
   * It either calls sweep() directly or defers the execution of
   * sweep() when the signal is being emitted.
   * @param d The signal object (@p this).
   */
  static void* notify(void* d);

  /** Reference counter.
   * The object is destroyed when @em ref_count_ reaches zero.
   */
  short ref_count_;

  /** Execution counter.
   * Indicates whether the signal is being emitted.
   */
  short exec_count_;

  /// Indicates whether the execution of sweep() is being deferred.
  bool deferred_;

  /// The list of slots.
  std::list<slot_base> slots_;
};

/// Exception safe sweeper for cleaning up invalid slots on the slot list.
struct SIGC_API signal_exec
{
  /// The parent sigc::signal_impl object.
  signal_impl* sig_;

  /** Increments the reference and execution counter of the parent sigc::signal_impl object.
   * @param sig The parent sigc::signal_impl object.
   */
  inline signal_exec(const signal_impl* sig)
    : sig_(const_cast<signal_impl*>(sig) )
    { sig_->reference_exec(); }

  /// Decrements the reference and execution counter of the parent sigc::signal_impl object.
  inline ~signal_exec()
    { sig_->unreference_exec(); }
};

/** Temporary slot list used during signal emission.
 *  Through evolution this class is slightly misnamed.  It is now
 *  an index into the slot_list passed into it.  It simply keeps track
 *  of where the end of this list was at construction, and pretends that's
 *  the end of your list.  This way you may connect during emittion without
 *  inadvertently entering an infinite loop, as well as make other
 *  modifications to the slot_list at your own risk.
 */
struct temp_slot_list
{
  typedef signal_impl::slot_list slot_list;
  typedef signal_impl::iterator_type iterator;
  typedef signal_impl::const_iterator_type const_iterator;

  temp_slot_list(slot_list &slots) : slots_(slots)
  {
    placeholder = slots_.insert(slots_.end(), slot_base());
  }

  ~temp_slot_list()
  {
    slots_.erase(placeholder);
  }

  iterator begin() { return slots_.begin(); }
  iterator end() { return placeholder; }
  const_iterator begin() const { return slots_.begin(); }
  const_iterator end() const { return placeholder; }

private:
  slot_list &slots_;
  slot_list::iterator placeholder;
};
  
} /* namespace internal */


/** @defgroup signal Signals
 * Use sigc::signal::connect() with sigc::mem_fun() and sigc::ptr_fun() to connect a method or function with a signal.
 *
 * @code
 * signal_clicked.connect( sigc::mem_fun(*this, &MyWindow::on_clicked) );
 * @endcode
 *
 * When the signal is emitted your method will be called.
 *
 * signal::connect() returns a connection, which you can later use to disconnect your method.
 * If the type of your object inherits from sigc::trackable the method is disconnected
 * automatically when your object is destroyed.
 *
 * When signals are copied they share the underlying information,
 * so you can have a protected/private sigc::signal member and a public accessor method.
 *
 * signal and slot objects provide the core functionality of this
 * library. A slot is a container for an arbitrary functor.
 * A signal is a list of slots that are executed on emission.
 * For compile time type safety a list of template arguments
 * must be provided for the signal template that determines the
 * parameter list for emission. Functors and closures are converted
 * into slots implicitely on connection, triggering compiler errors
 * if the given functor or closure cannot be invoked with the
 * parameter list of the signal to connect to.
 */

/** Base class for the sigc::signal# templates.
 * signal_base integrates most of the interface of the derived sigc::signal#
 * templates. The implementation, however, resides in sigc::internal::signal_impl.
 * A sigc::internal::signal_impl object is dynamically allocated from signal_base
 * when first connecting a slot to the signal. This ensures that empty signals
 * don't waste memory.
 *
 * @ingroup signal
 */
struct SIGC_API signal_base : public trackable
{
  typedef size_t size_type;

  signal_base();

  signal_base(const signal_base& src);

  ~signal_base();

  signal_base& operator = (const signal_base& src);

  /** Returns whether the list of slots is empty.
   * @return @p true if the list of slots is empty.
   */
  inline bool empty() const
    { return (!impl_ || impl_->empty()); }

  /// Empties the list of slots.
  void clear();

  /** Returns the number of slots in the list.
   * @return The number of slots in the list.
   */
  size_type size() const;

protected:
  typedef internal::signal_impl::iterator_type iterator_type;

  /** Adds a slot at the end of the list of slots.
   * With connect(), slots can also be added during signal emission.
   * In this case, they won't be executed until the next emission occurs.
   * @param slot_ The slot to add to the list of slots.
   * @return An iterator pointing to the new slot in the list.
   */
  iterator_type connect(const slot_base& slot_);

  /** Adds a slot at the given position into the list of slots.
   * Note that this function does not work during signal emission!
   * @param i An iterator indicating the position where @e slot_ should be inserted.
   * @param slot_ The slot to add to the list of slots.
   * @return An iterator pointing to the new slot in the list.
   */
  iterator_type insert(iterator_type i, const slot_base& slot_);

  /** Removes the slot at the given position from the list of slots.
   * Note that this function does not work during signal emission!
   * @param i An iterator pointing to the slot to be removed.
   * @return An iterator pointing to the slot in the list after the one removed.
   */
  iterator_type erase(iterator_type i);

  /** Returns the signal_impl object encapsulating the list of slots.
   * @return The signal_impl object encapsulating the list of slots.
   */
  internal::signal_impl* impl() const;

  /// The signal_impl object encapsulating the slot list.
  mutable internal::signal_impl* impl_;
};

} //namespace sigc

#endif /* _SIGC_SIGNAL_BASE_H_ */
