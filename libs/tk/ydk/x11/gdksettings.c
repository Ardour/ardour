/* GDK - The GIMP Drawing Kit
 * Copyright (C) 1995-1997 Peter Mattis, Spencer Kimball and Josh MacDonald
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

/*
 * Modified by the GTK+ Team and others 1997-2000.  See the AUTHORS
 * file for a list of people on the GTK+ Team.  See the ChangeLog
 * files for a list of changes.  These files are distributed with
 * GTK+ at ftp://ftp.gtk.org/pub/gtk/.
 */


#define GDK_SETTINGS_N_ELEMENTS()       G_N_ELEMENTS (gdk_settings_map)
#define GDK_SETTINGS_X_NAME(nth)        (gdk_settings_names + gdk_settings_map[nth].xsettings_offset)
#define GDK_SETTINGS_GDK_NAME(nth)      (gdk_settings_names + gdk_settings_map[nth].gdk_offset)

/* WARNING:
 * You will need to update gdk_settings_map when adding a
 * new setting, and make sure that checksettings does not
 * fail before committing
 */
static const char gdk_settings_names[] =
  "Net/DoubleClickTime\0"     "gtk-double-click-time\0"
  "Net/DoubleClickDistance\0" "gtk-double-click-distance\0"
  "Net/DndDragThreshold\0"    "gtk-dnd-drag-threshold\0"
  "Net/CursorBlink\0"         "gtk-cursor-blink\0"
  "Net/CursorBlinkTime\0"     "gtk-cursor-blink-time\0"
  "Net/ThemeName\0"           "gtk-theme-name\0"
  "Net/IconThemeName\0"       "gtk-icon-theme-name\0"
  "Gtk/CanChangeAccels\0"     "gtk-can-change-accels\0"
  "Gtk/ColorPalette\0"        "gtk-color-palette\0"
  "Gtk/FontName\0"            "gtk-font-name\0"
  "Gtk/IconSizes\0"           "gtk-icon-sizes\0"
  "Gtk/KeyThemeName\0"        "gtk-key-theme-name\0"
  "Gtk/ToolbarStyle\0"        "gtk-toolbar-style\0"
  "Gtk/ToolbarIconSize\0"     "gtk-toolbar-icon-size\0"
  "Gtk/IMPreeditStyle\0"      "gtk-im-preedit-style\0"
  "Gtk/IMStatusStyle\0"       "gtk-im-status-style\0"
  "Gtk/Modules\0"             "gtk-modules\0"
  "Gtk/FileChooserBackend\0"  "gtk-file-chooser-backend\0"
  "Gtk/ButtonImages\0"        "gtk-button-images\0"
  "Gtk/MenuImages\0"          "gtk-menu-images\0"
  "Gtk/MenuBarAccel\0"        "gtk-menu-bar-accel\0"
  "Gtk/CursorThemeName\0"     "gtk-cursor-theme-name\0"
  "Gtk/CursorThemeSize\0"     "gtk-cursor-theme-size\0"
  "Gtk/ShowInputMethodMenu\0" "gtk-show-input-method-menu\0"
  "Gtk/ShowUnicodeMenu\0"     "gtk-show-unicode-menu\0"
  "Gtk/TimeoutInitial\0"      "gtk-timeout-initial\0"
  "Gtk/TimeoutRepeat\0"       "gtk-timeout-repeat\0"
  "Gtk/ColorScheme\0"         "gtk-color-scheme\0"
  "Gtk/EnableAnimations\0"    "gtk-enable-animations\0"
  "Xft/Antialias\0"           "gtk-xft-antialias\0"
  "Xft/Hinting\0"             "gtk-xft-hinting\0"
  "Xft/HintStyle\0"           "gtk-xft-hintstyle\0"
  "Xft/RGBA\0"                "gtk-xft-rgba\0"
  "Xft/DPI\0"                 "gtk-xft-dpi\0"
  "Net/FallbackIconTheme\0"   "gtk-fallback-icon-theme\0"
  "Gtk/TouchscreenMode\0"     "gtk-touchscreen-mode\0"
  "Gtk/EnableAccels\0"        "gtk-enable-accels\0"
  "Gtk/EnableMnemonics\0"     "gtk-enable-mnemonics\0"
  "Gtk/ScrolledWindowPlacement\0" "gtk-scrolled-window-placement\0"
  "Gtk/IMModule\0"            "gtk-im-module\0"
  "Fontconfig/Timestamp\0"    "gtk-fontconfig-timestamp\0"
  "Net/SoundThemeName\0"      "gtk-sound-theme-name\0"
  "Net/EnableInputFeedbackSounds\0" "gtk-enable-input-feedback-sounds\0"
  "Net/EnableEventSounds\0"   "gtk-enable-event-sounds\0"
  "Gtk/CursorBlinkTimeout\0"  "gtk-cursor-blink-timeout\0"
  "Gtk/AutoMnemonics\0"       "gtk-auto-mnemonics\0";


static const struct
{
  gint xsettings_offset;
  gint gdk_offset;
} gdk_settings_map[] = {
  {    0,   20 },
  {   42,   66 },
  {   92,  113 },
  {  136,  152 },
  {  169,  189 },
  {  211,  225 },
  {  240,  258 },
  {  278,  298 },
  {  320,  337 },
  {  355,  368 },
  {  382,  396 },
  {  411,  428 },
  {  447,  464 },
  {  482,  502 },
  {  524,  543 },
  {  564,  582 },
  {  602,  614 },
  {  626,  649 },
  {  674,  691 },
  {  709,  724 },
  {  740,  757 },
  {  776,  796 },
  {  818,  838 },
  {  860,  884 },
  {  911,  931 },
  {  953,  972 },
  {  992, 1010 },
  { 1029, 1045 },
  { 1062, 1083 },
  { 1105, 1119 },
  { 1137, 1149 },
  { 1165, 1179 },
  { 1197, 1206 },
  { 1219, 1227 },
  { 1239, 1261 },
  { 1285, 1305 },
  { 1326, 1343 },
  { 1361, 1381 },
  { 1402, 1430 },
  { 1460, 1473 },
  { 1487, 1508 },
  { 1533, 1552 },
  { 1573, 1603 },
  { 1636, 1658 },
  { 1682, 1705 },
  { 1730, 1748 }
};
