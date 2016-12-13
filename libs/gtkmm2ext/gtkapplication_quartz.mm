/* GTK+ application-level integration for the Mac OS X/Cocoa 
 *
 * Copyright (C) 2007 Pioneer Research Center USA, Inc.
 * Copyright (C) 2007 Imendio AB
 * Copyright (C) 2009 Paul Davis
 *
 * This is a reimplementation in Cocoa of the sync-menu.c concept
 * from Imendio, although without the "set quit menu" API since
 * a Cocoa app needs to handle termination anyway.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; version 2.1
 * of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <sigc++/signal.h>
#include <sigc++/slot.h>

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <gtkmm2ext/gtkapplication.h>
#include <gtkmm2ext/gtkapplication-private.h>

#import <AppKit/NSMenu.h>
#import <AppKit/NSMenuItem.h>
#import <AppKit/NSCell.h>
#import <AppKit/NSEvent.h>
#import <AppKit/NSApplication.h>
#import <Foundation/NSString.h>
#import <Foundation/NSNotification.h>

#define UNUSED_PARAMETER(a) (void) (a)

// #define DEBUG(format, ...) g_printerr ("%s: " format, G_STRFUNC, ## __VA_ARGS__)
#define DEBUG(format, ...)

/* TODO
 *
 * - Sync adding/removing/reordering items
 * - Create on demand? (can this be done with gtk+? ie fill in menu
     items when the menu is opened)
 * - Figure out what to do per app/window...
 *
 */

static gint _exiting = 0;

static guint
gdk_quartz_keyval_to_ns_keyval (guint keyval)
{
	switch (keyval) {
	case GDK_BackSpace:
		return NSBackspaceCharacter;
	case GDK_Delete:
		return NSDeleteFunctionKey;
	case GDK_Pause:
		return NSPauseFunctionKey;
	case GDK_Scroll_Lock:
		return NSScrollLockFunctionKey;
	case GDK_Sys_Req:
		return NSSysReqFunctionKey;
	case GDK_Home:
		return NSHomeFunctionKey;
	case GDK_Left:
	case GDK_leftarrow:
		return NSLeftArrowFunctionKey;
	case GDK_Up:
	case GDK_uparrow:
		return NSUpArrowFunctionKey;
	case GDK_Right:
	case GDK_rightarrow:
		return NSRightArrowFunctionKey;
	case GDK_Down:
	case GDK_downarrow:
		return NSDownArrowFunctionKey;
	case GDK_Page_Up:
		return NSPageUpFunctionKey;
	case GDK_Page_Down:
		return NSPageDownFunctionKey;
	case GDK_End:
		return NSEndFunctionKey;
	case GDK_Begin:
		return NSBeginFunctionKey;
	case GDK_Select:
		return NSSelectFunctionKey;
	case GDK_Print:
		return NSPrintFunctionKey;
	case GDK_Execute:
		return NSExecuteFunctionKey;
	case GDK_Insert:
		return NSInsertFunctionKey;
	case GDK_Undo:
		return NSUndoFunctionKey;
	case GDK_Redo:
		return NSRedoFunctionKey;
	case GDK_Menu:
		return NSMenuFunctionKey;
	case GDK_Find:
		return NSFindFunctionKey;
	case GDK_Help:
		return NSHelpFunctionKey;
	case GDK_Break:
		return NSBreakFunctionKey;
	case GDK_Mode_switch:
		return NSModeSwitchFunctionKey;
	case GDK_F1:
		return NSF1FunctionKey;
	case GDK_F2:
		return NSF2FunctionKey;
	case GDK_F3:
		return NSF3FunctionKey;
	case GDK_F4:
		return NSF4FunctionKey;
	case GDK_F5:
		return NSF5FunctionKey;
	case GDK_F6:
		return NSF6FunctionKey;
	case GDK_F7:
		return NSF7FunctionKey;
	case GDK_F8:
		return NSF8FunctionKey;
	case GDK_F9:
		return NSF9FunctionKey;
	case GDK_F10:
		return NSF10FunctionKey;
	case GDK_F11:
		return NSF11FunctionKey;
	case GDK_F12:
		return NSF12FunctionKey;
	case GDK_F13:
		return NSF13FunctionKey;
	case GDK_F14:
		return NSF14FunctionKey;
	case GDK_F15:
		return NSF15FunctionKey;
	case GDK_F16:
		return NSF16FunctionKey;
	case GDK_F17:
		return NSF17FunctionKey;
	case GDK_F18:
		return NSF18FunctionKey;
	case GDK_F19:
		return NSF19FunctionKey;
	case GDK_F20:
		return NSF20FunctionKey;
	case GDK_F21:
		return NSF21FunctionKey;
	case GDK_F22:
		return NSF22FunctionKey;
	case GDK_F23:
		return NSF23FunctionKey;
	case GDK_F24:
		return NSF24FunctionKey;
	case GDK_F25:
		return NSF25FunctionKey;
	case GDK_F26:
		return NSF26FunctionKey;
	case GDK_F27:
		return NSF27FunctionKey;
	case GDK_F28:
		return NSF28FunctionKey;
	case GDK_F29:
		return NSF29FunctionKey;
	case GDK_F30:
		return NSF30FunctionKey;
	case GDK_F31:
		return NSF31FunctionKey;
	case GDK_F32:
		return NSF32FunctionKey;
	case GDK_F33:
		return NSF33FunctionKey;
	case GDK_F34:
		return NSF34FunctionKey;
	case GDK_F35:
		return NSF35FunctionKey;
	default:
		break;
	}

	return 0;
}

