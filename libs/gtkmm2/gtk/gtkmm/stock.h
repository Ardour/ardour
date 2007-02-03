// -*- c++ -*-
#ifndef _GTKMM_STOCK_H
#define _GTKMM_STOCK_H

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

#include <gtkmm/stockitem.h>
#include <gtkmm/stockid.h>
#include <gtkmm/iconset.h>
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
  const char* id;
};

namespace Stock
{

extern GTKMM_API const Gtk::BuiltinStockID DIALOG_AUTHENTICATION;   /*!< @image html stock_dialog_authentication_48.png     */

extern GTKMM_API const Gtk::BuiltinStockID DIALOG_INFO;      /*!< @image html stock_dialog_info_48.png         */
extern GTKMM_API const Gtk::BuiltinStockID DIALOG_WARNING;   /*!< @image html stock_dialog_warning_48.png      */
extern GTKMM_API const Gtk::BuiltinStockID DIALOG_ERROR;     /*!< @image html stock_dialog_error_48.png        */
extern GTKMM_API const Gtk::BuiltinStockID DIALOG_QUESTION;  /*!< @image html stock_dialog_question_48.png     */

// These aren't real stock items, because they provide only an icon.
extern GTKMM_API const Gtk::BuiltinStockID DND;              /*!< @image html stock_dnd_32.png                 */
extern GTKMM_API const Gtk::BuiltinStockID DND_MULTIPLE;     /*!< @image html stock_dnd_multiple_32.png        */

extern GTKMM_API const Gtk::BuiltinStockID ABOUT;            /*!< @image html stock_about_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID ADD;              /*!< @image html stock_add_24.png                 */
extern GTKMM_API const Gtk::BuiltinStockID APPLY;            /*!< @image html stock_apply_20.png               */
extern GTKMM_API const Gtk::BuiltinStockID BOLD;             /*!< @image html stock_text_bold_24.png           */
extern GTKMM_API const Gtk::BuiltinStockID CANCEL;           /*!< @image html stock_cancel_20.png              */
extern GTKMM_API const Gtk::BuiltinStockID CDROM;            /*!< @image html stock_cdrom_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID CLEAR;            /*!< @image html stock_clear_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID CLOSE;            /*!< @image html stock_close_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID COLOR_PICKER;     /*!< @image html stock_color_picker_24.png        */
extern GTKMM_API const Gtk::BuiltinStockID CONVERT;          /*!< @image html stock_convert_24.png             */
extern GTKMM_API const Gtk::BuiltinStockID CONNECT;          /*!< @image html stock_connect_24.png             */
extern GTKMM_API const Gtk::BuiltinStockID COPY;             /*!< @image html stock_copy_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID CUT;              /*!< @image html stock_cut_24.png                 */
extern GTKMM_API const Gtk::BuiltinStockID DELETE;           /*!< @image html stock_trash_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID DIRECTORY;        /*!< @image html stock_directory_24.png           */
extern GTKMM_API const Gtk::BuiltinStockID DISCONNECT;       /*!< @image html stock_disconnect_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID EDIT;             /*!< @image html stock_edit_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID EXECUTE;          /*!< @image html stock_exec_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID FILE;             /*!< @image html stock_file_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID FIND;             /*!< @image html stock_search_24.png              */
extern GTKMM_API const Gtk::BuiltinStockID FIND_AND_REPLACE; /*!< @image html stock_search_replace_24.png      */
extern GTKMM_API const Gtk::BuiltinStockID FLOPPY;           /*!< @image html stock_save_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID FULLSCREEN;       /*!< @image html stock_fullscreen_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID LEAVE_FULLSCREEN; /*!< @image html stock_leave_fullscreen_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID GOTO_BOTTOM;      /*!< @image html stock_bottom_24.png              */
extern GTKMM_API const Gtk::BuiltinStockID GOTO_FIRST;       /*!< @image html stock_first_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID GOTO_LAST;        /*!< @image html stock_last_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID GOTO_TOP;         /*!< @image html stock_top_24.png                 */
extern GTKMM_API const Gtk::BuiltinStockID GO_BACK;          /*!< @image html stock_left_arrow_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID GO_DOWN;          /*!< @image html stock_down_arrow_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID GO_FORWARD;       /*!< @image html stock_right_arrow_24.png         */
extern GTKMM_API const Gtk::BuiltinStockID GO_UP;            /*!< @image html stock_up_arrow_24.png            */
extern GTKMM_API const Gtk::BuiltinStockID HARDDISK;         /*!< @image html stock_harddisk_24.png            */
extern GTKMM_API const Gtk::BuiltinStockID HELP;             /*!< @image html stock_help_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID HOME;             /*!< @image html stock_home_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID INDEX;            /*!< @image html stock_index_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID INFO;            /*!< @image html stock_info_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID INDENT;           /*!< @image html stock_indent_24.png              */
extern GTKMM_API const Gtk::BuiltinStockID UNINDENT;         /*!< @image html stock_unindent_24.png            */
extern GTKMM_API const Gtk::BuiltinStockID ITALIC;           /*!< @image html stock_text_italic_24.png         */
extern GTKMM_API const Gtk::BuiltinStockID JUMP_TO;          /*!< @image html stock_jump_to_24.png             */
extern GTKMM_API const Gtk::BuiltinStockID JUSTIFY_CENTER;   /*!< @image html stock_align_center_24.png        */
extern GTKMM_API const Gtk::BuiltinStockID JUSTIFY_FILL;     /*!< @image html stock_align_justify_24.png       */
extern GTKMM_API const Gtk::BuiltinStockID JUSTIFY_LEFT;     /*!< @image html stock_align_left_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID JUSTIFY_RIGHT;    /*!< @image html stock_align_right_24.png         */
extern GTKMM_API const Gtk::BuiltinStockID MISSING_IMAGE;    /*!< @image html stock_broken_image_24.png        */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_FORWARD;    /*!< @image html stock_media_forward_24.png       */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_NEXT;       /*!< @image html stock_media_next_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_PAUSE;      /*!< @image html stock_media_pause_24.png         */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_PLAY;       /*!< @image html stock_media_play_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_PREVIOUS;   /*!< @image html stock_media_previous_24.png      */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_RECORD;     /*!< @image html stock_media_record_24.png        */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_REWIND;     /*!< @image html stock_media_rewind_24.png        */
extern GTKMM_API const Gtk::BuiltinStockID MEDIA_STOP;       /*!< @image html stock_media_stop_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID NETWORK;          /*!< @image html stock_network_24.png             */
extern GTKMM_API const Gtk::BuiltinStockID NEW;              /*!< @image html stock_new_24.png                 */
extern GTKMM_API const Gtk::BuiltinStockID NO;               /*!< @image html stock_no_20.png                  */
extern GTKMM_API const Gtk::BuiltinStockID OK;               /*!< @image html stock_ok_20.png                  */
extern GTKMM_API const Gtk::BuiltinStockID OPEN;             /*!< @image html stock_open_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID PASTE;            /*!< @image html stock_paste_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID PREFERENCES;      /*!< @image html stock_preferences_24.png         */
extern GTKMM_API const Gtk::BuiltinStockID PRINT;            /*!< @image html stock_print_24.png               */
extern GTKMM_API const Gtk::BuiltinStockID PRINT_PREVIEW;    /*!< @image html stock_print_preview_24.png       */
extern GTKMM_API const Gtk::BuiltinStockID PROPERTIES;       /*!< @image html stock_properties_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID QUIT;             /*!< @image html stock_exit_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID REDO;             /*!< @image html stock_redo_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID REFRESH;          /*!< @image html stock_refresh_24.png             */
extern GTKMM_API const Gtk::BuiltinStockID REMOVE;           /*!< @image html stock_remove_24.png              */
extern GTKMM_API const Gtk::BuiltinStockID REVERT_TO_SAVED;  /*!< @image html stock_revert_24.png              */
extern GTKMM_API const Gtk::BuiltinStockID SAVE;             /*!< @image html stock_save_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID SAVE_AS;          /*!< @image html stock_save_as_24.png             */
extern GTKMM_API const Gtk::BuiltinStockID SELECT_COLOR;     /*!< @image html stock_colorselector_24.png       */
extern GTKMM_API const Gtk::BuiltinStockID SELECT_FONT;      /*!< @image html stock_font_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID SORT_ASCENDING;   /*!< @image html stock_sort_ascending_24.png      */
extern GTKMM_API const Gtk::BuiltinStockID SORT_DESCENDING;  /*!< @image html stock_sort_descending_24.png     */
extern GTKMM_API const Gtk::BuiltinStockID SPELL_CHECK;      /*!< @image html stock_spellcheck_24.png          */
extern GTKMM_API const Gtk::BuiltinStockID STOP;             /*!< @image html stock_stop_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID STRIKETHROUGH;    /*!< @image html stock_text_strikethrough_24.png  */
extern GTKMM_API const Gtk::BuiltinStockID UNDELETE;         /*!< @image html stock_undelete_24.png            */
extern GTKMM_API const Gtk::BuiltinStockID UNDERLINE;        /*!< @image html stock_text_underline_24.png      */
extern GTKMM_API const Gtk::BuiltinStockID UNDO;             /*!< @image html stock_undo_24.png                */
extern GTKMM_API const Gtk::BuiltinStockID YES;              /*!< @image html stock_yes_20.png                 */
extern GTKMM_API const Gtk::BuiltinStockID ZOOM_100;         /*!< @image html stock_zoom_1_24.png              */
extern GTKMM_API const Gtk::BuiltinStockID ZOOM_FIT;         /*!< @image html stock_zoom_fit_24.png            */
extern GTKMM_API const Gtk::BuiltinStockID ZOOM_IN;          /*!< @image html stock_zoom_in_24.png             */
extern GTKMM_API const Gtk::BuiltinStockID ZOOM_OUT;         /*!< @image html stock_zoom_out_24.png            */


void add(const Gtk::StockItem& item);

bool lookup(const Gtk::StockID& stock_id, Gtk::StockItem& item);
bool lookup(const Gtk::StockID& stock_id, Gtk::IconSet& iconset);
bool lookup(const Gtk::StockID& stock_id, Gtk::IconSize size, Gtk::Image& image);

Glib::SListHandle<Gtk::StockID,Gtk::StockID_Traits> get_ids();

} // namespace Stock

} // namespace Gtk


#endif /* _GTKMM_STOCK_H */

