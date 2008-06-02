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
#ifndef _SIGC_CONNECTION_HPP_
#define _SIGC_CONNECTION_HPP_
#include <sigc++config.h>
#include <sigc++/signal.h>

namespace sigc {

/** Convinience class for safe disconnection.
 * Iterators must not be used beyond the lifetime of the list
 * they work on. A connection object can be created from a
 * slot list iterator and may safely be used to disconnect
 * the referred slot at any time (disconnect()). If the slot
 * has already been destroyed, disconnect() does nothing. empty() or
 * operator bool() can be used to test whether the connection is
 * still active. The connection can be blocked (block(), unblock()).
 *
 * This is possible because the connection object gets notified
 * when the referred slot dies (notify()).
 *
 * @ingroup signal
 */
struct SIGC_API connection
{
  /** Constructs an empty connection object. */
  connection();

  /** Constructs a connection object copying an existing one.
   * @param c The connection object to make a copy from.
   */
  connection(const connection& c);

  /** Constructs a connection object from a slot list iterator.
   * @param it The slot list iterator to take the slot from.
   */
  template <typename T_slot>
  connection(const slot_iterator<T_slot>& it) : slot_(&(*it))
    { if (slot_) slot_->add_destroy_notify_callback(this, &notify); }

  /** Constructs a connection object from a slot object.
   * This is only useful if you create your own slot list.
   * @param sl The slot to operate on.
   */
  explicit connection(slot_base& sl);

  /** Overrides this connection object copying another one.
   * @param c The connection object to make a copy from.
   */
  connection& operator=(const connection& c);

  /** Overrides this connection object with another slot list iterator.
   * @param it The new slot list iterator to take the slot from.
   */
  template <typename T_slot>
  connection& operator=(const slot_iterator<T_slot>& it)
    { set_slot(&(*it)); return *this; }

  ~connection();

  /** Returns whether the connection is still active.
   * @return @p false if the connection is still active.
   */
  bool empty() const;

  /** Returns whether the connection is still active.
   * @return @p true if the connection is still active.
   */
  bool connected() const;

  /** Returns whether the connection is blocked.
   * @return @p true if the connection is blocked.
   */
  bool blocked() const;

  /** Sets or unsets the blocking state of this connection.
   * See slot_base::block() for details.
   * @param should_block Indicates whether the blocking state should be set or unset.
   * @return @p true if the connection has been in blocking state before.
   */
  bool block(bool should_block = true);

  /** Unsets the blocking state of this connection.
   * @return @p true if the connection has been in blocking state before.
   */
  bool unblock();

  /// Disconnects the referred slot.
  void disconnect();

  /** Returns whether the connection is still active.
   * @return @p true if the connection is still active.
   */
  operator bool();

  /** Callback that is executed when the referred slot is destroyed.
   * @param d The connection object notified (@p this).
   */
  static void* notify(void* data);

private:
  void set_slot(slot_base* sl);

  /* Referred slot. Set to zero from notify().
   * A value of zero indicates an "empty" connection.
   */
  slot_base* slot_;
};

} /* namespace sigc */


#endif /* _SIGC_TRACKABLE_HPP_ */
