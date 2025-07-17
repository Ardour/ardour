/* GTK - The GIMP Toolkit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */

#ifndef __GTK_H__
#define __GTK_H__

#define __GTK_H_INSIDE__

#include <ydk/gdk.h>
#include <ytk/gtkaboutdialog.h>
#include <ytk/gtkaccelgroup.h>
#include <ytk/gtkaccellabel.h>
#include <ytk/gtkaccelmap.h>
#include <ytk/gtkaccessible.h>
#include <ytk/gtkaction.h>
#include <ytk/gtkactiongroup.h>
#include <ytk/gtkactivatable.h>
#include <ytk/gtkadjustment.h>
#include <ytk/gtkalignment.h>
#include <ytk/gtkarrow.h>
#include <ytk/gtkaspectframe.h>
#include <ytk/gtkassistant.h>
#include <ytk/gtkbbox.h>
#include <ytk/gtkbin.h>
#include <ytk/gtkbindings.h>
#include <ytk/gtkbox.h>
#include <ytk/gtkbuildable.h>
#include <ytk/gtkbuilder.h>
#include <ytk/gtkbutton.h>
#include <ytk/gtkcelleditable.h>
#include <ytk/gtkcelllayout.h>
#include <ytk/gtkcellrenderer.h>
#include <ytk/gtkcellrendereraccel.h>
#include <ytk/gtkcellrenderercombo.h>
#include <ytk/gtkcellrendererpixbuf.h>
#include <ytk/gtkcellrendererprogress.h>
#include <ytk/gtkcellrendererspin.h>
#include <ytk/gtkcellrendererspinner.h>
#include <ytk/gtkcellrenderertext.h>
#include <ytk/gtkcellrenderertoggle.h>
#include <ytk/gtkcellview.h>
#include <ytk/gtkcheckbutton.h>
#include <ytk/gtkcheckmenuitem.h>
#include <ytk/gtkclipboard.h>
#include <ytk/gtkcolorbutton.h>
#include <ytk/gtkcolorsel.h>
#include <ytk/gtkcolorseldialog.h>
#include <ytk/gtkcombobox.h>
#include <ytk/gtkcomboboxentry.h>
#include <ytk/gtkcomboboxtext.h>
#include <ytk/gtkcontainer.h>
#include <ytk/gtkdebug.h>
#include <ytk/gtkdialog.h>
#include <ytk/gtkdnd.h>
#include <ytk/gtkdrawingarea.h>
#include <ytk/gtkeditable.h>
#include <ytk/gtkentry.h>
#include <ytk/gtkentrybuffer.h>
#include <ytk/gtkentrycompletion.h>
#include <ytk/gtkenums.h>
#include <ytk/gtkeventbox.h>
#include <ytk/gtkexpander.h>
#include <ytk/gtkfixed.h>
#include <ytk/gtkfilechooser.h>
#include <ytk/gtkfilechooserbutton.h>
#include <ytk/gtkfilechooserdialog.h>
#include <ytk/gtkfilechooserwidget.h>
#include <ytk/gtkfilefilter.h>
#include <ytk/gtkfontbutton.h>
#include <ytk/gtkfontsel.h>
#include <ytk/gtkframe.h>
#include <ytk/gtkgc.h>
#include <ytk/gtkhandlebox.h>
#include <ytk/gtkhbbox.h>
#include <ytk/gtkhbox.h>
#include <ytk/gtkhpaned.h>
#include <ytk/gtkhruler.h>
#include <ytk/gtkhscale.h>
#include <ytk/gtkhscrollbar.h>
#include <ytk/gtkhseparator.h>
#include <ytk/gtkhsv.h>
#include <ytk/gtkiconfactory.h>
#include <ytk/gtkicontheme.h>
#include <ytk/gtkiconview.h>
#include <ytk/gtkimage.h>
#include <ytk/gtkimagemenuitem.h>
#include <ytk/gtkimcontext.h>
#include <ytk/gtkimcontextsimple.h>
#include <ytk/gtkimmulticontext.h>
#include <ytk/gtkinfobar.h>
#include <ytk/gtkinvisible.h>
#include <ytk/gtkitem.h>
#include <ytk/gtklabel.h>
#include <ytk/gtklayout.h>
#include <ytk/gtklinkbutton.h>
#include <ytk/gtkliststore.h>
#include <ytk/gtkmain.h>
#include <ytk/gtkmenu.h>
#include <ytk/gtkmenubar.h>
#include <ytk/gtkmenuitem.h>
#include <ytk/gtkmenushell.h>
#include <ytk/gtkmenutoolbutton.h>
#include <ytk/gtkmessagedialog.h>
#include <ytk/gtkmisc.h>
#include <ytk/gtkmodules.h>
#include <ytk/gtkmountoperation.h>
#include <ytk/gtknotebook.h>
#include <ytk/gtkobject.h>
#include <ytk/gtkoffscreenwindow.h>
#include <ytk/gtkorientable.h>
#include <ytk/gtkpaned.h>
#include <ytk/gtkplug.h>
#include <ytk/gtkprogressbar.h>
#include <ytk/gtkradioaction.h>
#include <ytk/gtkradiobutton.h>
#include <ytk/gtkradiomenuitem.h>
#include <ytk/gtkradiotoolbutton.h>
#include <ytk/gtkrange.h>
#include <ytk/gtkrc.h>
#include <ytk/gtkrecentaction.h>
#include <ytk/gtkrecentchooser.h>
#include <ytk/gtkrecentchooserdialog.h>
#include <ytk/gtkrecentchoosermenu.h>
#include <ytk/gtkrecentchooserwidget.h>
#include <ytk/gtkrecentfilter.h>
#include <ytk/gtkrecentmanager.h>
#include <ytk/gtkruler.h>
#include <ytk/gtkscale.h>
#include <ytk/gtkscalebutton.h>
#include <ytk/gtkscrollbar.h>
#include <ytk/gtkscrolledwindow.h>
#include <ytk/gtkselection.h>
#include <ytk/gtkseparator.h>
#include <ytk/gtkseparatormenuitem.h>
#include <ytk/gtkseparatortoolitem.h>
#include <ytk/gtksettings.h>
#include <ytk/gtkshow.h>
#include <ytk/gtksizegroup.h>
#include <ytk/gtksocket.h>
#include <ytk/gtkspinbutton.h>
#include <ytk/gtkspinner.h>
#include <ytk/gtkstatusbar.h>
#include <ytk/gtkstock.h>
#include <ytk/gtkstyle.h>
#include <ytk/gtktable.h>
#include <ytk/gtktearoffmenuitem.h>
#include <ytk/gtktextbuffer.h>
#include <ytk/gtktextbufferrichtext.h>
#include <ytk/gtktextchild.h>
#include <ytk/gtktextiter.h>
#include <ytk/gtktextmark.h>
#include <ytk/gtktexttag.h>
#include <ytk/gtktexttagtable.h>
#include <ytk/gtktextview.h>
#include <ytk/gtktoggleaction.h>
#include <ytk/gtktogglebutton.h>
#include <ytk/gtktoggletoolbutton.h>
#include <ytk/gtktoolbar.h>
#include <ytk/gtktoolbutton.h>
#include <ytk/gtktoolitem.h>
#include <ytk/gtktoolitemgroup.h>
#include <ytk/gtktoolpalette.h>
#include <ytk/gtktoolshell.h>
#include <ytk/gtktooltip.h>
#include <ytk/gtktestutils.h>
#include <ytk/gtktreednd.h>
#include <ytk/gtktreemodel.h>
#include <ytk/gtktreemodelfilter.h>
#include <ytk/gtktreemodelsort.h>
#include <ytk/gtktreeselection.h>
#include <ytk/gtktreesortable.h>
#include <ytk/gtktreestore.h>
#include <ytk/gtktreeview.h>
#include <ytk/gtktreeviewcolumn.h>
#include <ytk/gtktypeutils.h>
#include <ytk/gtkuimanager.h>
#include <ytk/gtkvbbox.h>
#include <ytk/gtkvbox.h>
#include <ytk/gtkversion.h>
#include <ytk/gtkviewport.h>
#include <ytk/gtkvpaned.h>
#include <ytk/gtkvruler.h>
#include <ytk/gtkvscale.h>
#include <ytk/gtkvscrollbar.h>
#include <ytk/gtkvseparator.h>
#include <ytk/gtkwidget.h>
#include <ytk/gtkwindow.h>

/* Deprecated */
#include <ytk/gtkoptionmenu.h>
#include <ytk/gtkprogress.h>
#include <ytk/gtktooltips.h>

#undef __GTK_H_INSIDE__

#endif /* __GTK_H__ */