static gboolean
keyval_is_keypad (guint keyval)
{
	switch (keyval) {
	case GDK_KP_F1:
	case GDK_KP_F2:
	case GDK_KP_F3:
	case GDK_KP_F4:
	case GDK_KP_Home:
	case GDK_KP_Left:
	case GDK_KP_Up:
	case GDK_KP_Right:
	case GDK_KP_Down:
	case GDK_KP_Page_Up:
	case GDK_KP_Page_Down:
	case GDK_KP_End:
	case GDK_KP_Begin:
	case GDK_KP_Insert:
	case GDK_KP_Delete:
	case GDK_KP_Equal:
	case GDK_KP_Multiply:
	case GDK_KP_Add:
	case GDK_KP_Separator:
	case GDK_KP_Subtract:
	case GDK_KP_Decimal:
	case GDK_KP_Divide:
	case GDK_KP_0:
	case GDK_KP_1:
	case GDK_KP_2:
	case GDK_KP_3:
	case GDK_KP_4:
	case GDK_KP_5:
	case GDK_KP_6:
	case GDK_KP_7:
	case GDK_KP_8:
	case GDK_KP_9:
		return TRUE;
		break;
	default:
		break;
	}
	return FALSE;
}

static guint
keyval_keypad_nonkeypad_equivalent (guint keyval)
{
	switch (keyval) {
	case GDK_KP_F1:
		return GDK_F1;
	case GDK_KP_F2:
		return GDK_F2;
	case GDK_KP_F3:
		return GDK_F3;
	case GDK_KP_F4:
		return GDK_F4;
	case GDK_KP_Home:
		return GDK_Home;
	case GDK_KP_Left:
		return GDK_Left;
	case GDK_KP_Up:
		return GDK_Up;
	case GDK_KP_Right:
		return GDK_Right;
	case GDK_KP_Down:
		return GDK_Down;
	case GDK_KP_Page_Up:
		return GDK_Page_Up;
	case GDK_KP_Page_Down:
		return GDK_Page_Down;
	case GDK_KP_End:
		return GDK_End;
	case GDK_KP_Begin:
		return GDK_Begin;
	case GDK_KP_Insert:
		return GDK_Insert;
	case GDK_KP_Delete:
		return GDK_Delete;
	case GDK_KP_Equal:
		return GDK_equal;
	case GDK_KP_Multiply:
		return GDK_asterisk;
	case GDK_KP_Add:
		return GDK_plus;
	case GDK_KP_Subtract:
		return GDK_minus;
	case GDK_KP_Decimal:
		return GDK_period;
	case GDK_KP_Divide:
		return GDK_slash;
	case GDK_KP_0:
		return GDK_0;
	case GDK_KP_1:
		return GDK_1;
	case GDK_KP_2:
		return GDK_2;
	case GDK_KP_3:
		return GDK_3;
	case GDK_KP_4:
		return GDK_4;
	case GDK_KP_5:
		return GDK_5;
	case GDK_KP_6:
		return GDK_6;
	case GDK_KP_7:
		return GDK_7;
	case GDK_KP_8:
		return GDK_8;
	case GDK_KP_9:
		return GDK_9;
	default:
		break;
	}

	return GDK_VoidSymbol;
}

static const gchar* 
gdk_quartz_keyval_to_string (guint keyval)
{
	switch (keyval) {
	case GDK_space:
		return " ";
	case GDK_exclam:
		return "!";
	case GDK_quotedbl:
		return "\"";
	case GDK_numbersign:
		return "#";
	case GDK_dollar:
		return "$";
	case GDK_percent:
		return "%";
	case GDK_ampersand:
		return "&";
	case GDK_apostrophe:
		return "'";
	case GDK_parenleft:
		return "(";
	case GDK_parenright:
		return ")";
	case GDK_asterisk:
		return "*";
	case GDK_plus:
		return "+";
	case GDK_comma:
		return ",";
	case GDK_minus:
		return "-";
	case GDK_period:
		return ".";
	case GDK_slash:
		return "/";
	case GDK_0:
		return "0";
	case GDK_1:
		return "1";
	case GDK_2:
		return "2";
	case GDK_3:
		return "3";
	case GDK_4:
		return "4";
	case GDK_5:
		return "5";
	case GDK_6:
		return "6";
	case GDK_7:
		return "7";
	case GDK_8:
		return "8";
	case GDK_9:
		return "9";
	case GDK_colon:
		return ":";
	case GDK_semicolon:
		return ";";
	case GDK_less:
		return "<";
	case GDK_equal:
		return "=";
	case GDK_greater:
		return ">";
	case GDK_question:
		return "?";
	case GDK_at:
		return "@";
	case GDK_A:
	case GDK_a:
		return "a";
	case GDK_B:
	case GDK_b:
		return "b";
	case GDK_C:
	case GDK_c:
		return "c";
	case GDK_D:
	case GDK_d:
		return "d";
	case GDK_E:
	case GDK_e:
		return "e";
	case GDK_F:
	case GDK_f:
		return "f";
	case GDK_G:
	case GDK_g:
		return "g";
	case GDK_H:
	case GDK_h:
		return "h";
	case GDK_I:
	case GDK_i:
		return "i";
	case GDK_J:
	case GDK_j:
		return "j";
	case GDK_K:
	case GDK_k:
		return "k";
	case GDK_L:
	case GDK_l:
		return "l";
	case GDK_M:
	case GDK_m:
		return "m";
	case GDK_N:
	case GDK_n:
		return "n";
	case GDK_O:
	case GDK_o:
		return "o";
	case GDK_P:
	case GDK_p:
		return "p";
	case GDK_Q:
	case GDK_q:
		return "q";
	case GDK_R:
	case GDK_r:
		return "r";
	case GDK_S:
	case GDK_s:
		return "s";
	case GDK_T:
	case GDK_t:
		return "t";
	case GDK_U:
	case GDK_u:
		return "u";
	case GDK_V:
	case GDK_v:
		return "v";
	case GDK_W:
	case GDK_w:
		return "w";
	case GDK_X:
	case GDK_x:
		return "x";
	case GDK_Y:
	case GDK_y:
		return "y";
	case GDK_Z:
	case GDK_z:
		return "z";
	case GDK_bracketleft:
		return "[";
	case GDK_backslash:
		return "\\";
	case GDK_bracketright:
		return "]";
	case GDK_asciicircum:
		return "^";
	case GDK_underscore:
		return "_";
	case GDK_grave:
		return "`";
	case GDK_braceleft:
		return "{";
	case GDK_bar:
		return "|";
	case GDK_braceright:
		return "}";
	case GDK_asciitilde:
		return "~";
	default:
		break;
	}
	return NULL;
};

