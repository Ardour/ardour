// -*- c++ -*-
/* $Id$ */

/* Copyright (C) 2002 The gtkmm Development Team
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

#include <gtkmm/stock.h>
#include <gtkmm/stockitem.h>
#include <gtk/gtkstock.h>

// Get rid of macro DELETE (from winnt.h).  We have some macro
// shadowing magic in stock.h, but it's safer to get rid of it
// entirely in the .cc file.
#undef DELETE


namespace Gtk
{

namespace Stock
{

const Gtk::BuiltinStockID DIALOG_AUTHENTICATION = { GTK_STOCK_DIALOG_AUTHENTICATION };
const Gtk::BuiltinStockID DIALOG_INFO = { GTK_STOCK_DIALOG_INFO };
const Gtk::BuiltinStockID DIALOG_WARNING = { GTK_STOCK_DIALOG_WARNING };
const Gtk::BuiltinStockID DIALOG_ERROR = { GTK_STOCK_DIALOG_ERROR };
const Gtk::BuiltinStockID DIALOG_QUESTION  = { GTK_STOCK_DIALOG_QUESTION };
const Gtk::BuiltinStockID DND = { GTK_STOCK_DND };
const Gtk::BuiltinStockID DND_MULTIPLE = { GTK_STOCK_DND_MULTIPLE };
const Gtk::BuiltinStockID ABOUT = { GTK_STOCK_ABOUT };
const Gtk::BuiltinStockID ADD = { GTK_STOCK_ADD };
const Gtk::BuiltinStockID APPLY= { GTK_STOCK_APPLY };
const Gtk::BuiltinStockID BOLD= { GTK_STOCK_BOLD };
const Gtk::BuiltinStockID CANCEL = { GTK_STOCK_CANCEL };
const Gtk::BuiltinStockID CDROM = { GTK_STOCK_CDROM };
const Gtk::BuiltinStockID CLEAR = { GTK_STOCK_CLEAR };
const Gtk::BuiltinStockID CLOSE = { GTK_STOCK_CLOSE };
const Gtk::BuiltinStockID COLOR_PICKER = { GTK_STOCK_COLOR_PICKER };
const Gtk::BuiltinStockID CONVERT = { GTK_STOCK_CONVERT };
const Gtk::BuiltinStockID CONNECT = { GTK_STOCK_CONNECT };
const Gtk::BuiltinStockID COPY = { GTK_STOCK_COPY };
const Gtk::BuiltinStockID CUT = { GTK_STOCK_CUT };
const Gtk::BuiltinStockID DELETE = { GTK_STOCK_DELETE };
const Gtk::BuiltinStockID DIRECTORY = { GTK_STOCK_DIRECTORY };
const Gtk::BuiltinStockID DISCONNECT = { GTK_STOCK_DISCONNECT };
const Gtk::BuiltinStockID EDIT = { GTK_STOCK_EDIT };
const Gtk::BuiltinStockID EXECUTE = { GTK_STOCK_EXECUTE };
const Gtk::BuiltinStockID FILE = { GTK_STOCK_FILE };
const Gtk::BuiltinStockID FIND = { GTK_STOCK_FIND };
const Gtk::BuiltinStockID FIND_AND_REPLACE = { GTK_STOCK_FIND_AND_REPLACE };
const Gtk::BuiltinStockID FLOPPY = { GTK_STOCK_FLOPPY };
const Gtk::BuiltinStockID GOTO_BOTTOM = { GTK_STOCK_GOTO_BOTTOM };
const Gtk::BuiltinStockID GOTO_FIRST = { GTK_STOCK_GOTO_FIRST };
const Gtk::BuiltinStockID GOTO_LAST = { GTK_STOCK_GOTO_LAST };
const Gtk::BuiltinStockID GOTO_TOP = { GTK_STOCK_GOTO_TOP };
const Gtk::BuiltinStockID GO_BACK = { GTK_STOCK_GO_BACK };
const Gtk::BuiltinStockID GO_DOWN = { GTK_STOCK_GO_DOWN };
const Gtk::BuiltinStockID GO_FORWARD = { GTK_STOCK_GO_FORWARD };
const Gtk::BuiltinStockID GO_UP = { GTK_STOCK_GO_UP };
const Gtk::BuiltinStockID HARDDISK = { GTK_STOCK_HARDDISK };
const Gtk::BuiltinStockID HELP = { GTK_STOCK_HELP };
const Gtk::BuiltinStockID HOME = { GTK_STOCK_HOME };
const Gtk::BuiltinStockID INDEX = { GTK_STOCK_INDEX };
const Gtk::BuiltinStockID INDENT = { GTK_STOCK_INDENT };
const Gtk::BuiltinStockID UNINDENT = { GTK_STOCK_UNINDENT };
const Gtk::BuiltinStockID ITALIC = { GTK_STOCK_ITALIC };
const Gtk::BuiltinStockID JUMP_TO = { GTK_STOCK_JUMP_TO };
const Gtk::BuiltinStockID JUSTIFY_CENTER = { GTK_STOCK_JUSTIFY_CENTER };
const Gtk::BuiltinStockID JUSTIFY_FILL = { GTK_STOCK_JUSTIFY_FILL };
const Gtk::BuiltinStockID JUSTIFY_LEFT = { GTK_STOCK_JUSTIFY_LEFT };
const Gtk::BuiltinStockID JUSTIFY_RIGHT = { GTK_STOCK_JUSTIFY_RIGHT };
const Gtk::BuiltinStockID MISSING_IMAGE = { GTK_STOCK_MISSING_IMAGE };
const Gtk::BuiltinStockID MEDIA_FORWARD = { GTK_STOCK_MEDIA_FORWARD };
const Gtk::BuiltinStockID MEDIA_NEXT = { GTK_STOCK_MEDIA_NEXT };
const Gtk::BuiltinStockID MEDIA_PAUSE = { GTK_STOCK_MEDIA_PAUSE };
const Gtk::BuiltinStockID MEDIA_PLAY = { GTK_STOCK_MEDIA_PLAY };
const Gtk::BuiltinStockID MEDIA_PREVIOUS = { GTK_STOCK_MEDIA_PREVIOUS };
const Gtk::BuiltinStockID MEDIA_RECORD = { GTK_STOCK_MEDIA_RECORD };
const Gtk::BuiltinStockID MEDIA_REWIND = { GTK_STOCK_MEDIA_REWIND };
const Gtk::BuiltinStockID MEDIA_STOP = { GTK_STOCK_MEDIA_STOP };
const Gtk::BuiltinStockID NETWORK = { GTK_STOCK_NETWORK };
const Gtk::BuiltinStockID NEW = { GTK_STOCK_NEW };
const Gtk::BuiltinStockID NO = { GTK_STOCK_NO };
const Gtk::BuiltinStockID OK = { GTK_STOCK_OK };
const Gtk::BuiltinStockID OPEN = { GTK_STOCK_OPEN };
const Gtk::BuiltinStockID PASTE = { GTK_STOCK_PASTE };
const Gtk::BuiltinStockID PREFERENCES = { GTK_STOCK_PREFERENCES };
const Gtk::BuiltinStockID PRINT = { GTK_STOCK_PRINT };
const Gtk::BuiltinStockID PRINT_PREVIEW = { GTK_STOCK_PRINT_PREVIEW };
const Gtk::BuiltinStockID PROPERTIES = { GTK_STOCK_PROPERTIES };
const Gtk::BuiltinStockID QUIT = { GTK_STOCK_QUIT };
const Gtk::BuiltinStockID REDO = { GTK_STOCK_REDO };
const Gtk::BuiltinStockID REFRESH = { GTK_STOCK_REFRESH };
const Gtk::BuiltinStockID REMOVE = { GTK_STOCK_REMOVE };
const Gtk::BuiltinStockID REVERT_TO_SAVED = { GTK_STOCK_REVERT_TO_SAVED };
const Gtk::BuiltinStockID SAVE = { GTK_STOCK_SAVE };
const Gtk::BuiltinStockID SAVE_AS = { GTK_STOCK_SAVE_AS };
const Gtk::BuiltinStockID SELECT_COLOR = { GTK_STOCK_SELECT_COLOR };
const Gtk::BuiltinStockID SELECT_FONT = { GTK_STOCK_SELECT_FONT };
const Gtk::BuiltinStockID SORT_ASCENDING = { GTK_STOCK_SORT_ASCENDING };
const Gtk::BuiltinStockID SORT_DESCENDING = { GTK_STOCK_SORT_DESCENDING };
const Gtk::BuiltinStockID SPELL_CHECK = { GTK_STOCK_SPELL_CHECK };
const Gtk::BuiltinStockID STOP = { GTK_STOCK_STOP };
const Gtk::BuiltinStockID STRIKETHROUGH = { GTK_STOCK_STRIKETHROUGH };
const Gtk::BuiltinStockID UNDELETE = { GTK_STOCK_UNDELETE };
const Gtk::BuiltinStockID UNDERLINE = { GTK_STOCK_UNDERLINE };
const Gtk::BuiltinStockID UNDO = { GTK_STOCK_UNDO };
const Gtk::BuiltinStockID YES = { GTK_STOCK_YES };
const Gtk::BuiltinStockID ZOOM_100 = { GTK_STOCK_ZOOM_100 };
const Gtk::BuiltinStockID ZOOM_FIT = { GTK_STOCK_ZOOM_FIT };
const Gtk::BuiltinStockID ZOOM_IN = { GTK_STOCK_ZOOM_IN };
const Gtk::BuiltinStockID ZOOM_OUT = { GTK_STOCK_ZOOM_OUT };


void add(const Gtk::StockItem& item)
{
  gtk_stock_add(item.gobj(), 1);
}

bool lookup(const Gtk::StockID& stock_id, Gtk::StockItem& item)
{
  return Gtk::StockItem::lookup(stock_id, item);
}

bool lookup(const Gtk::StockID& stock_id, Gtk::IconSet& iconset)
{
  iconset = Gtk::IconSet::lookup_default(stock_id);
  return (iconset.gobj() != 0);
}

bool lookup(const Gtk::StockID& stock_id, Gtk::IconSize size, Gtk::Image& image)
{
  image.set(stock_id, size);
  return (image.gobj() != 0);
}

Glib::SListHandle<Gtk::StockID,Gtk::StockID_Traits> get_ids()
{
  return Glib::SListHandle<Gtk::StockID,Gtk::StockID_Traits>(
      gtk_stock_list_ids(), Glib::OWNERSHIP_DEEP);
}

} // namespace Stock

} // namespace Gtk

