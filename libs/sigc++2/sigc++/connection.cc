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

#include <sigc++/connection.h>
using namespace std;

namespace sigc {

connection::connection()
: slot_(0)
{}

connection::connection(const connection& c)
: slot_(c.slot_)
{
  //Let the connection forget about the signal handler when the handler object dies:
  if (slot_)
    slot_->add_destroy_notify_callback(this, &notify);
}

connection::connection(slot_base& sl)
: slot_(&sl)
{
  //Let the connection forget about the signal handler when the handler object dies:
  slot_->add_destroy_notify_callback(this, &notify);
}

connection& connection::operator=(const connection& c)
{
  set_slot(c.slot_);
  return *this;
}

connection::~connection()
{
  if (slot_)
    slot_->remove_destroy_notify_callback(this);
}

bool connection::empty() const
{
  return (!slot_ || slot_->empty());
}

bool connection::connected() const
{
  return !empty();
}

bool connection::blocked() const
{
  return (slot_ ? slot_->blocked() : false);
}

bool connection::block(bool should_block)
{
  return (slot_ ? slot_->block(should_block) : false);
}

bool connection::unblock()
{
  return (slot_ ? slot_->unblock() : false);
}

void connection::disconnect()
{
  if (slot_)
    slot_->disconnect(); // This notifies slot_'s parent.
} 

connection::operator bool()
{
  return !empty();
}
    
void connection::set_slot(slot_base* sl)
{
  if (slot_)
    slot_->remove_destroy_notify_callback(this);

  slot_ = sl;

  if (slot_)
    slot_->add_destroy_notify_callback(this, &notify);
}

void* connection::notify(void* data)
{
  connection* self = reinterpret_cast<connection*>(data);
  self->slot_ = 0;
  return 0;
}

} /* namespace sigc */