static gboolean
keyval_is_uppercase (guint keyval)
{
	switch (keyval) {
	case GDK_A:
	case GDK_B:
	case GDK_C:
	case GDK_D:
	case GDK_E:
	case GDK_F:
	case GDK_G:
	case GDK_H:
	case GDK_I:
	case GDK_J:
	case GDK_K:
	case GDK_L:
	case GDK_M:
	case GDK_N:
	case GDK_O:
	case GDK_P:
	case GDK_Q:
	case GDK_R:
	case GDK_S:
	case GDK_T:
	case GDK_U:
	case GDK_V:
	case GDK_W:
	case GDK_X:
	case GDK_Y:
	case GDK_Z:
		return TRUE;
	default:
		return FALSE;
	}
	return FALSE;
}

/* gtk/osx has a problem in that mac main menu events
   are handled using an "internal" event handling system that 
   doesn't pass things back to the glib/gtk main loop. if we call
   gtk_main_iteration() block while in a menu event handler, then
   glib gets confused and thinks there are two threads running
   g_main_poll_func(). apps call call gdk_quartz_in_menu_event_handler()
   if they need to check this.
 */

static int _in_menu_event_handler = 0;

int 
gdk_quartz_in_menu_event_handler ()
{
	return _in_menu_event_handler;
}

static gboolean
idle_call_activate (gpointer data)
{
	gtk_menu_item_activate ((GtkMenuItem*) data);
	return FALSE;
}

@interface GNSMenuItem : NSMenuItem
{
    @public
	GtkMenuItem* gtk_menu_item;
	GClosure      *accel_closure;
}
- (id) initWithTitle:(NSString*) title andGtkWidget:(GtkMenuItem*) w;
- (void) activate:(id) sender;
@end

@implementation GNSMenuItem
- (id) initWithTitle:(NSString*) title andGtkWidget:(GtkMenuItem*) w
{
	/* All menu items have the action "activate", which will be handled by this child class
	 */

	self = [ super initWithTitle:title action:@selector(activate:) keyEquivalent:@"" ];

	if (self) {
		/* make this handle its own action */
		[ self setTarget:self ];
		gtk_menu_item = w;
		accel_closure = 0;
	}
	return self;
}
- (void) activate:(id) sender
{
	UNUSED_PARAMETER(sender);
    // Hot Fix. Increase Priority.
	g_idle_add_full (G_PRIORITY_HIGH_IDLE, idle_call_activate, gtk_menu_item, NULL);
//    g_idle_add (idle_call_activate, gtk_menu_item);
}
@end

static void push_menu_shell_to_nsmenu (GtkMenuShell *menu_shell,
				       NSMenu       *menu,
				       gboolean      toplevel,
				       gboolean      debug);

/*
 * utility functions
 */

static GtkWidget *
find_menu_label (GtkWidget *widget)
{
  GtkWidget *label = NULL;

  if (GTK_IS_LABEL (widget))
    return widget;

  if (GTK_IS_CONTAINER (widget))
    {
      GList *children;
      GList *l;

      children = gtk_container_get_children (GTK_CONTAINER (widget));

      for (l = children; l; l = l->next)
	{
	  label = find_menu_label ((GtkWidget*) l->data);
	  if (label)
	    break;
	}

      g_list_free (children);
    }

  return label;
}

static const gchar *
get_menu_label_text (GtkWidget  *menu_item,
		     GtkWidget **label)
{
  GtkWidget *my_label;

  my_label = find_menu_label (menu_item);
  if (label)
    *label = my_label;

  if (my_label)
    return gtk_label_get_text (GTK_LABEL (my_label));

  return NULL;
}

static gboolean
accel_find_func (GtkAccelKey * /*key*/,
		 GClosure    *closure,
		 gpointer     data)
{
  return (GClosure *) data == closure;
}


/*
 * CocoaMenu functions
 */

static GQuark cocoa_menu_quark = 0;

