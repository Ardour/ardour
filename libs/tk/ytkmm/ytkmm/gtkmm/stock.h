// -*- c++ -*-
#ifndef _GTKMM_STOCK_H
#define _GTKMM_STOCK_H

/* $Id$ */

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

#include <gtkmm/iconset.h>
#include <gtkmm/stockitem.h>
#include <gtkmm/stockid.h>
#include <gtkmm/image.h>

/* Shadow DELETE macro (from winnt.h).
 */
#if defined(DELETE) && !defined(GTKMM_MACRO_SHADOW_DELETE)
enum { GTKMM_MACRO_DEFINITION_DELETE = DELETE };
#undef DELETE
enum { DELETE = GTKMM_MACRO_DEFINITION_DELETE };
#define DELETE DELETE
#define GTKMM_MACRO_SHADOW_DELETE 1
#endif


namespace Gtk
{

// Created like so:
// const BuiltinStockID DIALOG_INFO = { GTK_STOCK_DIALOG_INFO }

/** See the list of pre-defined stock items, in the Stock namespace.
 */
struct BuiltinStockID
{
  /**
   * The text representation of the stock id, usually something like "gtk-about".
   */
  const char* id;
};

namespace Stock
{

extern GTKMM_API const Gtk::BuiltinStockID DIALOG_AUTHENTICATION;   /*!< @image html gtk-dialog-authentication.png     */

extern GTKMM_API const Gtk::BuiltinStockID DIALOG_INFO;      /*!< @image html gtk-dialog-info.png         */
extern GTKMM_API const Gtk::BuiltinStockID DIALOG_WARNING;   /*!< @image html gtk-dialog-warning.png      */
extern GTKMM_API const Gtk::BuiltinStockID DIALOG_ERROR;     /*!< @image html gtk-dialog-error.png        */
extern GTKMM_API const Gtk::BuiltinStockID DIALOG_QUESTION;  /*!< @image html gtk-dialog-question.png     */

// These aren't real stock items, because they provide only an icon.
extern GTKMM_API const Gtk::BuiltinStockID DND;              /*!< @image html gtk-dnd.png                 */
extern GTKMM_API const Gtk::BuiltinStockID DND_MULTIPLE;     /*!< @image html gtk-dnd-multiple.png        */

extern GTKMM_API const Gtk::BuiltinStockID ABOUT;            /*!< @image html gtk-about.png               */
extern GTKMM_API const Gtk::BuiltinStockID ADD;              /*!< @image html gtk-add.png                 */
extern GTKMM_API const Gtk::BuiltinStockID APPLY;            /*!< @image html gtk-apply.png               */
extern GTKMM_API const Gtk::BuiltinStockID BOLD;             /*!< @image html gtk-bold.png           */
extern GTKMM_API const Gtk::BuiltinStockID CANCEL;           /*!< @image html gtk-cancel.png              */
extern GTKMM_API const Gtk::BuiltinStockID CAPS_LOCK_WARNING; /*!< @image html gtk-caps-lock-warning.png  */
extern GTKMM_API const Gtk::BuiltinStockID CDROM;            /*!< @image html gtk-cdrom.png               */
extern GTKMM_API const Gtk::BuiltinStockID CLEAR;            /*!< @image html gtk-clear.png               */
extern GTKMM_API const Gtk::BuiltinStockID CLOSE;            /*!< @image html gtk-close.png               */
extern GTKMM_API const Gtk::BuiltinStockID COLOR_PICKER;     /*!< @image html gtk-color-picker.png        */
extern GTKMM_API const Gtk::BuiltinStockID CONVERT;          /*!< @image html gtk-convert.png             */
extern GTKMM_API const Gtk::BuiltinStockID CONNECT;          /*!< @image html gtk-connect.png             */
extern GTKMM_API const Gtk::BuiltinStockID COPY;             /*!< @image html gtk-copy.png                */
extern GTKMM_API const Gtk::BuiltinStockID CUT;              /*!< @image html gtk-cut.png                 */
extern GTKMM_API const Gtk::BuiltinStockID DELETE;           /*!< @image html gtk-delete.png               */
extern GTKMM_API const Gtk::BuiltinStockID DIRECTORY;        /*!< @image html gtk-directory.png           */
extern GTKMM_API const Gtk::BuiltinStockID DISCARD;          /*!< @image html gtk-discard.png             */
extern GTKMM_API const Gtk::BuiltinStockID DISCONNECT;       /*!< @image html gtk-disconnect.png          */
extern GTKMM_API const Gtk::BuiltinStockID EDIT;             /*!< @image html gtk-edit.png                */
extern GTKMM_API const Gtk::BuiltinStockID EXECUTE;          /*!< @image html gtk-execute.png                */
extern GTKMM_API const Gtk::BuiltinStockID FILE;             /*!< @image html gtk-file.png                */
extern GTKMM_API const Gtk::BuiltinStockID FIND;             /*!< @image html gtk-find.png              */
extern GTKMM_API const Gtk::BuiltinStockID FIND_AND_REPLACE; /*!< @image html gtk-find-and-replace.png      */
extern GTKMM_API const Gtk::BuiltinStockID FLOPPY;           /*!< @image html gtk-floppy.png                */
extern GTKMM_API const Gtk::BuiltinStockID FULLSCREEN;       /*!< @image html gtk-fullscreen.png                */
extern GTKMM_API const Gtk::BuiltinStockID LEAVE_FULLSCREEN; /*!< @image html gtk-leave-fullscreen.png                */
extern GTKMM_API const Gtk::BuiltinStockID GOTO_BOTTOM;      /*!< @image html gtk-goto-bottom.png              */
extern GTKMM_API const Gtk::BuiltinStockID GOTO_FIRST;       /*!< left-to-right languages: @image html gtk-goto-first-ltr.png
                                                                  right-to-left languages: @image html gtk-goto-first-rtl.png               */
extern GTKMM_API const Gtk::BuiltinStockID GOTO_LAST;        /*!< left-to-right languages: @image html gtk-goto-last-ltr.png
                                                                  right-to-left languages: @image html gtk-goto-last-rtl.png                */
extern GTKMM_API const Gtk::BuiltinStockID GOTO_TOP;         /*!< @image html gtk-goto-top.png                 */
extern GTKMM_API const Gtk::BuiltinStockID GO_BACK;          /*!< left-to-right languages: @image html gtk-go-back-ltr.png
                                                                  right-to-left languages: @image html gtk-go-back-rtl.png          */
extern GTKMM_API const Gtk::BuiltinStockID GO_DOWN;          /*!< @image html gtk-go-down.png          */
extern GTKMM_API const Gtk::BuiltinStockID GO_FORWARD;       /*!< left-to-right languages: @image html gtk-go-forward-ltr.png
                                                                  right-to-left languages: @image html gtk-go-forward-rtl.png         */
extern GTKMM_API const Gtk::BuiltinStockID GO_UP;            /*!< @image html gtk-go-up.png            */
extern GTKMM_API const Gtk::BuiltinStockID HARDDISK;         /*!< @image html gtk-harddisk.png            */
extern GTKMM_API const Gtk::BuiltinStockID HELP;             /*!< @image html gtk-help.png                */
extern GTKMM_API const Gtk::BuiltinStockID HOME;             /*!< @image html gtk-home.png                */
extern GTKMM_API const Gtk::BuiltinStockID INDEX;            /*!< @image html gtk-index.png               */
extern GTKMM_API const Gtk::BuiltinStockID INFO;             /*!< @image html gtk-info.png               */
extern GTKMM_API const Gtk::BuiltinStockID INDENT;           /*!< left-to-right languages: @image html gtk-indent-ltr.png
                                                                  right-to-left languages: @image html gtk-indent-rtl.png          */
extern GTKMM_API const Gtk::BuiltinStockID UNINDENT;         /*!< left-to-right languages: @image html gtk-unindent-ltr.png
                                                                  right-to-left languages: @image html gtk-unindent-rtl.png            */
extern GTKMM_API const Gtk::BuiltinStockID ITALIC;           /*!< @image html gtk-italic.png         */
extern GTKMM_API const Gtk::BuiltinStockID JUMP_TO;          /*!< left-to-right languages: @image html gtk-jump-to-ltr.png
                                                                  right-to-left languages: @image html gtk-jump-to-rtl.png             */
extern GTKMM_API const Gtk::BuiltinStockID JUSTIFY_CENTER;   /*!< @image html gtk-justify-center.png        */
extern GTKMM_API const Gtk::BuiltinStockID JUSTIFY_FILL;     /*!< @image html gtk-justify-fill.png       */
extern GTKMM_API const Gtk::BuiltinStockID JUSTIFY_LEFT;     /*!< @image html gtk-justify-left.png          */
extern GTKMM_API const Gtk::BuiltinStockID JUSTIFY_RIGHT;    /*!< @image html gtk-justify-right.png         */
extern GTKMM_API const Gtk::BuiltinStockID MISSING_IMAGE;    /*!< @image html gtk-missing-image.png        */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_FORWARD;    /*!< left-to-right languages: @image html gtk-media-forward-ltr.png
                                                                  right-to-left languages: @image html gtk-media-forward-rtl.png       */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_NEXT;       /*!< left-to-right languages: @image html gtk-media-next-ltr.png
                                                                  right-to-left languages: @image html gtk-media-next-rtl.png          */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_PAUSE;      /*!< @image html gtk-media-pause.png         */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_PLAY;       /*!< left-to-right languages: @image html gtk-media-play-ltr.png
                                                                  right-to-left languages: @image html gtk-media-play-rtl.png          */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_PREVIOUS;   /*!< left-to-right languages: @image html gtk-media-previous-ltr.png
                                                                  right-to-left languages: @image html gtk-media-previous-rtl.png      */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_RECORD;     /*!< @image html gtk-media-record.png        */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_REWIND;     /*!< left-to-right languages: @image html gtk-media-rewind-ltr.png
                                                                  right-to-left languages: @image html gtk-media-rewind-rtl.png        */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_STOP;       /*!< @image html gtk-media-stop.png          */
extern GTKMM_API const Gtk::BuiltinStockID NETWORK;          /*!< @image html gtk-network.png             */
extern GTKMM_API const Gtk::BuiltinStockID NEW;              /*!< @image html gtk-new.png                 */
extern GTKMM_API const Gtk::BuiltinStockID NO;               /*!< @image html gtk-no.png                  */
extern GTKMM_API const Gtk::BuiltinStockID OK;               /*!< @image html gtk-ok.png                  */
extern GTKMM_API const Gtk::BuiltinStockID OPEN;             /*!< @image html gtk-open.png                */
extern GTKMM_API const Gtk::BuiltinStockID ORIENTATION_PORTRAIT; /*!< @image html gtk-orientation-portrait.png                */
extern GTKMM_API const Gtk::BuiltinStockID ORIENTATION_LANDSCAPE; /*!< @image html gtk-orientation-landscape.png                */
extern GTKMM_API const Gtk::BuiltinStockID ORIENTATION_REVERSE_LANDSCAPE; /*!< @image html gtk-orientation-reverse-landscape.png                */
extern GTKMM_API const Gtk::BuiltinStockID ORIENTATION_REVERSE_PORTRAIT; /*!< @image html gtk-orientation-reverse-portrait.png                */
extern GTKMM_API const Gtk::BuiltinStockID PASTE;            /*!< @image html gtk-paste.png               */
extern GTKMM_API const Gtk::BuiltinStockID PREFERENCES;      /*!< @image html gtk-preferences.png         */
extern GTKMM_API const Gtk::BuiltinStockID PAGE_SETUP;       /*!< @image html gtk-page-setup.png          */
extern GTKMM_API const Gtk::BuiltinStockID PRINT;            /*!< @image html gtk-print.png               */
extern GTKMM_API const Gtk::BuiltinStockID PRINT_ERROR;      /*!< @image html gtk-print-error.png         */
extern GTKMM_API const Gtk::BuiltinStockID PRINT_PREVIEW;    /*!< @image html gtk-print-preview.png       */
extern GTKMM_API const Gtk::BuiltinStockID PRINT_REPORT;     /*!< @image html gtk-print-report.png        */
extern GTKMM_API const Gtk::BuiltinStockID PRINT_WARNING;    /*!< @image html gtk-print-warning.png       */
extern GTKMM_API const Gtk::BuiltinStockID PROPERTIES;       /*!< @image html gtk-properties.png          */
extern GTKMM_API const Gtk::BuiltinStockID QUIT;             /*!< @image html gtk-quit.png                */
extern GTKMM_API const Gtk::BuiltinStockID REDO;             /*!< left-to-right languages: @image html gtk-redo-ltr.png
                                                                  right-to-left languages: @image html gtk-redo-rtl.png                */
extern GTKMM_API const Gtk::BuiltinStockID REFRESH;          /*!< @image html gtk-refresh.png             */
extern GTKMM_API const Gtk::BuiltinStockID REMOVE;           /*!< @image html gtk-remove.png              */
extern GTKMM_API const Gtk::BuiltinStockID REVERT_TO_SAVED;  /*!< left-to-right languages: @image html gtk-revert-to-saved-ltr.png
                                                                  right-to-left languages: @image html gtk-revert-to-saved-rtl.png              */
extern GTKMM_API const Gtk::BuiltinStockID SAVE;             /*!< @image html gtk-save.png                */
extern GTKMM_API const Gtk::BuiltinStockID SAVE_AS;          /*!< @image html gtk-save-as.png             */
extern GTKMM_API const Gtk::BuiltinStockID SELECT_ALL;       /*!< @image html gtk-select-all.png           */
extern GTKMM_API const Gtk::BuiltinStockID SELECT_COLOR;     /*!< @image html gtk-select-color.png       */
extern GTKMM_API const Gtk::BuiltinStockID SELECT_FONT;      /*!< @image html gtk-select-font.png                */
extern GTKMM_API const Gtk::BuiltinStockID SORT_ASCENDING;   /*!< @image html gtk-sort-ascending.png      */
extern GTKMM_API const Gtk::BuiltinStockID SORT_DESCENDING;  /*!< @image html gtk-sort-descending.png     */
extern GTKMM_API const Gtk::BuiltinStockID SPELL_CHECK;      /*!< @image html gtk-spell-check.png          */
extern GTKMM_API const Gtk::BuiltinStockID STOP;             /*!< @image html gtk-stop.png                */
extern GTKMM_API const Gtk::BuiltinStockID STRIKETHROUGH;    /*!< @image html gtk-strikethrough.png  */
extern GTKMM_API const Gtk::BuiltinStockID UNDELETE;         /*!< left-to-right languages: @image html gtk-undelete-ltr.png
                                                                  right-to-left languages: @image html gtk-undelete-rtl.png            */
extern GTKMM_API const Gtk::BuiltinStockID UNDERLINE;        /*!< @image html gtk-underline.png      */
extern GTKMM_API const Gtk::BuiltinStockID UNDO;             /*!< left-to-right languages: @image html gtk-undo-ltr.png
                                                                  right-to-left languages: @image html gtk-undo-rtl.png                */
extern GTKMM_API const Gtk::BuiltinStockID YES;              /*!< @image html gtk-yes.png                 */
extern GTKMM_API const Gtk::BuiltinStockID ZOOM_100;         /*!< @image html gtk-zoom-100.png              */
extern GTKMM_API const Gtk::BuiltinStockID ZOOM_FIT;         /*!< @image html gtk-zoom-fit.png            */
extern GTKMM_API const Gtk::BuiltinStockID ZOOM_IN;          /*!< @image html gtk-zoom-in.png             */
extern GTKMM_API const Gtk::BuiltinStockID ZOOM_OUT;         /*!< @image html gtk-zoom-out.png            */

/** Add a stock item to the list of registered stock items.
 * @param item StockItem to register.
 *
 * If an item already exists with the same stock ID the old item gets replaced.
 */
void add(const Gtk::StockItem& item);

/** Fills item with the registered values for stock_id.
  * @param stock_id StockID to search for.
  * @param item item to fill in case stockid was found.
  *
  * @return <tt>true</tt> if the item was found - <tt>false</tt> otherwise.
  */
bool lookup(const Gtk::StockID& stock_id, Gtk::StockItem& item);

/** See IconSet::lookup_default()
  * @param stock_id StockID to search for.
  * @param iconset to fill.
  *
  * @return <tt>true</tt> if the item was found - <tt>false</tt> otherwise.
  */
bool lookup(const Gtk::StockID& stock_id, Gtk::IconSet& iconset);

/** Receive an Image of the registered stock id with the correct size.
  * @param stock_id StockID to search for.
  * @param size: IconSize of the desired Image.
  * @param image: Image to fill.
  *
  * @return <tt>true</tt> if the item was found - <tt>false</tt> otherwise
  */
bool lookup(const Gtk::StockID& stock_id, Gtk::IconSize size, Gtk::Image& image);

/** Retrieves a list of all known stock IDs added to an IconFactory or registered with Stock::add().
  *
  * @return list of all known stock IDs.
  */
Glib::SListHandle<Gtk::StockID,Gtk::StockID_Traits> get_ids();

} // namespace Stock

} // namespace Gtk


#endif /* _GTKMM_STOCK_H */
