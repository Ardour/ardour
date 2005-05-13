/* $Id$ */
/* gtkmm - a C++ wrapper for the Gtk toolkit
 *
 * Copyright 1999-2002 The gtkmm Development Team
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

#ifndef _GTKMM_H
#define _GTKMM_H

/* Gtkmm version.  */
extern const int gtkmm_major_version;
extern const int gtkmm_minor_version;
extern const int gtkmm_micro_version;

#include <glibmm.h>
#include <gdkmm.h>

#include <gtkmm/object.h>
#include <gtkmm/aboutdialog.h>
#include <gtkmm/accelkey.h>
#include <gtkmm/accelgroup.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/alignment.h>
#include <gtkmm/arrow.h>
#include <gtkmm/aspectframe.h>
#include <gtkmm/base.h>
#include <gtkmm/bin.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/checkmenuitem.h>
#include <gtkmm/cellrenderer.h>
#include <gtkmm/cellrenderercombo.h>
#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/cellrendererprogress.h>
#include <gtkmm/cellrenderertext.h>
#include <gtkmm/cellrenderertoggle.h>
#include <gtkmm/colorbutton.h>
#include <gtkmm/colorselection.h>
#include <gtkmm/combo.h> //TODO: Deprecated.
#include <gtkmm/combobox.h>
#include <gtkmm/comboboxentry.h>
#include <gtkmm/comboboxentrytext.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/container.h>
#include <gtkmm/curve.h>
#include <gtkmm/dialog.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/editable.h>
#include <gtkmm/entry.h>
#include <gtkmm/expander.h>
#include <gtkmm/enums.h>
#include <gtkmm/eventbox.h>
#include <gtkmm/filechooser.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/filechooserdialog.h>
#include <gtkmm/filechooserwidget.h>
#include <gtkmm/filefilter.h>
#include <gtkmm/fileselection.h> //TODO: Deprecated.
#include <gtkmm/fixed.h>
#include <gtkmm/fontbutton.h>
#include <gtkmm/fontselection.h>
#include <gtkmm/frame.h>
//#include <gtkmm/rc.h>
#include <gtkmm/handlebox.h>
#include <gtkmm/iconset.h>
#include <gtkmm/iconfactory.h>
#include <gtkmm/iconsource.h>
#include <gtkmm/iconview.h>
#include <gtkmm/image.h>
#include <gtkmm/imagemenuitem.h>
#include <gtkmm/inputdialog.h>
#include <gtkmm/item.h>
#include <gtkmm/calendar.h>
#include <gtkmm/invisible.h>
#include <gtkmm/label.h>
#include <gtkmm/liststore.h>
#include <gtkmm/main.h>
#include <gtkmm/menu.h>
#include <gtkmm/menu_elems.h>
#include <gtkmm/menubar.h>
#include <gtkmm/menuitem.h>
#include <gtkmm/menushell.h>
#include <gtkmm/messagedialog.h>
#include <gtkmm/misc.h>
#include <gtkmm/notebook.h>
#include <gtkmm/object.h>
#include <gtkmm/optionmenu.h>
#include <gtkmm/paned.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/radioaction.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/radiomenuitem.h>
#include <gtkmm/radiotoolbutton.h>
#include <gtkmm/range.h>
#include <gtkmm/ruler.h>
#include <gtkmm/scale.h>
#include <gtkmm/scrollbar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/separator.h>
#include <gtkmm/separatormenuitem.h>
#include <gtkmm/separatortoolitem.h>
#include <gtkmm/settings.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/statusbar.h>
#include <gtkmm/stock.h>
#include <gtkmm/stockid.h>
#include <gtkmm/stockitem.h>
#include <gtkmm/style.h>
#include <gtkmm/table.h>
#include <gtkmm/textbuffer.h>
#include <gtkmm/textchildanchor.h>
#include <gtkmm/textiter.h>
#include <gtkmm/textmark.h>
#include <gtkmm/texttag.h>
#include <gtkmm/texttagtable.h>
#include <gtkmm/textview.h>
#include <gtkmm/toggleaction.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/toolbar.h>
#include <gtkmm/toolitem.h>
#include <gtkmm/toolbutton.h>
#include <gtkmm/toggletoolbutton.h>
#include <gtkmm/tooltips.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treemodelfilter.h>
#include <gtkmm/treemodelsort.h>
#include <gtkmm/treepath.h>
#include <gtkmm/treerowreference.h>
#include <gtkmm/treeselection.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/treeviewcolumn.h>
#include <gtkmm/uimanager.h>
#include <gtkmm/viewport.h>
#include <gtkmm/widget.h>
#include <gtkmm/window.h>

#endif /* #ifndef GTKMM_H */