static NSMenu *
cocoa_menu_get (GtkWidget *widget)
{
  return (NSMenu*) g_object_get_qdata (G_OBJECT (widget), cocoa_menu_quark);
}

static void
cocoa_menu_free (gpointer *ptr)
{
	NSMenu* menu = (NSMenu*) ptr;
	[menu release];
}

static void
cocoa_menu_connect (GtkWidget *menu,
		    NSMenu*    cocoa_menu)
{
	[cocoa_menu retain];

	if (cocoa_menu_quark == 0)
		cocoa_menu_quark = g_quark_from_static_string ("NSMenu");
	
	g_object_set_qdata_full (G_OBJECT (menu), cocoa_menu_quark,
				 cocoa_menu,
				 (GDestroyNotify) cocoa_menu_free);
}

/*
 * NSMenuItem functions
 */

static GQuark cocoa_menu_item_quark = 0;
static void cocoa_menu_item_connect (GtkWidget*   menu_item,
				     GNSMenuItem* cocoa_menu_item,
				     GtkWidget     *label);

static void
cocoa_menu_item_free (gpointer *ptr)
{
	GNSMenuItem* item = (GNSMenuItem*) ptr;
	[item release];
}

static GNSMenuItem *
cocoa_menu_item_get (GtkWidget *widget)
{
  return (GNSMenuItem*) g_object_get_qdata (G_OBJECT (widget), cocoa_menu_item_quark);
}

static void
cocoa_menu_item_update_state (NSMenuItem* cocoa_item,
			      GtkWidget      *widget)
{
  gboolean sensitive;
  gboolean visible;

  g_object_get (widget,
                "sensitive", &sensitive,
                "visible",   &visible,
                NULL);

  if (!sensitive)
	  [cocoa_item setEnabled:NO];
  else
	  [cocoa_item setEnabled:YES];

#if 0
  // requires OS X 10.5 or later
  if (!visible)
	  [cocoa_item setHidden:YES];
  else
	  [cocoa_item setHidden:NO];
#endif
}

static void
cocoa_menu_item_update_active (NSMenuItem *cocoa_item,
				GtkWidget  *widget)
{
  gboolean active;

  g_object_get (widget, "active", &active, NULL);

  if (active) 
    [cocoa_item setState:NSOnState];
  else
    [cocoa_item setState:NSOffState];
}

static void
cocoa_menu_item_update_submenu (NSMenuItem *cocoa_item,
				GtkWidget      *widget)
{
  GtkWidget *submenu;
  
  g_return_if_fail (cocoa_item != NULL);
  g_return_if_fail (widget != NULL);

  submenu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (widget));

  if (submenu)
    {
      GtkWidget* label = NULL;
      const gchar *label_text;
      NSMenu* cocoa_submenu;

      label_text = get_menu_label_text (widget, &label);

      /* create a new nsmenu to hold the GTK menu */

      if (label_text) 
	      cocoa_submenu = [ [ NSMenu alloc ] initWithTitle:[ [ NSString alloc] initWithCString:label_text encoding:NSUTF8StringEncoding]];
      else
	      cocoa_submenu = [ [ NSMenu alloc ] initWithTitle:@""];

      [cocoa_submenu setAutoenablesItems:NO];
      cocoa_menu_connect (submenu, cocoa_submenu);

      /* connect the new nsmenu to the passed-in item (which lives in
	 the parent nsmenu)
	 (Note: this will release any pre-existing version of this submenu)
      */
      [ cocoa_item setSubmenu:cocoa_submenu];

      /* and push the GTK menu into the submenu */
      push_menu_shell_to_nsmenu (GTK_MENU_SHELL (submenu), cocoa_submenu, FALSE, FALSE);

      [ cocoa_submenu release ];
    }
}

static void
cocoa_menu_item_update_label (NSMenuItem *cocoa_item,
			       GtkWidget      *widget)
{
  const gchar *label_text;

  g_return_if_fail (cocoa_item != NULL);
  g_return_if_fail (widget != NULL);

  label_text = get_menu_label_text (widget, NULL);
  if (label_text)
	  [cocoa_item setTitle:[ [ NSString alloc] initWithCString:label_text encoding:NSUTF8StringEncoding]];
  else 
	  [cocoa_item setTitle:@""];
}

