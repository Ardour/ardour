// -*- c++ -*-
/* $Id$ */

/* 
 *
 * Copyright 1998-2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtk/gtkimagemenuitem.h>

#include <gtkmm/menu_elems.h>
#include <gtkmm/label.h>

#ifndef GLIBMM_WIN32
#include <strings.h>
#endif // GLIBMM_WIN32

namespace Gtk
{

namespace Menu_Helpers
{

Element::Element()
{
}

Element::Element(MenuItem& child)
{
  //TODO: Can't we avoid using RefPtr<> with a widget. It's not what it's meant for, and apparently it doesn't work well. murrayc.
  child_ = Glib::RefPtr<MenuItem>(&child);
  child_->reference(); //It's normally used with a create(), which starts with a refcount of 1.
}

Element::~Element()
{}

void Element::set_child(MenuItem* pChild)
{
  child_ = Glib::RefPtr<MenuItem>(pChild);
  child_->reference(); //TODO. We used to use the old RefPtr::operator=(), and this is what it did.
}

void Element::set_accel_key(const AccelKey& accel_key)
{
  if(child_)
    child_->set_accel_key(accel_key);
}

const Glib::RefPtr<MenuItem>& Element::get_child() const
{
  return child_;
}

MenuElem::MenuElem(MenuItem& child)
: Element(child)
{}

MenuElem::MenuElem(const Glib::ustring& label, 
                   const CallSlot& slot)
{
  set_child( manage(new MenuItem(label, true)) );
  if(slot)
    child_->signal_activate().connect(slot);
  child_->show();
}

MenuElem::MenuElem(const Glib::ustring& label,
                   const AccelKey& accel_key,
                   const CallSlot& slot)
{
  set_child( manage(new MenuItem(label, true)) );
  if(slot)
    child_->signal_activate().connect(slot);
  set_accel_key(accel_key);
  child_->show();
}

MenuElem::MenuElem(const Glib::ustring& label, Menu& submenu)
{
  set_child( manage(new MenuItem(label, true)) );
  child_->set_submenu(submenu);
  child_->show();
}

MenuElem::MenuElem(const Glib::ustring& label, 
                   const AccelKey& accel_key,
                   Gtk::Menu& submenu)
{
  set_child( manage(new MenuItem(label, true)) );
  child_->set_submenu(submenu);
  set_accel_key(accel_key);
  child_->show();
}

SeparatorElem::SeparatorElem()
{
  set_child( manage(new SeparatorMenuItem()) );
  child_->show();
}

ImageMenuElem::ImageMenuElem(ImageMenuItem& child)
: Element(child)
{}

ImageMenuElem::ImageMenuElem(const Glib::ustring& label,
                             Gtk::Widget& image_widget,
                             const CallSlot& slot)
{
  image_widget.show(); //We assume that the coder wants to actually show the widget.
  set_child( manage(new ImageMenuItem(image_widget, label, true)) );
  if(slot)
    child_->signal_activate().connect(slot);
  child_->show();
}

ImageMenuElem::ImageMenuElem(const Glib::ustring& label,
                             const AccelKey& accel_key,
                             Gtk::Widget& image_widget,
                             const CallSlot& slot)
{
  image_widget.show(); //We assume that the coder wants to actually show the widget.
  set_child( manage(new ImageMenuItem(image_widget, label, true)) );
  if(slot)
    child_->signal_activate().connect(slot);
  set_accel_key(accel_key);
  child_->show();
}

ImageMenuElem::ImageMenuElem(const Glib::ustring& label,
                             Gtk::Widget& image_widget,
                             Gtk::Menu& submenu)
{
  image_widget.show(); //We assume that the coder wants to actually show the widget.
  set_child( manage(new ImageMenuItem(image_widget, label, true)) );
  child_->set_submenu(submenu);
  child_->show();
}

ImageMenuElem::ImageMenuElem(const Glib::ustring& label,
                             const AccelKey& accel_key,
                             Gtk::Widget& image_widget,
                             Gtk::Menu& submenu)
{
  image_widget.show(); //We assume that the coder wants to actually show the widget.
  set_child( manage(new ImageMenuItem(image_widget, label, true)) );
  set_accel_key(accel_key);
  child_->set_submenu(submenu);
  child_->show();
}

StockMenuElem::StockMenuElem(const Gtk::StockID& stock_id,
                             const CallSlot& slot)
{
  set_child( manage(new ImageMenuItem(stock_id)) );
  if(slot)
    child_->signal_activate().connect(slot);
  child_->show();
}

StockMenuElem::StockMenuElem(const Gtk::StockID& stock_id,
                             const AccelKey& accel_key,
                             const CallSlot& slot)
{
  set_child( manage(new ImageMenuItem(stock_id)) );
  if(slot)
    child_->signal_activate().connect(slot);
  set_accel_key(accel_key);
  child_->show();
}

StockMenuElem::StockMenuElem(const Gtk::StockID& stock_id,
                             Gtk::Menu& submenu)
{
  set_child( manage(new ImageMenuItem(stock_id)) );
  child_->set_submenu(submenu);
  child_->show();
}

StockMenuElem::StockMenuElem(const Gtk::StockID& stock_id,
                             const AccelKey& accel_key,
                             Gtk::Menu& submenu)
{
  set_child( manage(new ImageMenuItem(stock_id)) );
  set_accel_key(accel_key);
  child_->set_submenu(submenu);
  child_->show();
}

CheckMenuElem::CheckMenuElem(CheckMenuItem& child)
: Element(child)
{}

CheckMenuElem::CheckMenuElem(const Glib::ustring& label,
                             const CallSlot& slot)
{
  CheckMenuItem* item = manage(new CheckMenuItem(label, true));
  set_child( item );
  if(slot)
    item->signal_toggled().connect(slot);
  child_->show();
}

CheckMenuElem::CheckMenuElem(const Glib::ustring& label,
                             const AccelKey& accel_key,
                             const CallSlot& slot)
{
  CheckMenuItem* item = manage(new CheckMenuItem(label, true));
  set_child( item );
  set_accel_key(accel_key);
  if(slot)
    item->signal_toggled().connect(slot);
  child_->show();
}


RadioMenuElem::RadioMenuElem(RadioMenuItem& child)
: Element(child), gr_(0)
{}

RadioMenuElem::RadioMenuElem(RadioMenuItem::Group& group,
                             const Glib::ustring& label,
                             const CallSlot& slot)
  : gr_(&group)
{
  CheckMenuItem* item = manage(new RadioMenuItem(*gr_, label, true));
  set_child( item );
  if(slot)
    item->signal_toggled().connect(slot);
  child_->show();
}

RadioMenuElem::RadioMenuElem(RadioMenuItem::Group& gr,
                             const Glib::ustring& label,
                             const AccelKey& accel_key,
                             const CallSlot& slot)
  : gr_(&gr)
{
  CheckMenuItem* item = manage(new RadioMenuItem(*gr_, label, true));
  set_child( item );
  set_accel_key(accel_key);
  if(slot)
    item->signal_toggled().connect(slot);
  child_->show();
}

TearoffMenuElem::TearoffMenuElem(TearoffMenuItem& child)
: Element(child)
{}

TearoffMenuElem::TearoffMenuElem(const CallSlot& slot)
{
  set_child( manage(new TearoffMenuItem()) );
  if(slot)
    child_->signal_activate().connect(slot);
  child_->show();
}

TearoffMenuElem::TearoffMenuElem(const AccelKey& accel_key,
                                 const CallSlot& slot)
{
  set_child( manage(new TearoffMenuItem()) );
  set_accel_key(accel_key);
  if(slot)
    child_->signal_activate().connect(slot);
  child_->show();
}

} /* namespace Menu_Helpers */

} /* namespace Gtk */

