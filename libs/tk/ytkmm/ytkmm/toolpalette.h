// -*- c++ -*-
// Generated by gmmproc 2.45.3 -- DO NOT MODIFY!
#ifndef _GTKMM_TOOLPALETTE_H
#define _GTKMM_TOOLPALETTE_H


#include <glibmm/ustring.h>
#include <sigc++/sigc++.h>

/* Copyright (C) 2009 The gtkmm Development Team
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

#include <ytkmm/container.h>
#include <ytkmm/toolitem.h>
#include <ytkmm/orientable.h>
#include <ytkmm/adjustment.h>
#include <ytkmm/toolitemgroup.h>


#ifndef DOXYGEN_SHOULD_SKIP_THIS
typedef struct _GtkToolPalette GtkToolPalette;
typedef struct _GtkToolPaletteClass GtkToolPaletteClass;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gtk
{ class ToolPalette_Class; } // namespace Gtk
namespace Gtk
{

/** @addtogroup gtkmmEnums gtkmm Enums and Flags */

/** 
 *
 * @ingroup gtkmmEnums
 * @par Bitwise operators:
 * <tt>%ToolPaletteDragTargets operator|(ToolPaletteDragTargets, ToolPaletteDragTargets)</tt><br>
 * <tt>%ToolPaletteDragTargets operator&(ToolPaletteDragTargets, ToolPaletteDragTargets)</tt><br>
 * <tt>%ToolPaletteDragTargets operator^(ToolPaletteDragTargets, ToolPaletteDragTargets)</tt><br>
 * <tt>%ToolPaletteDragTargets operator~(ToolPaletteDragTargets)</tt><br>
 * <tt>%ToolPaletteDragTargets& operator|=(ToolPaletteDragTargets&, ToolPaletteDragTargets)</tt><br>
 * <tt>%ToolPaletteDragTargets& operator&=(ToolPaletteDragTargets&, ToolPaletteDragTargets)</tt><br>
 * <tt>%ToolPaletteDragTargets& operator^=(ToolPaletteDragTargets&, ToolPaletteDragTargets)</tt><br>
 */
enum ToolPaletteDragTargets
{
  TOOL_PALETTE_DRAG_ITEMS = 1 << 0,
  TOOL_PALETTE_DRAG_GROUPS = 1 << 1
};

/** @ingroup gtkmmEnums */
inline ToolPaletteDragTargets operator|(ToolPaletteDragTargets lhs, ToolPaletteDragTargets rhs)
  { return static_cast<ToolPaletteDragTargets>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs)); }

/** @ingroup gtkmmEnums */
inline ToolPaletteDragTargets operator&(ToolPaletteDragTargets lhs, ToolPaletteDragTargets rhs)
  { return static_cast<ToolPaletteDragTargets>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs)); }

/** @ingroup gtkmmEnums */
inline ToolPaletteDragTargets operator^(ToolPaletteDragTargets lhs, ToolPaletteDragTargets rhs)
  { return static_cast<ToolPaletteDragTargets>(static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs)); }

/** @ingroup gtkmmEnums */
inline ToolPaletteDragTargets operator~(ToolPaletteDragTargets flags)
  { return static_cast<ToolPaletteDragTargets>(~static_cast<unsigned>(flags)); }

/** @ingroup gtkmmEnums */
inline ToolPaletteDragTargets& operator|=(ToolPaletteDragTargets& lhs, ToolPaletteDragTargets rhs)
  { return (lhs = static_cast<ToolPaletteDragTargets>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs))); }

/** @ingroup gtkmmEnums */
inline ToolPaletteDragTargets& operator&=(ToolPaletteDragTargets& lhs, ToolPaletteDragTargets rhs)
  { return (lhs = static_cast<ToolPaletteDragTargets>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs))); }