static void
cocoa_menu_item_update_accelerator (NSMenuItem *cocoa_item,
				     GtkWidget *widget)
{
  GtkWidget *label;

  g_return_if_fail (cocoa_item != NULL);
  g_return_if_fail (widget != NULL);

  /* important note: this function doesn't do anything to actually change
     key handling. Its goal is to get Cocoa to display the correct
     accelerator as part of a menu item. Actual accelerator handling
     is still done by GTK, so this is more cosmetic than it may 
     appear.
  */

  get_menu_label_text (widget, &label);  

  if (GTK_IS_ACCEL_LABEL (label) &&
	    GTK_ACCEL_LABEL (label)->accel_closure)
	{
		GtkAccelKey *key;
		
		key = gtk_accel_group_find (GTK_ACCEL_LABEL (label)->accel_group,
					    accel_find_func,
				  GTK_ACCEL_LABEL (label)->accel_closure);
		
		if (key            &&
		    key->accel_key &&
		    key->accel_flags & GTK_ACCEL_VISIBLE)
		{
			guint modifiers = 0; 
			const gchar* str = NULL;
			guint actual_key = key->accel_key; 
			
			if (keyval_is_keypad (actual_key)) {
				if ((actual_key = keyval_keypad_nonkeypad_equivalent (actual_key)) == GDK_VoidSymbol) {
					/* GDK_KP_Separator */
					[cocoa_item setKeyEquivalent:@""];
					return;
				}
				modifiers |= NSNumericPadKeyMask;
			}
			
			/* if we somehow got here with GDK_A ... GDK_Z rather than GDK_a ... GDK_z, then take note
			   of that and make sure we use a shift modifier.
			*/
			
			if (keyval_is_uppercase (actual_key)) {
				modifiers |= NSShiftKeyMask;
			}
			
			str = gdk_quartz_keyval_to_string (actual_key);
			
			if (str) {
				unichar ukey = str[0];
				[cocoa_item setKeyEquivalent:[NSString stringWithCharacters:&ukey length:1]];
			} else {
				unichar ukey = gdk_quartz_keyval_to_ns_keyval (actual_key);
				if (ukey != 0) {
					[cocoa_item setKeyEquivalent:[NSString stringWithCharacters:&ukey length:1]];
				} else {
					/* cannot map this key to Cocoa key equivalent */
					[cocoa_item setKeyEquivalent:@""];
					return;
				}
			} 
			
			if (key->accel_mods || modifiers)
			{
				if (key->accel_mods & GDK_SHIFT_MASK) {
					modifiers |= NSShiftKeyMask;
				}
				
				/* gdk/quartz maps Alt/Option to Mod1 */
				
				if (key->accel_mods & (GDK_MOD1_MASK)) {
					modifiers |= NSAlternateKeyMask;
				}
				
				if (key->accel_mods & GDK_CONTROL_MASK) {
					modifiers |= NSControlKeyMask;
				}
				
				/* our modified gdk/quartz maps Command to Mod2 */
				
				if (key->accel_mods & GDK_MOD2_MASK) {
					modifiers |= NSCommandKeyMask;
				}
			}  
			
			[cocoa_item setKeyEquivalentModifierMask:modifiers];
			return;
		}
	}

	/*  otherwise, clear the menu shortcut  */
	[cocoa_item setKeyEquivalent:@""];
}

static void
cocoa_menu_item_accel_changed (GtkAccelGroup*   /*accel_group*/,
			       guint            /*keyval*/,
				GdkModifierType /*modifier*/,
				GClosure        *accel_closure,
				GtkWidget       *widget)
{
  GNSMenuItem *cocoa_item;
  GtkWidget      *label;

  if (_exiting) 
    return;

  cocoa_item = cocoa_menu_item_get (widget);
  get_menu_label_text (widget, &label);

  if (GTK_IS_ACCEL_LABEL (label) &&
      GTK_ACCEL_LABEL (label)->accel_closure == accel_closure)
    cocoa_menu_item_update_accelerator (cocoa_item, widget);
}

static void
cocoa_menu_item_update_accel_closure (GNSMenuItem *cocoa_item,
				      GtkWidget      *widget)
{
  GtkAccelGroup *group;
  GtkWidget     *label;

  get_menu_label_text (widget, &label);

  if (cocoa_item->accel_closure)
    {
      group = gtk_accel_group_from_accel_closure (cocoa_item->accel_closure);

      g_signal_handlers_disconnect_by_func (group,
					    (void*) cocoa_menu_item_accel_changed,
					    widget);

      g_closure_unref (cocoa_item->accel_closure);
      cocoa_item->accel_closure = NULL;
    }

  if (GTK_IS_ACCEL_LABEL (label)) {
    cocoa_item->accel_closure = GTK_ACCEL_LABEL (label)->accel_closure;
  }

  if (cocoa_item->accel_closure)
    {
      g_closure_ref (cocoa_item->accel_closure);

      group = gtk_accel_group_from_accel_closure (cocoa_item->accel_closure);

      g_signal_connect_object (group, "accel-changed",
			       G_CALLBACK (cocoa_menu_item_accel_changed),
			       widget, (GConnectFlags) 0);
    }

  cocoa_menu_item_update_accelerator (cocoa_item, widget);
}

static void
cocoa_menu_item_notify_label (GObject    *object,
			      GParamSpec *pspec,
			      gpointer)
{
  GNSMenuItem *cocoa_item;

  if (_exiting) 
    return;

  cocoa_item = cocoa_menu_item_get (GTK_WIDGET (object));

  if (!strcmp (pspec->name, "label"))
    {
      cocoa_menu_item_update_label (cocoa_item,
				     GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "accel-closure"))
    {
      cocoa_menu_item_update_accel_closure (cocoa_item,
					     GTK_WIDGET (object));
    }
}

static void
cocoa_menu_item_notify (GObject        *object,
			GParamSpec     *pspec,
			NSMenuItem *cocoa_item)
{
  if (_exiting)
    return;

  if (!strcmp (pspec->name, "sensitive") ||
      !strcmp (pspec->name, "visible"))
    {
      cocoa_menu_item_update_state (cocoa_item, GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "active"))
    {
      cocoa_menu_item_update_active (cocoa_item, GTK_WIDGET (object));
    }
  else if (!strcmp (pspec->name, "submenu"))
    {
      cocoa_menu_item_update_submenu (cocoa_item, GTK_WIDGET (object));
    }
}

