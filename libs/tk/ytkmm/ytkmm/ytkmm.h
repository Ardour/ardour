/* gtkmm - a C++ wrapper for the Gtk toolkit
 *
 * Copyright 1999-2002 The gtkmm Development Team
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

#ifndef _GTKMM_H
#define _GTKMM_H

/** @mainpage gtkmm Reference Manual
 *
 * @section description Description
 *
 * gtkmm is the official C++ interface for the popular GUI library GTK+.
 * Highlights include typesafe callbacks, and a comprehensive set of widgets
 * that are easily extensible via inheritance.
 *
 * For instance, see @ref Widgets, @ref Dialogs, @ref TreeView "TreeView" and
 * @ref TextView "TextView".
 *
 * See also the
 * <a href="http://library.gnome.org/devel/gtkmm-tutorial/stable/">Programming
 * with gtkmm</a> book.
 *
 *
 * @section features Features
 *
 * - GTK+’s mature, capable set of @ref widgets Widgets. See
 *   <a href="http://www.gtk.org/">the GTK+ website</a> for more information.
 * - Use inheritance to derive custom widgets.
 * - Type-safe signal handlers (slots), in standard C++, using
 *   <a href="http://libsigc.sourceforge.net/">libsigc++</a>.
 * - Polymorphism.
 * - Use of the Standard C++ Library, including strings, containers and
 *   iterators.
 * - Full internationalisation with UTF8.
 * - Complete C++ memory management.
 *   - Member instances or dynamic new and delete.
 *   - Optional automatic deletion of child widgets.
 *   - No manual reference-counting.
 * - Full use of C++ namespaces.
 * - No macros.
 *
 * @section basics Basic Usage
 *
 * Include the gtkmm header:
 * @code
 * #include <gtkmm.h>
 * @endcode
 * (You may include individual headers, such as @c gtkmm/button.h instead.)
 *
 * If your source file is @c program.cc, you can compile it with:
 * @code
 * g++ program.cc -o program  `pkg-config --cflags --libs gtkmm-2.4`
 * @endcode
 *
 * Alternatively, if using autoconf, use the following in @c configure.ac:
 * @code
 * PKG_CHECK_MODULES([GTKMM], [gtkmm-2.4])
 * @endcode
 * Then use the generated @c GTKMM_CFLAGS and @c GTKMM_LIBS variables in the
 * project @c Makefile.am files. For example:
 * @code
 * program_CPPFLAGS = $(GTKMM_CFLAGS)
 * program_LDADD = $(GTKMM_LIBS)
 * @endcode
 */

/* Gtkmm version.  */
extern const int gtkmm_major_version;
extern const int gtkmm_minor_version;
extern const int gtkmm_micro_version;

#ifndef COMPILER_MSVC
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#include <glibmm.h>
#pragma GCC diagnostic pop
#else
#include <glibmm.h>
#endif
#include <giomm.h>
#include <ydkmm/ydkmm.h>

