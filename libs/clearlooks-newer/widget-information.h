
/* Object Type Lookups/Macros

   Based on/modified from functions in
   Smooth-Engine.
*/
#define GE_IS_WIDGET(object) ((object)  && ge_object_is_a ((GObject*)(object), "GtkWidget"))
#define GE_IS_CONTAINER(object) ((object)  && ge_object_is_a ((GObject*)(object), "GtkContainer"))
#define GE_IS_BIN(object) ((object)  && ge_object_is_a ((GObject*)(object), "GtkBin"))

#define GE_IS_ARROW(object) ((object)  && ge_object_is_a ((GObject*)(object), "GtkArrow"))

#define GE_IS_SEPARATOR(object) ((object)  && ge_object_is_a ((GObject*)(object), "GtkSeparator"))
#define GE_IS_VSEPARATOR(object) ((object)  && ge_object_is_a ((GObject*)(object), "GtkVSeparator"))
#define GE_IS_HSEPARATOR(object) ((object)  && ge_object_is_a ((GObject*)(object), "GtkHSeparator"))

#define GE_IS_HANDLE_BOX(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkHandleBox"))
#define GE_IS_HANDLE_BOX_ITEM(object) ((object) && GE_IS_HANDLE_BOX(object->parent))
#define GE_IS_BONOBO_DOCK_ITEM(object) ((object) && ge_object_is_a ((GObject*)(object), "BonoboDockItem"))
#define GE_IS_BONOBO_DOCK_ITEM_GRIP(object) ((object) && ge_object_is_a ((GObject*)(object), "BonoboDockItemGrip"))
#define GE_IS_BONOBO_TOOLBAR(object) ((object) && ge_object_is_a ((GObject*)(object), "BonoboUIToolbar"))
#define GE_IS_EGG_TOOLBAR(object) ((object) && ge_object_is_a ((GObject*)(object), "Toolbar"))
#define GE_IS_TOOLBAR(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkToolbar"))
#define GE_IS_PANEL_WIDGET(object) ((object) && (ge_object_is_a ((GObject*)(object), "PanelWidget") || ge_object_is_a ((GObject*)(object), "PanelApplet")))

#define GE_IS_COMBO_BOX_ENTRY(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkComboBoxEntry"))
#define GE_IS_COMBO_BOX(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkComboBox"))
#define GE_IS_COMBO(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkCombo"))
#define GE_IS_OPTION_MENU(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkOptionMenu"))

#define GE_IS_TOGGLE_BUTTON(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkToggleButton"))
#define GE_IS_CHECK_BUTTON(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkCheckButton"))
#define GE_IS_SPIN_BUTTON(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkSpinButton"))

#define GE_IS_STATUSBAR(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkStatusbar"))
#define GE_IS_PROGRESS_BAR(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkProgressBar"))

#define GE_IS_MENU_SHELL(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkMenuShell"))
#define GE_IS_MENU(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkMenu"))
#define GE_IS_MENU_BAR(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkMenuBar"))
#define GE_IS_MENU_ITEM(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkMenuItem"))

#define GE_IS_CHECK_MENU_ITEM(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkCheckMenuItem"))

#define GE_IS_RANGE(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkRange"))

#define GE_IS_SCROLLBAR(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkScrollbar"))
#define GE_IS_VSCROLLBAR(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkVScrollbar"))
#define GE_IS_HSCROLLBAR(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkHScrollbar"))

#define GE_IS_SCALE(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkScale"))
#define GE_IS_VSCALE(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkVScale"))
#define GE_IS_HSCALE(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkHScale"))

#define GE_IS_PANED(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkPaned"))
#define GE_IS_VPANED(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkVPaned"))
#define GE_IS_HPANED(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkHPaned"))

#define GE_IS_BOX(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkBox"))
#define GE_IS_VBOX(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkVBox"))
#define GE_IS_HBOX(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkHBox"))

#define GE_IS_CLIST(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkCList"))
#define GE_IS_TREE_VIEW(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkTreeView"))
#define GE_IS_ENTRY(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkEntry"))
#define GE_IS_BUTTON(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkButton"))
#define GE_IS_FIXED(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkFixed"))

#define TOGGLE_BUTTON(object) (GE_IS_TOGGLE_BUTTON(object)?(GtkToggleButton *)object:NULL)

#define GE_IS_NOTEBOOK(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkNotebook"))
#define GE_IS_CELL_RENDERER_TOGGLE(object) ((object) && ge_object_is_a ((GObject*)(object), "GtkCellRendererToggle"))

#define GE_WIDGET_HAS_DEFAULT(object) ((object) && GE_IS_WIDGET(object) && GTK_WIDGET_HAS_DEFAULT(object))

GE_INTERNAL gboolean ge_object_is_a (GObject * object, const gchar * type_name);

GE_INTERNAL gboolean ge_is_combo_box_entry (GtkWidget * widget);
GE_INTERNAL gboolean ge_is_combo_box (GtkWidget * widget, gboolean as_list);
GE_INTERNAL gboolean ge_is_combo (GtkWidget * widget);
GE_INTERNAL gboolean ge_is_in_combo_box (GtkWidget * widget);

GE_INTERNAL gboolean ge_is_toolbar_item (GtkWidget * widget);

GE_INTERNAL gboolean ge_is_panel_widget_item (GtkWidget * widget);

GE_INTERNAL gboolean ge_is_bonobo_dock_item (GtkWidget * widget);

GE_INTERNAL GtkWidget *ge_find_combo_box_widget_parent (GtkWidget * widget);

GE_INTERNAL void ge_option_menu_get_props (GtkWidget * widget,
                            GtkRequisition * indicator_size,
                            GtkBorder * indicator_spacing);

GE_INTERNAL void ge_button_get_default_border (GtkWidget *widget,
                                               GtkBorder *border);

GE_INTERNAL gboolean ge_widget_is_ltr (GtkWidget *widget);