static void
cocoa_menu_item_connect (GtkWidget*   menu_item,
			 GNSMenuItem* cocoa_item,
			 GtkWidget     *label)
{
	GNSMenuItem* old_item = cocoa_menu_item_get (menu_item);

	[cocoa_item retain];

	if (cocoa_menu_item_quark == 0)
		cocoa_menu_item_quark = g_quark_from_static_string ("NSMenuItem");

	g_object_set_qdata_full (G_OBJECT (menu_item), cocoa_menu_item_quark,
				 cocoa_item,
				 (GDestroyNotify) cocoa_menu_item_free);
	
	if (!old_item) {

		g_signal_connect (menu_item, "notify",
				  G_CALLBACK (cocoa_menu_item_notify),
				  cocoa_item);
		
		if (label)
			g_signal_connect_swapped (label, "notify::label",
						  G_CALLBACK (cocoa_menu_item_notify_label),
						  menu_item);
	}
}

static void
add_menu_item (NSMenu* cocoa_menu, GtkWidget* menu_item, int index)
{
	GtkWidget* label      = NULL;
	GNSMenuItem *cocoa_item;
	
	DEBUG ("add %s to menu %s separator ? %d\n", get_menu_label_text (menu_item, NULL), 
	       [[cocoa_menu title] cStringUsingEncoding:NSUTF8StringEncoding],
	       GTK_IS_SEPARATOR_MENU_ITEM(menu_item));

	cocoa_item = cocoa_menu_item_get (menu_item);

	if (cocoa_item) 
		return;

	if (GTK_IS_SEPARATOR_MENU_ITEM (menu_item)) {
		cocoa_item = [NSMenuItem separatorItem];
		DEBUG ("\ta separator\n");
	} else {

		if (!GTK_WIDGET_VISIBLE (menu_item)) {
			DEBUG ("\tnot visible\n");
			return;
		}

		const gchar* label_text = get_menu_label_text (menu_item, &label);
		
		if (label_text)
			cocoa_item = [ [ GNSMenuItem alloc] initWithTitle:[ [ NSString alloc] initWithCString:label_text encoding:NSUTF8StringEncoding]
				       andGtkWidget:(GtkMenuItem*)menu_item];
		else
			cocoa_item = [ [ GNSMenuItem alloc] initWithTitle:@"" andGtkWidget:(GtkMenuItem*)menu_item];
		DEBUG ("\tan item\n");
	}
	
	/* connect GtkMenuItem and NSMenuItem so that we can notice changes to accel/label/submenu etc. */
	cocoa_menu_item_connect (menu_item, (GNSMenuItem*) cocoa_item, label);
	
	[ cocoa_item setEnabled:YES];
	if (index >= 0) 
		[ cocoa_menu insertItem:cocoa_item atIndex:index];
	else 
		[ cocoa_menu addItem:cocoa_item];
	
	if (!GTK_WIDGET_IS_SENSITIVE (menu_item))
		[cocoa_item setState:NSOffState];

#if MAC_OS_X_VERSION_MIN_REQUIRED > MAC_OS_X_VERSION_10_4
	if (!GTK_WIDGET_VISIBLE (menu_item))
		[cocoa_item setHidden:YES];
#endif
	
	if (GTK_IS_CHECK_MENU_ITEM (menu_item))
		cocoa_menu_item_update_active (cocoa_item, menu_item);
	
	if (!GTK_IS_SEPARATOR_MENU_ITEM (menu_item))
		cocoa_menu_item_update_accel_closure (cocoa_item, menu_item);
	
	if (gtk_menu_item_get_submenu (GTK_MENU_ITEM (menu_item))) 
		cocoa_menu_item_update_submenu (cocoa_item, menu_item);

	[ cocoa_item release];
}
	
static void
push_menu_shell_to_nsmenu (GtkMenuShell *menu_shell,
			   NSMenu*       cocoa_menu,
			   gboolean      /*toplevel*/,
			   gboolean      /*debug*/)
{
  GList         *children;
  GList         *l;

  children = gtk_container_get_children (GTK_CONTAINER (menu_shell));

  for (l = children; l; l = l->next)
    {
      GtkWidget   *menu_item = (GtkWidget*) l->data;

      if (GTK_IS_TEAROFF_MENU_ITEM (menu_item))
	continue;

      if (g_object_get_data (G_OBJECT (menu_item), "gtk-empty-menu-item"))
	continue;

      add_menu_item (cocoa_menu, menu_item, -1);
    }
  
  g_list_free (children);
}


static gulong emission_hook_id = 0;

static gboolean
parent_set_emission_hook (GSignalInvocationHint* /*ihint*/,
			  guint                  /*n_param_values*/,
			  const GValue*          param_values,
			  gpointer               data)
{
  GtkWidget *instance = (GtkWidget*) g_value_get_object (param_values);

  if (GTK_IS_MENU_ITEM (instance))
    {
      GtkWidget *previous_parent = (GtkWidget*) g_value_get_object (param_values + 1);
      GtkWidget *menu_shell      = NULL;

      if (GTK_IS_MENU_SHELL (previous_parent))
	{
	  menu_shell = previous_parent;
        }
      else if (GTK_IS_MENU_SHELL (instance->parent))
	{
	  menu_shell = instance->parent;
	}

      if (menu_shell)
        {
	  NSMenu *cocoa_menu = cocoa_menu_get (menu_shell);

	  if (cocoa_menu)
	    {
	      push_menu_shell_to_nsmenu (GTK_MENU_SHELL (menu_shell),
					 cocoa_menu,
					 cocoa_menu == (NSMenu*) data,
					 FALSE);
	    }
        }
    }

  return TRUE;
}

