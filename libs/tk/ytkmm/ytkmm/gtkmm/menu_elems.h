/* $Id$ */
#ifndef _GTKMM_MENU_ELEMS_H
#define _GTKMM_MENU_ELEMS_H
/* menu_elems.h
 *
 * Copyright (C) 1998-2002 The gtkmm Development Team
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

#include <gtkmm/container.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/imagemenuitem.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/tearoffmenuitem.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/accelkey.h>
#include <gdk/gdkkeysyms.h>


namespace Gtk
{

class Menu;

namespace Menu_Helpers
{

// input class (MenuItem-Factory)

class Element
{
public:
  typedef sigc::slot<void> CallSlot;

  Element();
  Element(MenuItem& child);
  ~Element();

  const Glib::RefPtr<MenuItem>& get_child() const;

protected:

  void set_child(MenuItem* pChild);
  void set_accel_key(const AccelKey& accel_key);

  //We use a RefPtr to avoid leaks when the manage()d widget never gets added to a container.
  //TODO: RefPtr is probably meant only for use with a create() method - see the extra reference() in set_child().
  Glib::RefPtr<MenuItem> child_;
};

/** Use this class and its subclasses to build menu items.
 * For example,
 * @code
 * m_Menu_File.items().push_back( Gtk::Menu_Helpers::MenuElem("_New",
 *   sigc::mem_fun(*this, &ExampleWindow::on_menu_file_new) ) );
 * @endcode
 *
 * @ingroup Menus
 */
class MenuElem : public Element
{
public:

  MenuElem(MenuItem& child);

  /** Create a labeled, non-accelerated MenuItem with a sigc::slot
   * @param label The menu item's name
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  MenuElem(const Glib::ustring& label, const CallSlot& slot = CallSlot());

  /** Create a labeled, accelerated MenuItem with a sigc::slot
   * @param label The menu item's name
   * @param key The accelerator key combination
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  MenuElem(const Glib::ustring& label, const AccelKey& key,
           const CallSlot& slot = CallSlot());

  /** Create a labeled, non-accelerated MenuItem with a submenu
   * @param label The menu item's name
   * @param submenu The sub menu
   */
  MenuElem(const Glib::ustring& label, Gtk::Menu& submenu);

  /** Create a labeled, accelerated MenuItem with a submenu
   * @param label The menu item's name
   * @param key The accelerator key combination
   * @param submenu The sub menu
   */
  MenuElem(const Glib::ustring& label,
           const AccelKey& key,
           Gtk::Menu& submenu);
};

class SeparatorElem : public Element
{
public:
  SeparatorElem();
};

class ImageMenuElem : public Element
{
public:
  ImageMenuElem(ImageMenuItem& child);

  /** Create a labeled, non-accelerated MenuItem with a sigc::slot
   * @param label The menu item's name
   * @param image_widget The image
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  ImageMenuElem(const Glib::ustring& label,
                Gtk::Widget& image_widget,
                const CallSlot& slot = CallSlot());

  /** Create a labeled, accelerated MenuItem with a sigc::slot
   * @param label The menu item's name
   * @param key The accelerator key combination
   * @param image_widget The image
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  ImageMenuElem(const Glib::ustring& label, const AccelKey& key,
                Gtk::Widget& image_widget,
                const CallSlot& slot = CallSlot());

  /** Create a labeled, non-accelerated MenuItem with a submenu
   * @param label The menu item's name
   * @param image_widget The image
   * @param submenu The sub menu
   */
  ImageMenuElem(const Glib::ustring& label,
                Gtk::Widget& image_widget,
                Gtk::Menu& submenu);

  /** Create a labeled, accelerated MenuItem with a submenu
   * @param label The menu item's name
   * @param key The accelerator key combination
   * @param image_widget The image
   * @param submenu The sub menu
   */
  ImageMenuElem(const Glib::ustring& label, const AccelKey& key,
                Gtk::Widget& image_widget,
                Gtk::Menu& submenu);
};

class StockMenuElem : public Element
{
public:
  /** Create a non-accelerated MenuItem from a stock item
   * @param stock_id The ID of the stock item
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  StockMenuElem(const Gtk::StockID& stock_id,
                const CallSlot& slot = CallSlot());

  /** Create an accelerated MenuItem from a stock item
   * @param stock_id The ID of the stock item
   * @param key The accelerator key combination
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  StockMenuElem(const Gtk::StockID& stock_id,
                const AccelKey& key,
                const CallSlot& slot = CallSlot());

  /** Create a non-accelerated MenuItem from a stock item with a submenu
   * @param stock_id The ID of the stock item
   * @param submenu The sub menu
   */
  StockMenuElem(const Gtk::StockID& stock_id,
                Gtk::Menu& submenu);

  /** Create an accelerated MenuItem from a stock item with a submenu
   * @param stock_id The ID of the stock item
   * @param key The accelerator key combination
   * @param submenu The sub menu
   */
  StockMenuElem(const Gtk::StockID& stock_id,
                const AccelKey& key,
                Gtk::Menu& submenu);
};

class CheckMenuElem : public Element
{
public:
  CheckMenuElem(CheckMenuItem& child);

  /** Create a labeled, non-accelerated MenuItem with a sigc::slot
   * @param label The menu item's name
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  CheckMenuElem(const Glib::ustring& label, const CallSlot& slot = CallSlot());

  /** Create a labeled, accelerated CheckMenuItem with a sigc::slot
   * @param label The menu item's name
   * @param key The accelerator key combination
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  CheckMenuElem(const Glib::ustring& label, const AccelKey& key,
                const CallSlot& slot = CallSlot());
};


class RadioMenuElem : public Element
{
public:
  RadioMenuElem(RadioMenuItem& child);

  /** Create a labeled, non-accelerated MenuItem with a sigc::slot
   * @param label The menu item's name
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  RadioMenuElem(RadioMenuItem::Group&, const Glib::ustring& label,
                const CallSlot& slot = CallSlot());

  /** Create a labeled, accelerated CheckMenuItem with a sigc::slot
   * @param group The RadioMenuItem group in which to put this.
   * @param label The menu item's name
   * @param key The accelerator key combination
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  RadioMenuElem(RadioMenuItem::Group& group, const Glib::ustring& label,
                const AccelKey& key,
                const CallSlot& slot = CallSlot());

protected:
  RadioMenuItem::Group* gr_;
};

class TearoffMenuElem : public Element
{
public:
  TearoffMenuElem(TearoffMenuItem& child);

  /** Create a non-accelerated TearoffMenuItem with a sigc::slot
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  TearoffMenuElem(const CallSlot& slot = CallSlot());

  /** Create accelerated TearoffMenuItem with a sigc::slot
   * @param key The accelerator key combination
   * @param slot Use sigc::mem_fun() to specify a signal handler
   */
  TearoffMenuElem(const AccelKey& key,
                  const CallSlot& slot = CallSlot());
};

} /* namespace Menu_Helpers */

} /* namespace Gtk */

#endif //_GTKMM_MENU_ELEMS_H
