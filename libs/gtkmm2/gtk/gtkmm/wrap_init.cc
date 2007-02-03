
#include <glib.h>

// Disable the 'const' function attribute of the get_type() functions.
// GCC would optimize them out because we don't use the return value.
#undef  G_GNUC_CONST
#define G_GNUC_CONST /* empty */

#include <gtkmm/wrap_init.h>
#include <glibmm/error.h>
#include <glibmm/object.h>

// #include the widget headers so that we can call the get_type() static methods:

#include "pagesetupunixdialog.h"
#include "printunixdialog.h"
#include "printer.h"
#include "printjob.h"
#include "aboutdialog.h"
#include "accelgroup.h"
#include "accellabel.h"
#include "action.h"
#include "actiongroup.h"
#include "adjustment.h"
#include "alignment.h"
#include "arrow.h"
#include "aspectframe.h"
#include "assistant.h"
#include "bin.h"
#include "box.h"
#include "button.h"
#include "buttonbox.h"
#include "calendar.h"
#include "celleditable.h"
#include "celllayout.h"
#include "cellview.h"
#include "cellrenderer.h"
#include "cellrendereraccel.h"
#include "cellrenderercombo.h"
#include "cellrendererpixbuf.h"
#include "cellrendererprogress.h"
#include "cellrendererspin.h"
#include "cellrenderertext.h"
#include "cellrenderertoggle.h"
#include "checkbutton.h"
#include "checkmenuitem.h"
#include "clipboard.h"
#include "colorselection.h"
#include "colorbutton.h"
#include "combobox.h"
#include "comboboxentry.h"
#include "container.h"
#include "curve.h"
#include "dialog.h"
#include "drawingarea.h"
#include "editable.h"
#include "entry.h"
#include "entrycompletion.h"
#include "enums.h"
#include "eventbox.h"
#include "expander.h"
#include "filechooser.h"
#include "filechooserbutton.h"
#include "filechooserwidget.h"
#include "filechooserdialog.h"
#include "filefilter.h"
#include "fileselection.h"
#include "fixed.h"
#include "fontbutton.h"
#include "fontselection.h"
#include "frame.h"
#include "handlebox.h"
#include "iconfactory.h"
#include "iconset.h"
#include "iconsource.h"
#include "iconinfo.h"
#include "icontheme.h"
#include "iconview.h"
#include "image.h"
#include "imagemenuitem.h"
#include "inputdialog.h"
#include "invisible.h"
#include "item.h"
#include "label.h"
#include "layout.h"
#include "linkbutton.h"
#include "liststore.h"
#include "main.h"
#include "menu.h"
#include "menubar.h"
#include "menuitem.h"
#include "menushell.h"
#include "menutoolbutton.h"
#include "messagedialog.h"
#include "misc.h"
#include "notebook.h"
#include "object.h"
#include "optionmenu.h"
#include "paned.h"
#include "plug.h"
#include "progressbar.h"
#include "papersize.h"
#include "pagesetup.h"
#include "printsettings.h"
#include "printcontext.h"
#include "printoperation.h"
#include "printoperationpreview.h"
#include "radioaction.h"
#include "radiobutton.h"
#include "radiomenuitem.h"
#include "radiotoolbutton.h"
#include "range.h"
#include "rc.h"
#include "recentchooser.h"
#include "recentchooserdialog.h"
#include "recentchoosermenu.h"
#include "recentchooserwidget.h"
#include "recentfilter.h"
#include "recentinfo.h"
#include "recentmanager.h"
#include "ruler.h"
#include "scale.h"
#include "scrollbar.h"
#include "scrolledwindow.h"
#include "selectiondata.h"
#include "separator.h"
#include "separatormenuitem.h"
#include "separatortoolitem.h"
#include "settings.h"
#include "sizegroup.h"
#include "socket.h"
#include "spinbutton.h"
#include "statusbar.h"
#include "statusicon.h"
#include "stockitem.h"
#include "style.h"
#include "table.h"
#include "targetlist.h"
#include "tearoffmenuitem.h"
#include "textattributes.h"
#include "textbuffer.h"
#include "textchildanchor.h"
#include "textiter.h"
#include "textmark.h"
#include "texttag.h"
#include "texttagtable.h"
#include "textview.h"
#include "toggleaction.h"
#include "togglebutton.h"
#include "toggletoolbutton.h"
#include "toolbar.h"
#include "toolitem.h"
#include "toolbutton.h"
#include "tooltips.h"
#include "treedragdest.h"
#include "treedragsource.h"
#include "treepath.h"
#include "treerowreference.h"
#include "treeselection.h"
#include "treesortable.h"
#include "treeiter.h"
#include "treemodel.h"
#include "treemodelfilter.h"
#include "treemodelsort.h"
#include "treestore.h"
#include "treeview.h"
#include "treeviewcolumn.h"
#include "viewport.h"
#include "uimanager.h"
#include "widget.h"
#include "window.h"
#include "combo.h"