#include <ytkmm/box.h>
#include <ytkmm/dialog.h>
#include <ytkmm/object.h>
#include <ytkmm/aboutdialog.h>
#include <ytkmm/accelkey.h>
#include <ytkmm/accelgroup.h>
#include <ytkmm/adjustment.h>
#include <ytkmm/alignment.h>
#include <ytkmm/arrow.h>
#include <ytkmm/aspectframe.h>
#include <ytkmm/assistant.h>
#include <ytkmm/base.h>
#include <ytkmm/bin.h>
#include <ytkmm/border.h>
#include <ytkmm/builder.h>
#include <ytkmm/button.h>
#include <ytkmm/buttonbox.h>
#include <ytkmm/cellview.h>
#include <ytkmm/checkbutton.h>
#include <ytkmm/checkmenuitem.h>
#include <ytkmm/cellrenderer.h>
#include <ytkmm/cellrendereraccel.h>
#include <ytkmm/cellrenderercombo.h>
#include <ytkmm/cellrendererpixbuf.h>
#include <ytkmm/cellrendererprogress.h>
#include <ytkmm/cellrendererspin.h>
#include <ytkmm/cellrendererspinner.h>
#include <ytkmm/cellrenderertext.h>
#include <ytkmm/cellrenderertoggle.h>
#include <ytkmm/colorbutton.h>
#include <ytkmm/colorselection.h>
#include <ytkmm/combobox.h>
#include <ytkmm/comboboxentry.h>
#include <ytkmm/comboboxentrytext.h>
#include <ytkmm/comboboxtext.h>
#include <ytkmm/container.h>
#include <ytkmm/drawingarea.h>
#include <ytkmm/editable.h>
#include <ytkmm/entry.h>
#include <ytkmm/expander.h>
#include <ytkmm/enums.h>
#include <ytkmm/eventbox.h>
#include <ytkmm/filechooser.h>
#include <ytkmm/filechooserbutton.h>
#include <ytkmm/filechooserdialog.h>
#include <ytkmm/filechooserwidget.h>
#include <ytkmm/filefilter.h>
#include <ytkmm/fixed.h>
#include <ytkmm/fontbutton.h>
#include <ytkmm/fontselection.h>
#include <ytkmm/frame.h>
//#include <ytkmm/rc.h>
#include <ytkmm/handlebox.h>
#include <ytkmm/iconset.h>
#include <ytkmm/iconfactory.h>
#include <ytkmm/iconsource.h>
#include <ytkmm/icontheme.h>
#include <ytkmm/iconview.h>
#include <ytkmm/image.h>
#include <ytkmm/imagemenuitem.h>
#include <ytkmm/infobar.h>
#include <ytkmm/item.h>
#include <ytkmm/invisible.h>
#include <ytkmm/label.h>
#include <ytkmm/layout.h>
#include <ytkmm/liststore.h>
#include <ytkmm/listviewtext.h>
#include <ytkmm/linkbutton.h>
#include <ytkmm/main.h>
#include <ytkmm/menu.h>
#include <ytkmm/menu_elems.h>
#include <ytkmm/menubar.h>
#include <ytkmm/menuitem.h>
#include <ytkmm/menushell.h>
#include <ytkmm/messagedialog.h>
#include <ytkmm/misc.h>
#include <ytkmm/notebook.h>
#include <ytkmm/object.h>
#include <ytkmm/offscreenwindow.h>
#include <ytkmm/optionmenu.h>
#include <ytkmm/paned.h>
#include <ytkmm/progressbar.h>
#include <ytkmm/radioaction.h>
#include <ytkmm/radiobutton.h>
#include <ytkmm/radiomenuitem.h>
#include <ytkmm/radiotoolbutton.h>
#include <ytkmm/range.h>
#include <ytkmm/recentaction.h>
#include <ytkmm/recentchooser.h>
#include <ytkmm/recentchooserdialog.h>
#include <ytkmm/recentchoosermenu.h>
#include <ytkmm/recentchooserwidget.h>
#include <ytkmm/recentfilter.h>
#include <ytkmm/recentinfo.h>
#include <ytkmm/recentmanager.h>
#include <ytkmm/ruler.h>
#include <ytkmm/scale.h>
#include <ytkmm/scrollbar.h>
#include <ytkmm/scrolledwindow.h>
#include <ytkmm/separator.h>
#include <ytkmm/separatormenuitem.h>
#include <ytkmm/separatortoolitem.h>
#include <ytkmm/settings.h>
#include <ytkmm/sizegroup.h>
#include <ytkmm/spinbutton.h>
#include <ytkmm/spinner.h>
#include <ytkmm/statusbar.h>
#include <ytkmm/stock.h>
#include <ytkmm/stockid.h>
#include <ytkmm/stockitem.h>
#include <ytkmm/style.h>
#include <ytkmm/table.h>
#include <ytkmm/tearoffmenuitem.h>
#include <ytkmm/textbuffer.h>
#include <ytkmm/textchildanchor.h>
#include <ytkmm/textiter.h>
#include <ytkmm/textmark.h>
#include <ytkmm/texttag.h>
#include <ytkmm/texttagtable.h>
#include <ytkmm/textview.h>
#include <ytkmm/toggleaction.h>
#include <ytkmm/togglebutton.h>
#include <ytkmm/toolbar.h>
#include <ytkmm/toolitem.h>
#include <ytkmm/toolbutton.h>
#include <ytkmm/toolpalette.h>
#include <ytkmm/toggletoolbutton.h>
#include <ytkmm/menutoolbutton.h>
#include <ytkmm/tooltip.h>
#include <ytkmm/tooltips.h>
#include <ytkmm/treemodel.h>
#include <ytkmm/treemodelfilter.h>
#include <ytkmm/treemodelsort.h>
#include <ytkmm/treepath.h>
#include <ytkmm/treerowreference.h>
#include <ytkmm/treeselection.h>
#include <ytkmm/treestore.h>
#include <ytkmm/treeview.h>
#include <ytkmm/treeviewcolumn.h>
#include <ytkmm/uimanager.h>
#include <ytkmm/viewport.h>
#include <ytkmm/widget.h>
#include <ytkmm/window.h>

#endif /* #ifndef GTKMM_H */