static void
parent_set_emission_hook_remove (GtkWidget*, gpointer)
{
	g_signal_remove_emission_hook (g_signal_lookup ("parent-set", GTK_TYPE_WIDGET),
				       emission_hook_id);
}

/* Building "standard" Cocoa/OS X menus */

#warning You can safely ignore the next warning about a duplicate interface definition
@interface NSApplication(NSWindowsMenu)
	- (void)setAppleMenu:(NSMenu *)aMenu;
@end

static NSMenu* _main_menubar = 0;
static NSMenu* _window_menu = 0;
static NSMenu* _app_menu = 0;

static int
add_to_menubar (NSMenu *menu)
{
	NSMenuItem *dummyItem = [[NSMenuItem alloc] initWithTitle:@""
				 action:nil keyEquivalent:@""];
	[dummyItem setSubmenu:menu];
	[_main_menubar addItem:dummyItem];
	[dummyItem release];
	return 0;
}

#if 0
static int
add_to_app_menu (NSMenu *menu)
{
	NSMenuItem *dummyItem = [[NSMenuItem alloc] initWithTitle:@""
				 action:nil keyEquivalent:@""];
	[dummyItem setSubmenu:menu];
	[_app_menu addItem:dummyItem];
	[dummyItem release];
	return 0;
}
#endif

static int
create_apple_menu ()
{
	NSMenuItem *menuitem;
	// Create the application (Apple) menu.
	_app_menu = [[NSMenu alloc] initWithTitle: @"Apple Menu"];

	NSMenu *menuServices = [[NSMenu alloc] initWithTitle: @"Services"];
	[NSApp setServicesMenu:menuServices];

	[_app_menu addItem: [NSMenuItem separatorItem]];
	menuitem = [[NSMenuItem alloc] initWithTitle: @"Services"
		    action:nil keyEquivalent:@""];
	[menuitem setSubmenu:menuServices];
	[_app_menu addItem: menuitem];
	[menuitem release];
	[_app_menu addItem: [NSMenuItem separatorItem]];
	menuitem = [[NSMenuItem alloc] initWithTitle:@"Hide"
		    action:@selector(hide:) keyEquivalent:@"h"];
	[menuitem setTarget: NSApp];
	[_app_menu addItem: menuitem];
	[menuitem release];
	menuitem = [[NSMenuItem alloc] initWithTitle:@"Hide Others"
		    action:@selector(hideOtherApplications:) keyEquivalent:@""];
	[menuitem setTarget: NSApp];
	[_app_menu addItem: menuitem];
	[menuitem release];
	menuitem = [[NSMenuItem alloc] initWithTitle:@"Show All"
		    action:@selector(unhideAllApplications:) keyEquivalent:@""];
	[menuitem setTarget: NSApp];
	[_app_menu addItem: menuitem];
	[menuitem release];
	[_app_menu addItem: [NSMenuItem separatorItem]];
	menuitem = [[NSMenuItem alloc] initWithTitle:@"Quit"
		    action:@selector(terminate:) keyEquivalent:@"q"];
	[menuitem setTarget: NSApp];
	[_app_menu addItem: menuitem];
	[menuitem release];

	[NSApp setAppleMenu:_app_menu];
	add_to_menubar (_app_menu);

	return 0;
}

#if 0
static int
add_to_window_menu (NSMenu *menu)
{
	NSMenuItem *dummyItem = [[NSMenuItem alloc] initWithTitle:@""
				 action:nil keyEquivalent:@""];
	[dummyItem setSubmenu:menu];
	[_window_menu addItem:dummyItem];
	[dummyItem release];
	return 0;
}

static int
create_window_menu ()
{   
	_window_menu = [[NSMenu alloc] initWithTitle: @"Window"];

	[_window_menu addItemWithTitle:@"Minimize"
	 action:@selector(performMiniaturize:) keyEquivalent:@""];
	[_window_menu addItem: [NSMenuItem separatorItem]];
	[_window_menu addItemWithTitle:@"Bring All to Front"
	 action:@selector(arrangeInFront:) keyEquivalent:@""];

	[NSApp setWindowsMenu:_window_menu];
	add_to_menubar(_window_menu);

	return 0;
}  
#endif

/*
 * public functions
 */

extern "C" void
gtk_application_set_menu_bar (GtkMenuShell *menu_shell)
{
  NSMenu* cocoa_menubar;

  g_return_if_fail (GTK_IS_MENU_SHELL (menu_shell));

  if (cocoa_menu_quark == 0)
    cocoa_menu_quark = g_quark_from_static_string ("NSMenu");

  if (cocoa_menu_item_quark == 0)
    cocoa_menu_item_quark = g_quark_from_static_string ("NSMenuItem");

  cocoa_menubar = [ [ NSApplication sharedApplication] mainMenu];

  /* turn off auto-enabling for the menu - its silly and slow and
     doesn't really make sense for a Gtk/Cocoa hybrid menu.
   */

  [cocoa_menubar setAutoenablesItems:NO];

  emission_hook_id =
    g_signal_add_emission_hook (g_signal_lookup ("parent-set",
						 GTK_TYPE_WIDGET),
				0,
				parent_set_emission_hook,
				cocoa_menubar, NULL);


  g_signal_connect (menu_shell, "destroy",
		    G_CALLBACK (parent_set_emission_hook_remove),
		    NULL);

  push_menu_shell_to_nsmenu (menu_shell, cocoa_menubar, TRUE, FALSE);
}