extern "C"
{

//Declarations of the *_get_type() functions:

GType gtk_about_dialog_get_type(void);
GType gtk_accel_group_get_type(void);
GType gtk_accel_label_get_type(void);
GType gtk_action_get_type(void);
GType gtk_action_group_get_type(void);
GType gtk_adjustment_get_type(void);
GType gtk_alignment_get_type(void);
GType gtk_arrow_get_type(void);
GType gtk_aspect_frame_get_type(void);
GType gtk_assistant_get_type(void);
GType gtk_bin_get_type(void);
GType gtk_box_get_type(void);
GType gtk_button_get_type(void);
GType gtk_button_box_get_type(void);
GType gtk_calendar_get_type(void);
GType gtk_cell_renderer_get_type(void);
GType gtk_cell_renderer_accel_get_type(void);
GType gtk_cell_renderer_combo_get_type(void);
GType gtk_cell_renderer_pixbuf_get_type(void);
GType gtk_cell_renderer_progress_get_type(void);
GType gtk_cell_renderer_spin_get_type(void);
GType gtk_cell_renderer_text_get_type(void);
GType gtk_cell_renderer_toggle_get_type(void);
GType gtk_cell_view_get_type(void);
GType gtk_check_button_get_type(void);
GType gtk_check_menu_item_get_type(void);
GType gtk_clipboard_get_type(void);
GType gtk_color_button_get_type(void);
GType gtk_color_selection_get_type(void);
GType gtk_color_selection_dialog_get_type(void);
#ifndef GTKMM_DISABLE_DEPRECATED
GType gtk_combo_get_type(void);
#endif // *_DISABLE_DEPRECATED
GType gtk_combo_box_get_type(void);
GType gtk_combo_box_entry_get_type(void);
#ifndef GTKMM_DISABLE_DEPRECATED
GType gtk_list_get_type(void);
#endif // *_DISABLE_DEPRECATED
#ifndef GTKMM_DISABLE_DEPRECATED
GType gtk_list_item_get_type(void);
#endif // *_DISABLE_DEPRECATED
GType gtk_container_get_type(void);
GType gtk_curve_get_type(void);
GType gtk_dialog_get_type(void);
GType gtk_drawing_area_get_type(void);
GType gtk_entry_get_type(void);
GType gtk_entry_completion_get_type(void);
GType gtk_event_box_get_type(void);
GType gtk_expander_get_type(void);
GType gtk_file_chooser_button_get_type(void);
GType gtk_file_chooser_dialog_get_type(void);
GType gtk_file_chooser_widget_get_type(void);
GType gtk_file_filter_get_type(void);
#ifndef GTKMM_DISABLE_DEPRECATED
GType gtk_file_selection_get_type(void);
#endif // *_DISABLE_DEPRECATED
GType gtk_fixed_get_type(void);
GType gtk_font_button_get_type(void);
GType gtk_font_selection_get_type(void);
GType gtk_font_selection_dialog_get_type(void);
GType gtk_frame_get_type(void);
GType gtk_gamma_curve_get_type(void);
GType gtk_hbox_get_type(void);
GType gtk_hbutton_box_get_type(void);
GType gtk_hpaned_get_type(void);
GType gtk_hruler_get_type(void);
GType gtk_hscale_get_type(void);
GType gtk_hscrollbar_get_type(void);
GType gtk_hseparator_get_type(void);
GType gtk_handle_box_get_type(void);
GType gtk_icon_factory_get_type(void);
GType gtk_icon_theme_get_type(void);
GType gtk_icon_view_get_type(void);
GType gtk_image_get_type(void);
GType gtk_image_menu_item_get_type(void);
GType gtk_input_dialog_get_type(void);
GType gtk_invisible_get_type(void);
GType gtk_item_get_type(void);
GType gtk_label_get_type(void);
GType gtk_layout_get_type(void);
GType gtk_link_button_get_type(void);
GType gtk_list_store_get_type(void);
GType gtk_menu_get_type(void);
GType gtk_menu_bar_get_type(void);
GType gtk_menu_item_get_type(void);
GType gtk_menu_shell_get_type(void);
GType gtk_menu_tool_button_get_type(void);
GType gtk_message_dialog_get_type(void);
GType gtk_misc_get_type(void);
GType gtk_notebook_get_type(void);
GType gtk_object_get_type(void);
#ifndef GTKMM_DISABLE_DEPRECATED
GType gtk_option_menu_get_type(void);
#endif // *_DISABLE_DEPRECATED
GType gtk_page_setup_get_type(void);
#ifndef G_OS_WIN32
GType gtk_page_setup_unix_dialog_get_type(void);
#endif //G_OS_WIN32
GType gtk_paned_get_type(void);
#ifndef G_OS_WIN32
GType gtk_plug_get_type(void);
#endif //G_OS_WIN32
GType gtk_print_context_get_type(void);
#ifndef G_OS_WIN32
GType gtk_print_job_get_type(void);
#endif //G_OS_WIN32
GType gtk_print_operation_get_type(void);
GType gtk_print_settings_get_type(void);
#ifndef G_OS_WIN32
GType gtk_print_unix_dialog_get_type(void);
#endif //G_OS_WIN32
#ifndef G_OS_WIN32
GType gtk_printer_get_type(void);
#endif //G_OS_WIN32
GType gtk_progress_bar_get_type(void);
GType gtk_radio_action_get_type(void);
GType gtk_radio_button_get_type(void);
GType gtk_radio_menu_item_get_type(void);
GType gtk_radio_tool_button_get_type(void);
GType gtk_range_get_type(void);
GType gtk_rc_style_get_type(void);
GType gtk_recent_chooser_dialog_get_type(void);
GType gtk_recent_chooser_menu_get_type(void);
GType gtk_recent_chooser_widget_get_type(void);
GType gtk_recent_filter_get_type(void);
GType gtk_recent_manager_get_type(void);
GType gtk_ruler_get_type(void);
GType gtk_scale_get_type(void);
GType gtk_scrollbar_get_type(void);
GType gtk_scrolled_window_get_type(void);
GType gtk_separator_get_type(void);
GType gtk_separator_menu_item_get_type(void);
GType gtk_separator_tool_item_get_type(void);
GType gtk_settings_get_type(void);
GType gtk_size_group_get_type(void);
#ifndef G_OS_WIN32
GType gtk_socket_get_type(void);
#endif //G_OS_WIN32
GType gtk_spin_button_get_type(void);
GType gtk_status_icon_get_type(void);
GType gtk_statusbar_get_type(void);
GType gtk_style_get_type(void);
GType gtk_table_get_type(void);
GType gtk_tearoff_menu_item_get_type(void);
GType gtk_text_buffer_get_type(void);
GType gtk_text_child_anchor_get_type(void);
GType gtk_text_mark_get_type(void);
GType gtk_text_tag_get_type(void);
GType gtk_text_tag_table_get_type(void);
GType gtk_text_view_get_type(void);
GType gtk_toggle_action_get_type(void);
GType gtk_toggle_button_get_type(void);
GType gtk_toggle_tool_button_get_type(void);
GType gtk_tool_button_get_type(void);
GType gtk_tool_item_get_type(void);
GType gtk_toolbar_get_type(void);
GType gtk_tooltips_get_type(void);
GType gtk_tree_model_filter_get_type(void);
GType gtk_tree_model_sort_get_type(void);
GType gtk_tree_selection_get_type(void);
GType gtk_tree_store_get_type(void);
GType gtk_tree_view_get_type(void);
GType gtk_tree_view_column_get_type(void);
GType gtk_ui_manager_get_type(void);
GType gtk_vbox_get_type(void);
GType gtk_vbutton_box_get_type(void);
GType gtk_vpaned_get_type(void);
GType gtk_vruler_get_type(void);
GType gtk_vscale_get_type(void);
GType gtk_vscrollbar_get_type(void);
GType gtk_vseparator_get_type(void);
GType gtk_viewport_get_type(void);
GType gtk_widget_get_type(void);
GType gtk_window_get_type(void);
GType gtk_window_group_get_type(void);

//Declarations of the *_error_quark() functions:

GQuark gtk_file_chooser_error_quark(void);
GQuark gtk_icon_theme_error_quark(void);
GQuark gtk_print_error_quark(void);
GQuark gtk_recent_chooser_error_quark(void);
GQuark gtk_recent_manager_error_quark(void);
} // extern "C"