/** @ingroup gtkmmEnums */
inline ToolPaletteDragTargets& operator^=(ToolPaletteDragTargets& lhs, ToolPaletteDragTargets rhs)
  { return (lhs = static_cast<ToolPaletteDragTargets>(static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs))); }

} // namespace Gtk


#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Glib
{

template <>
class Value<Gtk::ToolPaletteDragTargets> : public Glib::Value_Flags<Gtk::ToolPaletteDragTargets>
{
public:
  static GType value_type() G_GNUC_CONST;
};

} // namespace Glib
#endif /* DOXYGEN_SHOULD_SKIP_THIS */


namespace Gtk
{


/** A ToolPalette allows you to add ToolItems to a palette-like container with
 * various categories and drag and drop support.
 *
 * ToolItems cannot be added directly to a ToolPalette - instead they are added
 * to a ToolItemGroup which can than be added to a ToolPalette. To add a
 * ToolItemGroup to a ToolPalette, use Gtk::Container::add().
 *
 * The easiest way to use drag and drop with ToolPalette is to call
 * add_drag_dest() with the desired drag source palette and the desired drag
 * target widget. Then get_drag_item() can be used to get the dragged item in
 * the "drag-data-received" signal handler of the drag target.
 *
 * @ingroup Widgets
 * @ingroup Containers
 */

class ToolPalette
: public Container,
  public Orientable
{
  public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  typedef ToolPalette CppObjectType;
  typedef ToolPalette_Class CppClassType;
  typedef GtkToolPalette BaseObjectType;
  typedef GtkToolPaletteClass BaseClassType;
#endif /* DOXYGEN_SHOULD_SKIP_THIS */

  virtual ~ToolPalette();

#ifndef DOXYGEN_SHOULD_SKIP_THIS

private:
  friend class ToolPalette_Class;
  static CppClassType toolpalette_class_;

  // noncopyable
  ToolPalette(const ToolPalette&);
  ToolPalette& operator=(const ToolPalette&);

protected:
  explicit ToolPalette(const Glib::ConstructParams& construct_params);
  explicit ToolPalette(GtkToolPalette* castitem);

#endif /* DOXYGEN_SHOULD_SKIP_THIS */

public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  static GType get_type()      G_GNUC_CONST;


  static GType get_base_type() G_GNUC_CONST;
#endif

  ///Provides access to the underlying C GtkObject.
  GtkToolPalette*       gobj()       { return reinterpret_cast<GtkToolPalette*>(gobject_); }

  ///Provides access to the underlying C GtkObject.
  const GtkToolPalette* gobj() const { return reinterpret_cast<GtkToolPalette*>(gobject_); }


public:
  //C++ methods used to invoke GTK+ virtual functions:

protected:
  //GTK+ Virtual Functions (override these to change behaviour):

  //Default Signal Handlers::


private:

  
public:
  ToolPalette();


  /** Sets the position of the group as an index of the tool palette.
   * If position is 0 the group will become the first child, if position is
   * -1 it will become the last child.
   * 
   * @param group A Gtk::ToolItemGroup which is a child of palette.
   * @param position A new index for group.
   */
  void set_group_position(ToolItemGroup& group, int position);
  
  /** Sets whether the group should be exclusive or not.
   * If an exclusive group is expanded all other groups are collapsed.
   * 
   * @param group A Gtk::ToolItemGroup which is a child of palette.
   * @param exclusive Whether the group should be exclusive or not.
   */
  void set_exclusive(ToolItemGroup& group, bool exclusive);
  
  /** Sets whether the group should be given extra space.
   * 
   * @param group A Gtk::ToolItemGroup which is a child of palette.
   * @param expand Whether the group should be given extra space.
   */
  void set_expand(ToolItemGroup& group, bool expand =  true);

  
  /** Gets the position of @a group in @a palette as index.
   * See set_group_position().
   * 
   * @param group A Gtk::ToolItemGroup.
   * @return The index of group or -1 if @a group is not a child of @a palette.
   */
  int get_group_position(ToolItemGroup& group) const;
  
  /** Gets whether @a group is exclusive or not.
   * See set_exclusive().
   * 
   * @param group A Gtk::ToolItemGroup which is a child of palette.
   * @return <tt>true</tt> if @a group is exclusive.
   */
  bool get_exclusive(ToolItemGroup& group) const;
  
  /** Gets whether group should be given extra space.
   * See set_expand().
   * 
   * @param group A Gtk::ToolItemGroup which is a child of palette.
   * @return <tt>true</tt> if group should be given extra space, <tt>false</tt> otherwise.
   */
  bool get_expand(ToolItemGroup& group) const;

  
  /** Sets the size of icons in the tool palette.
   * 
   * @param icon_size The Gtk::IconSize that icons in the tool
   * palette shall have.
   */
  void set_icon_size(IconSize icon_size);
  
  /** Unsets the tool palette icon size set with set_icon_size(),
   * so that user preferences will be used to determine the icon size.
   */
  void unset_icon_size();
  
  /** Sets the style (text, icons or both) of items in the tool palette.
   * 
   * @param style The Gtk::ToolbarStyle that items in the tool palette shall have.
   */
  void set_style(ToolbarStyle style);
  
  /** Unsets a toolbar style set with set_style(),
   * so that user preferences will be used to determine the toolbar style.
   */
  void unset_style();

  
  /** Gets the size of icons in the tool palette.
   * See set_icon_size().
   * 
   * @return The Gtk::IconSize of icons in the tool palette.
   */
  IconSize get_icon_size() const;
  
  /** Gets the style (icons, text or both) of items in the tool palette.
   * 
   * @return The Gtk::ToolbarStyle of items in the tool palette.
   */
  ToolbarStyle get_style() const;

  
  /** Gets the item at position (x, y).
   * See get_drop_group().
   * 
   * @param x The x position.
   * @param y The y position.
   * @return The Gtk::ToolItem at position or <tt>0</tt> if there is no such item.
   */
  ToolItem* get_drop_item(int x, int y);
  
  /** Gets the item at position (x, y).
   * See get_drop_group().
   * 
   * @param x The x position.
   * @param y The y position.
   * @return The Gtk::ToolItem at position or <tt>0</tt> if there is no such item.
   */
  const ToolItem* get_drop_item(int x, int y) const;

//This conversion is needed because of https://bugzilla.gnome.org/show_bug.cgi?id=567729#c37
 

  /** Gets the group at position (x, y).
   * 
   * @param x The x position.
   * @param y The y position.
   * @return The Gtk::ToolItemGroup at position or <tt>0</tt>
   * if there is no such group.
   */
  ToolItemGroup* get_drop_group(int x, int y);
  
  /** Gets the group at position (x, y).
   * 
   * @param x The x position.
   * @param y The y position.
   * @return The Gtk::ToolItemGroup at position or <tt>0</tt>
   * if there is no such group.
   */
  const ToolItemGroup* get_drop_group(int x, int y) const;

  
  /** Get the dragged item from the selection.
   * This could be a Gtk::ToolItem or a Gtk::ToolItemGroup.
   * 
   * @param selection A Gtk::SelectionData.
   * @return The dragged item in selection.
   */
  ToolItem* get_drag_item(const SelectionData& selection);
  
  /** Get the dragged item from the selection.
   * This could be a Gtk::ToolItem or a Gtk::ToolItemGroup.
   * 
   * @param selection A Gtk::SelectionData.
   * @return The dragged item in selection.
   */
  const ToolItem* get_drag_item(const SelectionData& selection) const;

  
  /** Sets the tool palette as a drag source.
   * Enables all groups and items in the tool palette as drag sources
   * on button 1 and button 3 press with copy and move actions.
   * See Gtk::DragSource::set().
   * 
   * @param targets The Gtk::ToolPaletteDragTarget<!-- -->s
   * which the widget should support.
   */
  void set_drag_source(ToolPaletteDragTargets targets =  TOOL_PALETTE_DRAG_ITEMS);
  
  /** Sets @a palette as drag source (see set_drag_source())
   * and sets @a widget as a drag destination for drags from @a palette.
   * See gtk_drag_dest_set().
   * 
   * @param widget A Gtk::Widget which should be a drag destination for @a palette.
   * @param flags The flags that specify what actions GTK+ should take for drops
   * on that widget.
   * @param targets The Gtk::ToolPaletteDragTarget<!-- -->s which the widget
   * should support.
   * @param actions The Gdk::DragAction<!-- -->s which the widget should suppport.
   */
  void add_drag_dest(Gtk::Widget& widget, DestDefaults flags =  DEST_DEFAULT_ALL, ToolPaletteDragTargets targets =  TOOL_PALETTE_DRAG_ITEMS, Gdk::DragAction actions =  Gdk::ACTION_COPY);

  
  /** Gets the horizontal adjustment of the tool palette.
   * 
   * @return The horizontal adjustment of @a palette.
   */
  Adjustment* get_hadjustment();
  
  /** Gets the horizontal adjustment of the tool palette.
   * 
   * @return The horizontal adjustment of @a palette.
   */
  const Adjustment* get_hadjustment() const;
  
  /** Gets the vertical adjustment of the tool palette.
   * 
   * @return The vertical adjustment of @a palette.
   */
  Adjustment* get_vadjustment();
  
  /** Gets the vertical adjustment of the tool palette.
   * 
   * @return The vertical adjustment of @a palette.
   */
  const Adjustment* get_vadjustment() const;


  /** Gets the target entry for a dragged Gtk::ToolItem.
   * 
   * @return The Gtk::TargetEntry for a dragged item.
   */
  static TargetEntry get_drag_target_item();
  
  /** Get the target entry for a dragged Gtk::ToolItemGroup.
   * 
   * @return The Gtk::TargetEntry for a dragged group.
   */
  static TargetEntry get_drag_target_group();

  //Ignore the set_scroll_adjustment signal. It's in many widgets and seems internal.
  //_WRAP_SIGNAL(void set_scroll_adjustments(Adjustment* hadjustment, Adjustment* vadjustment), "set-scroll-adjustments")
  

  /** Size of icons in this tool palette.
   *
   * @return A PropertyProxy that allows you to get or set the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy< IconSize > property_icon_size() ;

/** Size of icons in this tool palette.
   *
   * @return A PropertyProxy_ReadOnly that allows you to get the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly< IconSize > property_icon_size() const;

  /** Whether the icon-size property has been set.
   *
   * @return A PropertyProxy that allows you to get or set the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy< bool > property_icon_size_set() ;

/** Whether the icon-size property has been set.
   *
   * @return A PropertyProxy_ReadOnly that allows you to get the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly< bool > property_icon_size_set() const;

  /** Style of items in the tool palette.
   *
   * @return A PropertyProxy that allows you to get or set the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy< ToolbarStyle > property_toolbar_style() ;

/** Style of items in the tool palette.
   *
   * @return A PropertyProxy_ReadOnly that allows you to get the value of the property,
   * or receive notification when the value of the property changes.
   */
  Glib::PropertyProxy_ReadOnly< ToolbarStyle > property_toolbar_style() const;


};

} // namespace Gtk


namespace Glib
{
  /** A Glib::wrap() method for this object.
   * 
   * @param object The C instance.
   * @param take_copy False if the result should take ownership of the C instance. True if it should take a new copy or ref.
   * @result A C++ instance that wraps this C instance.
   *
   * @relates Gtk::ToolPalette
   */
  Gtk::ToolPalette* wrap(GtkToolPalette* object, bool take_copy = false);
} //namespace Glib


#endif /* _GTKMM_TOOLPALETTE_H */