extern "C" void
gtk_application_add_app_menu_item (GtkApplicationMenuGroup *group,
				   GtkMenuItem     *menu_item)
{
  // we know that the application menu is always the submenu of the first item in the main menu
  NSMenu* mainMenu;
  NSMenu *appMenu;
  NSMenuItem *firstItem;
  GList   *list;
  gint     index = 0;

  mainMenu = [NSApp mainMenu];
  firstItem = [ mainMenu itemAtIndex:0];
  appMenu = [ firstItem submenu ];

  g_return_if_fail (group != NULL);
  g_return_if_fail (GTK_IS_MENU_ITEM (menu_item));

  for (list = _gtk_application_menu_groups; list; list = g_list_next (list))
    {
      GtkApplicationMenuGroup *list_group = (GtkApplicationMenuGroup*) list->data;

      index += g_list_length (list_group->items);

      /*  adjust index for the separator between groups, but not
       *  before the first group
       */
      if (list_group->items && list->prev)
	index++;

      if (group == list_group)
	{
		/*  add a separator before adding the first item, but not
		 *  for the first group
		 */
		
		if (!group->items && list->prev)
		   {
			   [appMenu insertItem:[NSMenuItem separatorItem] atIndex:index+1];
			   index++;
		   }
		DEBUG ("Add to APP menu bar %s\n", get_menu_label_text (GTK_WIDGET(menu_item), NULL));
		add_menu_item (appMenu, GTK_WIDGET(menu_item), index+1);

		group->items = g_list_append (group->items, menu_item);
		gtk_widget_hide (GTK_WIDGET (menu_item));
		return;
	}
    }

  if (!list)
    g_warning ("%s: app menu group %p does not exist",
	       G_STRFUNC, group);
}

/* application delegate, currently in C++ */

#include <gtkmm2ext/application.h>
#include <glibmm/ustring.h>

namespace Gtk {
	namespace Application {
		sigc::signal<void,bool> ActivationChanged;
		sigc::signal<void,const Glib::ustring&> ShouldLoad;
		sigc::signal<void> ShouldQuit;
	}
}

@interface GtkApplicationNotificationObject : NSObject {}
- (GtkApplicationNotificationObject*) init; 
@end

@implementation GtkApplicationNotificationObject
- (GtkApplicationNotificationObject*) init
{
	self = [ super init ];

	if (self) {
		[[NSNotificationCenter defaultCenter] addObserver:self
		 selector:@selector(appDidBecomeActive:)
		 name:NSApplicationDidBecomeActiveNotification
		 object:[NSApplication sharedApplication]];

		[[NSNotificationCenter defaultCenter] addObserver:self
		 selector:@selector(appDidBecomeInactive:)
		 name:NSApplicationWillResignActiveNotification 
		 object:[NSApplication sharedApplication]];
	}

	return self;
}

- (void)appDidBecomeActive:(NSNotification *) notification
{
	UNUSED_PARAMETER(notification);
	Gtkmm2ext::Application::instance()->ActivationChanged (true);
}

- (void)appDidBecomeInactive:(NSNotification *) notification
{
	UNUSED_PARAMETER(notification);
	Gtkmm2ext::Application::instance()->ActivationChanged (false);
}

@end

@interface GtkApplicationDelegate : NSObject
-(BOOL) application:(NSApplication*) app openFile:(NSString*) file;
- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication *) app;
- (void) startApp;
@end

@implementation GtkApplicationDelegate
-(BOOL) application:(NSApplication*) app openFile:(NSString*) file
{
	UNUSED_PARAMETER(app);
	Glib::ustring utf8_path ([file UTF8String]);
	Gtkmm2ext::Application::instance()->ShouldLoad (utf8_path);
	return 1;
}
- (NSApplicationTerminateReply) applicationShouldTerminate:(NSApplication *) app
{
	UNUSED_PARAMETER(app);
	Gtkmm2ext::Application::instance()->ShouldQuit ();
	return NSTerminateCancel;
}
@end


/* Basic setup */

extern "C" int
gtk_application_init ()
{
	_main_menubar = [[NSMenu alloc] initWithTitle: @""];

	if (!_main_menubar) 
		return -1;

	[NSApp setMainMenu: _main_menubar];
	create_apple_menu ();
	// create_window_menu ();

	/* this will stick around for ever ... is that OK ? */

	[ [GtkApplicationNotificationObject alloc] init];
	[ NSApp setDelegate: [GtkApplicationDelegate new]];

	return 0;
}

extern "C" void
gtk_application_ready ()
{
	[ NSApp finishLaunching ];
        [[NSApplication sharedApplication] activateIgnoringOtherApps : YES];
}

extern "C" void
gtk_application_hide ()
{
    [NSApp performSelector:@selector(hide:)];
}

extern "C" void
gtk_application_cleanup()
{
	_exiting = 1;

	if (_window_menu) {
		[ _window_menu release ];
		_window_menu = 0;
	}
	if (_app_menu) {
		[ _app_menu release ];
	        _app_menu = 0;
	}
	if (_main_menubar) {
		[ _main_menubar release ];
	        _main_menubar = 0;
	}
}