//Declarations of the *_Class::wrap_new() methods, instead of including all the private headers:

namespace Gtk {  class AboutDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class AccelGroup_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class AccelLabel_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Action_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ActionGroup_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Adjustment_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Alignment_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Arrow_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class AspectFrame_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Assistant_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Bin_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Box_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Button_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ButtonBox_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Calendar_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellRenderer_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellRendererAccel_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellRendererCombo_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellRendererPixbuf_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellRendererProgress_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellRendererSpin_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellRendererText_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellRendererToggle_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CellView_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CheckButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class CheckMenuItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Clipboard_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ColorButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ColorSelection_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ColorSelectionDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef GTKMM_DISABLE_DEPRECATED
namespace Gtk {  class Combo_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif // *_DISABLE_DEPRECATED
namespace Gtk {  class ComboBox_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ComboBoxEntry_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef GTKMM_DISABLE_DEPRECATED
namespace Gtk {  class ComboDropDown_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif // *_DISABLE_DEPRECATED
#ifndef GTKMM_DISABLE_DEPRECATED
namespace Gtk {  class ComboDropDownItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif // *_DISABLE_DEPRECATED
namespace Gtk {  class Container_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Curve_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Dialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class DrawingArea_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Entry_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class EntryCompletion_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class EventBox_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Expander_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class FileChooserButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class FileChooserDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class FileChooserWidget_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class FileFilter_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef GTKMM_DISABLE_DEPRECATED
namespace Gtk {  class FileSelection_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif // *_DISABLE_DEPRECATED
namespace Gtk {  class Fixed_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class FontButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class FontSelection_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class FontSelectionDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Frame_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class GammaCurve_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class HBox_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class HButtonBox_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class HPaned_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class HRuler_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class HScale_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class HScrollbar_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class HSeparator_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class HandleBox_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class IconFactory_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class IconTheme_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class IconView_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Image_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ImageMenuItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class InputDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Invisible_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Item_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Label_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Layout_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class LinkButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ListStore_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Menu_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class MenuBar_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class MenuItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class MenuShell_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class MenuToolButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class MessageDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Misc_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Notebook_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Object_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef GTKMM_DISABLE_DEPRECATED
namespace Gtk {  class OptionMenu_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif // *_DISABLE_DEPRECATED
namespace Gtk {  class PageSetup_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef G_OS_WIN32
namespace Gtk {  class PageSetupUnixDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
namespace Gtk {  class Paned_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef G_OS_WIN32
namespace Gtk {  class Plug_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
namespace Gtk {  class PrintContext_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef G_OS_WIN32
namespace Gtk {  class PrintJob_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
namespace Gtk {  class PrintOperation_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class PrintSettings_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef G_OS_WIN32
namespace Gtk {  class PrintUnixDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
#ifndef G_OS_WIN32
namespace Gtk {  class Printer_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
namespace Gtk {  class ProgressBar_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RadioAction_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RadioButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RadioMenuItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RadioToolButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Range_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RcStyle_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RecentChooserDialog_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RecentChooserMenu_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RecentChooserWidget_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RecentFilter_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class RecentManager_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Ruler_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Scale_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Scrollbar_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ScrolledWindow_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Separator_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class SeparatorMenuItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class SeparatorToolItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Settings_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class SizeGroup_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#ifndef G_OS_WIN32
namespace Gtk {  class Socket_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
#endif //G_OS_WIN32
namespace Gtk {  class SpinButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class StatusIcon_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Statusbar_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Style_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Table_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TearoffMenuItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TextBuffer_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TextChildAnchor_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TextMark_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TextTag_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TextTagTable_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TextView_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ToggleAction_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ToggleButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ToggleToolButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ToolButton_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class ToolItem_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Toolbar_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Tooltips_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TreeModelFilter_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TreeModelSort_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TreeSelection_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TreeStore_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TreeView_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class TreeViewColumn_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class UIManager_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class VBox_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class VButtonBox_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class VPaned_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class VRuler_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class VScale_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class VScrollbar_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class VSeparator_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Viewport_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Widget_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class Window_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }
namespace Gtk {  class WindowGroup_Class { public: static Glib::ObjectBase* wrap_new(GObject*); };  }

namespace Gtk { 

void wrap_init()
{
  // Register Error domains:
  Glib::Error::register_domain(gtk_file_chooser_error_quark(), &Gtk::FileChooserError::throw_func);
  Glib::Error::register_domain(gtk_icon_theme_error_quark(), &Gtk::IconThemeError::throw_func);
  Glib::Error::register_domain(gtk_print_error_quark(), &Gtk::PrintError::throw_func);
  Glib::Error::register_domain(gtk_recent_chooser_error_quark(), &Gtk::RecentChooserError::throw_func);
  Glib::Error::register_domain(gtk_recent_manager_error_quark(), &Gtk::RecentManagerError::throw_func);

// Map gtypes to gtkmm wrapper-creation functions:
  Glib::wrap_register(gtk_about_dialog_get_type(), &Gtk::AboutDialog_Class::wrap_new);
  Glib::wrap_register(gtk_accel_group_get_type(), &Gtk::AccelGroup_Class::wrap_new);
  Glib::wrap_register(gtk_accel_label_get_type(), &Gtk::AccelLabel_Class::wrap_new);
  Glib::wrap_register(gtk_action_get_type(), &Gtk::Action_Class::wrap_new);
  Glib::wrap_register(gtk_action_group_get_type(), &Gtk::ActionGroup_Class::wrap_new);
  Glib::wrap_register(gtk_adjustment_get_type(), &Gtk::Adjustment_Class::wrap_new);
  Glib::wrap_register(gtk_alignment_get_type(), &Gtk::Alignment_Class::wrap_new);
  Glib::wrap_register(gtk_arrow_get_type(), &Gtk::Arrow_Class::wrap_new);
  Glib::wrap_register(gtk_aspect_frame_get_type(), &Gtk::AspectFrame_Class::wrap_new);
  Glib::wrap_register(gtk_assistant_get_type(), &Gtk::Assistant_Class::wrap_new);
  Glib::wrap_register(gtk_bin_get_type(), &Gtk::Bin_Class::wrap_new);
  Glib::wrap_register(gtk_box_get_type(), &Gtk::Box_Class::wrap_new);
  Glib::wrap_register(gtk_button_get_type(), &Gtk::Button_Class::wrap_new);
  Glib::wrap_register(gtk_button_box_get_type(), &Gtk::ButtonBox_Class::wrap_new);
  Glib::wrap_register(gtk_calendar_get_type(), &Gtk::Calendar_Class::wrap_new);
  Glib::wrap_register(gtk_cell_renderer_get_type(), &Gtk::CellRenderer_Class::wrap_new);
  Glib::wrap_register(gtk_cell_renderer_accel_get_type(), &Gtk::CellRendererAccel_Class::wrap_new);
  Glib::wrap_register(gtk_cell_renderer_combo_get_type(), &Gtk::CellRendererCombo_Class::wrap_new);
  Glib::wrap_register(gtk_cell_renderer_pixbuf_get_type(), &Gtk::CellRendererPixbuf_Class::wrap_new);
  Glib::wrap_register(gtk_cell_renderer_progress_get_type(), &Gtk::CellRendererProgress_Class::wrap_new);
  Glib::wrap_register(gtk_cell_renderer_spin_get_type(), &Gtk::CellRendererSpin_Class::wrap_new);
  Glib::wrap_register(gtk_cell_renderer_text_get_type(), &Gtk::CellRendererText_Class::wrap_new);
  Glib::wrap_register(gtk_cell_renderer_toggle_get_type(), &Gtk::CellRendererToggle_Class::wrap_new);
  Glib::wrap_register(gtk_cell_view_get_type(), &Gtk::CellView_Class::wrap_new);
  Glib::wrap_register(gtk_check_button_get_type(), &Gtk::CheckButton_Class::wrap_new);
  Glib::wrap_register(gtk_check_menu_item_get_type(), &Gtk::CheckMenuItem_Class::wrap_new);
  Glib::wrap_register(gtk_clipboard_get_type(), &Gtk::Clipboard_Class::wrap_new);
  Glib::wrap_register(gtk_color_button_get_type(), &Gtk::ColorButton_Class::wrap_new);
  Glib::wrap_register(gtk_color_selection_get_type(), &Gtk::ColorSelection_Class::wrap_new);
  Glib::wrap_register(gtk_color_selection_dialog_get_type(), &Gtk::ColorSelectionDialog_Class::wrap_new);
#ifndef GTKMM_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_combo_get_type(), &Gtk::Combo_Class::wrap_new);
#endif // *_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_combo_box_get_type(), &Gtk::ComboBox_Class::wrap_new);
  Glib::wrap_register(gtk_combo_box_entry_get_type(), &Gtk::ComboBoxEntry_Class::wrap_new);
#ifndef GTKMM_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_list_get_type(), &Gtk::ComboDropDown_Class::wrap_new);
#endif // *_DISABLE_DEPRECATED
#ifndef GTKMM_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_list_item_get_type(), &Gtk::ComboDropDownItem_Class::wrap_new);
#endif // *_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_container_get_type(), &Gtk::Container_Class::wrap_new);
  Glib::wrap_register(gtk_curve_get_type(), &Gtk::Curve_Class::wrap_new);
  Glib::wrap_register(gtk_dialog_get_type(), &Gtk::Dialog_Class::wrap_new);
  Glib::wrap_register(gtk_drawing_area_get_type(), &Gtk::DrawingArea_Class::wrap_new);
  Glib::wrap_register(gtk_entry_get_type(), &Gtk::Entry_Class::wrap_new);
  Glib::wrap_register(gtk_entry_completion_get_type(), &Gtk::EntryCompletion_Class::wrap_new);
  Glib::wrap_register(gtk_event_box_get_type(), &Gtk::EventBox_Class::wrap_new);
  Glib::wrap_register(gtk_expander_get_type(), &Gtk::Expander_Class::wrap_new);
  Glib::wrap_register(gtk_file_chooser_button_get_type(), &Gtk::FileChooserButton_Class::wrap_new);
  Glib::wrap_register(gtk_file_chooser_dialog_get_type(), &Gtk::FileChooserDialog_Class::wrap_new);
  Glib::wrap_register(gtk_file_chooser_widget_get_type(), &Gtk::FileChooserWidget_Class::wrap_new);
  Glib::wrap_register(gtk_file_filter_get_type(), &Gtk::FileFilter_Class::wrap_new);
#ifndef GTKMM_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_file_selection_get_type(), &Gtk::FileSelection_Class::wrap_new);
#endif // *_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_fixed_get_type(), &Gtk::Fixed_Class::wrap_new);
  Glib::wrap_register(gtk_font_button_get_type(), &Gtk::FontButton_Class::wrap_new);
  Glib::wrap_register(gtk_font_selection_get_type(), &Gtk::FontSelection_Class::wrap_new);
  Glib::wrap_register(gtk_font_selection_dialog_get_type(), &Gtk::FontSelectionDialog_Class::wrap_new);
  Glib::wrap_register(gtk_frame_get_type(), &Gtk::Frame_Class::wrap_new);
  Glib::wrap_register(gtk_gamma_curve_get_type(), &Gtk::GammaCurve_Class::wrap_new);
  Glib::wrap_register(gtk_hbox_get_type(), &Gtk::HBox_Class::wrap_new);
  Glib::wrap_register(gtk_hbutton_box_get_type(), &Gtk::HButtonBox_Class::wrap_new);
  Glib::wrap_register(gtk_hpaned_get_type(), &Gtk::HPaned_Class::wrap_new);
  Glib::wrap_register(gtk_hruler_get_type(), &Gtk::HRuler_Class::wrap_new);
  Glib::wrap_register(gtk_hscale_get_type(), &Gtk::HScale_Class::wrap_new);
  Glib::wrap_register(gtk_hscrollbar_get_type(), &Gtk::HScrollbar_Class::wrap_new);
  Glib::wrap_register(gtk_hseparator_get_type(), &Gtk::HSeparator_Class::wrap_new);
  Glib::wrap_register(gtk_handle_box_get_type(), &Gtk::HandleBox_Class::wrap_new);
  Glib::wrap_register(gtk_icon_factory_get_type(), &Gtk::IconFactory_Class::wrap_new);
  Glib::wrap_register(gtk_icon_theme_get_type(), &Gtk::IconTheme_Class::wrap_new);
  Glib::wrap_register(gtk_icon_view_get_type(), &Gtk::IconView_Class::wrap_new);
  Glib::wrap_register(gtk_image_get_type(), &Gtk::Image_Class::wrap_new);
  Glib::wrap_register(gtk_image_menu_item_get_type(), &Gtk::ImageMenuItem_Class::wrap_new);
  Glib::wrap_register(gtk_input_dialog_get_type(), &Gtk::InputDialog_Class::wrap_new);
  Glib::wrap_register(gtk_invisible_get_type(), &Gtk::Invisible_Class::wrap_new);
  Glib::wrap_register(gtk_item_get_type(), &Gtk::Item_Class::wrap_new);
  Glib::wrap_register(gtk_label_get_type(), &Gtk::Label_Class::wrap_new);
  Glib::wrap_register(gtk_layout_get_type(), &Gtk::Layout_Class::wrap_new);
  Glib::wrap_register(gtk_link_button_get_type(), &Gtk::LinkButton_Class::wrap_new);
  Glib::wrap_register(gtk_list_store_get_type(), &Gtk::ListStore_Class::wrap_new);
  Glib::wrap_register(gtk_menu_get_type(), &Gtk::Menu_Class::wrap_new);
  Glib::wrap_register(gtk_menu_bar_get_type(), &Gtk::MenuBar_Class::wrap_new);
  Glib::wrap_register(gtk_menu_item_get_type(), &Gtk::MenuItem_Class::wrap_new);
  Glib::wrap_register(gtk_menu_shell_get_type(), &Gtk::MenuShell_Class::wrap_new);
  Glib::wrap_register(gtk_menu_tool_button_get_type(), &Gtk::MenuToolButton_Class::wrap_new);
  Glib::wrap_register(gtk_message_dialog_get_type(), &Gtk::MessageDialog_Class::wrap_new);
  Glib::wrap_register(gtk_misc_get_type(), &Gtk::Misc_Class::wrap_new);
  Glib::wrap_register(gtk_notebook_get_type(), &Gtk::Notebook_Class::wrap_new);
  Glib::wrap_register(gtk_object_get_type(), &Gtk::Object_Class::wrap_new);
#ifndef GTKMM_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_option_menu_get_type(), &Gtk::OptionMenu_Class::wrap_new);
#endif // *_DISABLE_DEPRECATED
  Glib::wrap_register(gtk_page_setup_get_type(), &Gtk::PageSetup_Class::wrap_new);
#ifndef G_OS_WIN32
  Glib::wrap_register(gtk_page_setup_unix_dialog_get_type(), &Gtk::PageSetupUnixDialog_Class::wrap_new);
#endif //G_OS_WIN32
  Glib::wrap_register(gtk_paned_get_type(), &Gtk::Paned_Class::wrap_new);
#ifndef G_OS_WIN32
  Glib::wrap_register(gtk_plug_get_type(), &Gtk::Plug_Class::wrap_new);
#endif //G_OS_WIN32
  Glib::wrap_register(gtk_print_context_get_type(), &Gtk::PrintContext_Class::wrap_new);
#ifndef G_OS_WIN32
  Glib::wrap_register(gtk_print_job_get_type(), &Gtk::PrintJob_Class::wrap_new);
#endif //G_OS_WIN32
  Glib::wrap_register(gtk_print_operation_get_type(), &Gtk::PrintOperation_Class::wrap_new);
  Glib::wrap_register(gtk_print_settings_get_type(), &Gtk::PrintSettings_Class::wrap_new);
#ifndef G_OS_WIN32
  Glib::wrap_register(gtk_print_unix_dialog_get_type(), &Gtk::PrintUnixDialog_Class::wrap_new);
#endif //G_OS_WIN32
#ifndef G_OS_WIN32
  Glib::wrap_register(gtk_printer_get_type(), &Gtk::Printer_Class::wrap_new);
#endif //G_OS_WIN32
  Glib::wrap_register(gtk_progress_bar_get_type(), &Gtk::ProgressBar_Class::wrap_new);
  Glib::wrap_register(gtk_radio_action_get_type(), &Gtk::RadioAction_Class::wrap_new);
  Glib::wrap_register(gtk_radio_button_get_type(), &Gtk::RadioButton_Class::wrap_new);
  Glib::wrap_register(gtk_radio_menu_item_get_type(), &Gtk::RadioMenuItem_Class::wrap_new);
  Glib::wrap_register(gtk_radio_tool_button_get_type(), &Gtk::RadioToolButton_Class::wrap_new);
  Glib::wrap_register(gtk_range_get_type(), &Gtk::Range_Class::wrap_new);
  Glib::wrap_register(gtk_rc_style_get_type(), &Gtk::RcStyle_Class::wrap_new);
  Glib::wrap_register(gtk_recent_chooser_dialog_get_type(), &Gtk::RecentChooserDialog_Class::wrap_new);
  Glib::wrap_register(gtk_recent_chooser_menu_get_type(), &Gtk::RecentChooserMenu_Class::wrap_new);
  Glib::wrap_register(gtk_recent_chooser_widget_get_type(), &Gtk::RecentChooserWidget_Class::wrap_new);
  Glib::wrap_register(gtk_recent_filter_get_type(), &Gtk::RecentFilter_Class::wrap_new);
  Glib::wrap_register(gtk_recent_manager_get_type(), &Gtk::RecentManager_Class::wrap_new);
  Glib::wrap_register(gtk_ruler_get_type(), &Gtk::Ruler_Class::wrap_new);
  Glib::wrap_register(gtk_scale_get_type(), &Gtk::Scale_Class::wrap_new);
  Glib::wrap_register(gtk_scrollbar_get_type(), &Gtk::Scrollbar_Class::wrap_new);
  Glib::wrap_register(gtk_scrolled_window_get_type(), &Gtk::ScrolledWindow_Class::wrap_new);
  Glib::wrap_register(gtk_separator_get_type(), &Gtk::Separator_Class::wrap_new);
  Glib::wrap_register(gtk_separator_menu_item_get_type(), &Gtk::SeparatorMenuItem_Class::wrap_new);
  Glib::wrap_register(gtk_separator_tool_item_get_type(), &Gtk::SeparatorToolItem_Class::wrap_new);
  Glib::wrap_register(gtk_settings_get_type(), &Gtk::Settings_Class::wrap_new);
  Glib::wrap_register(gtk_size_group_get_type(), &Gtk::SizeGroup_Class::wrap_new);
#ifndef G_OS_WIN32
  Glib::wrap_register(gtk_socket_get_type(), &Gtk::Socket_Class::wrap_new);
#endif //G_OS_WIN32
  Glib::wrap_register(gtk_spin_button_get_type(), &Gtk::SpinButton_Class::wrap_new);
  Glib::wrap_register(gtk_status_icon_get_type(), &Gtk::StatusIcon_Class::wrap_new);
  Glib::wrap_register(gtk_statusbar_get_type(), &Gtk::Statusbar_Class::wrap_new);
  Glib::wrap_register(gtk_style_get_type(), &Gtk::Style_Class::wrap_new);
  Glib::wrap_register(gtk_table_get_type(), &Gtk::Table_Class::wrap_new);
  Glib::wrap_register(gtk_tearoff_menu_item_get_type(), &Gtk::TearoffMenuItem_Class::wrap_new);
  Glib::wrap_register(gtk_text_buffer_get_type(), &Gtk::TextBuffer_Class::wrap_new);
  Glib::wrap_register(gtk_text_child_anchor_get_type(), &Gtk::TextChildAnchor_Class::wrap_new);
  Glib::wrap_register(gtk_text_mark_get_type(), &Gtk::TextMark_Class::wrap_new);
  Glib::wrap_register(gtk_text_tag_get_type(), &Gtk::TextTag_Class::wrap_new);
  Glib::wrap_register(gtk_text_tag_table_get_type(), &Gtk::TextTagTable_Class::wrap_new);
  Glib::wrap_register(gtk_text_view_get_type(), &Gtk::TextView_Class::wrap_new);
  Glib::wrap_register(gtk_toggle_action_get_type(), &Gtk::ToggleAction_Class::wrap_new);
  Glib::wrap_register(gtk_toggle_button_get_type(), &Gtk::ToggleButton_Class::wrap_new);
  Glib::wrap_register(gtk_toggle_tool_button_get_type(), &Gtk::ToggleToolButton_Class::wrap_new);
  Glib::wrap_register(gtk_tool_button_get_type(), &Gtk::ToolButton_Class::wrap_new);
  Glib::wrap_register(gtk_tool_item_get_type(), &Gtk::ToolItem_Class::wrap_new);
  Glib::wrap_register(gtk_toolbar_get_type(), &Gtk::Toolbar_Class::wrap_new);
  Glib::wrap_register(gtk_tooltips_get_type(), &Gtk::Tooltips_Class::wrap_new);
  Glib::wrap_register(gtk_tree_model_filter_get_type(), &Gtk::TreeModelFilter_Class::wrap_new);
  Glib::wrap_register(gtk_tree_model_sort_get_type(), &Gtk::TreeModelSort_Class::wrap_new);
  Glib::wrap_register(gtk_tree_selection_get_type(), &Gtk::TreeSelection_Class::wrap_new);
  Glib::wrap_register(gtk_tree_store_get_type(), &Gtk::TreeStore_Class::wrap_new);
  Glib::wrap_register(gtk_tree_view_get_type(), &Gtk::TreeView_Class::wrap_new);
  Glib::wrap_register(gtk_tree_view_column_get_type(), &Gtk::TreeViewColumn_Class::wrap_new);
  Glib::wrap_register(gtk_ui_manager_get_type(), &Gtk::UIManager_Class::wrap_new);
  Glib::wrap_register(gtk_vbox_get_type(), &Gtk::VBox_Class::wrap_new);
  Glib::wrap_register(gtk_vbutton_box_get_type(), &Gtk::VButtonBox_Class::wrap_new);
  Glib::wrap_register(gtk_vpaned_get_type(), &Gtk::VPaned_Class::wrap_new);
  Glib::wrap_register(gtk_vruler_get_type(), &Gtk::VRuler_Class::wrap_new);
  Glib::wrap_register(gtk_vscale_get_type(), &Gtk::VScale_Class::wrap_new);
  Glib::wrap_register(gtk_vscrollbar_get_type(), &Gtk::VScrollbar_Class::wrap_new);
  Glib::wrap_register(gtk_vseparator_get_type(), &Gtk::VSeparator_Class::wrap_new);
  Glib::wrap_register(gtk_viewport_get_type(), &Gtk::Viewport_Class::wrap_new);
  Glib::wrap_register(gtk_widget_get_type(), &Gtk::Widget_Class::wrap_new);
  Glib::wrap_register(gtk_window_get_type(), &Gtk::Window_Class::wrap_new);
  Glib::wrap_register(gtk_window_group_get_type(), &Gtk::WindowGroup_Class::wrap_new);

  // Register the gtkmm gtypes:
  Gtk::AboutDialog::get_type();
  Gtk::AccelGroup::get_type();
  Gtk::AccelLabel::get_type();
  Gtk::Action::get_type();
  Gtk::ActionGroup::get_type();
  Gtk::Adjustment::get_type();
  Gtk::Alignment::get_type();
  Gtk::Arrow::get_type();
  Gtk::AspectFrame::get_type();
  Gtk::Assistant::get_type();
  Gtk::Bin::get_type();
  Gtk::Box::get_type();
  Gtk::Button::get_type();
  Gtk::ButtonBox::get_type();
  Gtk::Calendar::get_type();
  Gtk::CellRenderer::get_type();
  Gtk::CellRendererAccel::get_type();
  Gtk::CellRendererCombo::get_type();
  Gtk::CellRendererPixbuf::get_type();
  Gtk::CellRendererProgress::get_type();
  Gtk::CellRendererSpin::get_type();
  Gtk::CellRendererText::get_type();
  Gtk::CellRendererToggle::get_type();
  Gtk::CellView::get_type();
  Gtk::CheckButton::get_type();
  Gtk::CheckMenuItem::get_type();
  Gtk::Clipboard::get_type();
  Gtk::ColorButton::get_type();
  Gtk::ColorSelection::get_type();
  Gtk::ColorSelectionDialog::get_type();
#ifndef GTKMM_DISABLE_DEPRECATED
  Gtk::Combo::get_type();
#endif // *_DISABLE_DEPRECATED
  Gtk::ComboBox::get_type();
  Gtk::ComboBoxEntry::get_type();
#ifndef GTKMM_DISABLE_DEPRECATED
  Gtk::ComboDropDown::get_type();
#endif // *_DISABLE_DEPRECATED
#ifndef GTKMM_DISABLE_DEPRECATED
  Gtk::ComboDropDownItem::get_type();
#endif // *_DISABLE_DEPRECATED
  Gtk::Container::get_type();
  Gtk::Curve::get_type();
  Gtk::Dialog::get_type();
  Gtk::DrawingArea::get_type();
  Gtk::Entry::get_type();
  Gtk::EntryCompletion::get_type();
  Gtk::EventBox::get_type();
  Gtk::Expander::get_type();
  Gtk::FileChooserButton::get_type();
  Gtk::FileChooserDialog::get_type();
  Gtk::FileChooserWidget::get_type();
  Gtk::FileFilter::get_type();
#ifndef GTKMM_DISABLE_DEPRECATED
  Gtk::FileSelection::get_type();
#endif // *_DISABLE_DEPRECATED
  Gtk::Fixed::get_type();
  Gtk::FontButton::get_type();
  Gtk::FontSelection::get_type();
  Gtk::FontSelectionDialog::get_type();
  Gtk::Frame::get_type();
  Gtk::GammaCurve::get_type();
  Gtk::HBox::get_type();
  Gtk::HButtonBox::get_type();
  Gtk::HPaned::get_type();
  Gtk::HRuler::get_type();
  Gtk::HScale::get_type();
  Gtk::HScrollbar::get_type();
  Gtk::HSeparator::get_type();
  Gtk::HandleBox::get_type();
  Gtk::IconFactory::get_type();
  Gtk::IconTheme::get_type();
  Gtk::IconView::get_type();
  Gtk::Image::get_type();
  Gtk::ImageMenuItem::get_type();
  Gtk::InputDialog::get_type();
  Gtk::Invisible::get_type();
  Gtk::Item::get_type();
  Gtk::Label::get_type();
  Gtk::Layout::get_type();
  Gtk::LinkButton::get_type();
  Gtk::ListStore::get_type();
  Gtk::Menu::get_type();
  Gtk::MenuBar::get_type();
  Gtk::MenuItem::get_type();
  Gtk::MenuShell::get_type();
  Gtk::MenuToolButton::get_type();
  Gtk::MessageDialog::get_type();
  Gtk::Misc::get_type();
  Gtk::Notebook::get_type();
  Gtk::Object::get_type();
#ifndef GTKMM_DISABLE_DEPRECATED
  Gtk::OptionMenu::get_type();
#endif // *_DISABLE_DEPRECATED
  Gtk::PageSetup::get_type();
#ifndef G_OS_WIN32
  Gtk::PageSetupUnixDialog::get_type();
#endif //G_OS_WIN32
  Gtk::Paned::get_type();
#ifndef G_OS_WIN32
  Gtk::Plug::get_type();
#endif //G_OS_WIN32
  Gtk::PrintContext::get_type();
#ifndef G_OS_WIN32
  Gtk::PrintJob::get_type();
#endif //G_OS_WIN32
  Gtk::PrintOperation::get_type();
  Gtk::PrintSettings::get_type();
#ifndef G_OS_WIN32
  Gtk::PrintUnixDialog::get_type();
#endif //G_OS_WIN32
#ifndef G_OS_WIN32
  Gtk::Printer::get_type();
#endif //G_OS_WIN32
  Gtk::ProgressBar::get_type();
  Gtk::RadioAction::get_type();
  Gtk::RadioButton::get_type();
  Gtk::RadioMenuItem::get_type();
  Gtk::RadioToolButton::get_type();
  Gtk::Range::get_type();
  Gtk::RcStyle::get_type();
  Gtk::RecentChooserDialog::get_type();
  Gtk::RecentChooserMenu::get_type();
  Gtk::RecentChooserWidget::get_type();
  Gtk::RecentFilter::get_type();
  Gtk::RecentManager::get_type();
  Gtk::Ruler::get_type();
  Gtk::Scale::get_type();
  Gtk::Scrollbar::get_type();
  Gtk::ScrolledWindow::get_type();
  Gtk::Separator::get_type();
  Gtk::SeparatorMenuItem::get_type();
  Gtk::SeparatorToolItem::get_type();
  Gtk::Settings::get_type();
  Gtk::SizeGroup::get_type();
#ifndef G_OS_WIN32
  Gtk::Socket::get_type();
#endif //G_OS_WIN32
  Gtk::SpinButton::get_type();
  Gtk::StatusIcon::get_type();
  Gtk::Statusbar::get_type();
  Gtk::Style::get_type();
  Gtk::Table::get_type();
  Gtk::TearoffMenuItem::get_type();
  Gtk::TextBuffer::get_type();
  Gtk::TextChildAnchor::get_type();
  Gtk::TextMark::get_type();
  Gtk::TextTag::get_type();
  Gtk::TextTagTable::get_type();
  Gtk::TextView::get_type();
  Gtk::ToggleAction::get_type();
  Gtk::ToggleButton::get_type();
  Gtk::ToggleToolButton::get_type();
  Gtk::ToolButton::get_type();
  Gtk::ToolItem::get_type();
  Gtk::Toolbar::get_type();
  Gtk::Tooltips::get_type();
  Gtk::TreeModelFilter::get_type();
  Gtk::TreeModelSort::get_type();
  Gtk::TreeSelection::get_type();
  Gtk::TreeStore::get_type();
  Gtk::TreeView::get_type();
  Gtk::TreeViewColumn::get_type();
  Gtk::UIManager::get_type();
  Gtk::VBox::get_type();
  Gtk::VButtonBox::get_type();
  Gtk::VPaned::get_type();
  Gtk::VRuler::get_type();
  Gtk::VScale::get_type();
  Gtk::VScrollbar::get_type();
  Gtk::VSeparator::get_type();
  Gtk::Viewport::get_type();
  Gtk::Widget::get_type();
  Gtk::Window::get_type();
  Gtk::WindowGroup::get_type();

} // wrap_init()

} //Gtk


