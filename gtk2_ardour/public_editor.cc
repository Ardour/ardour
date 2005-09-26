#include "public_editor.h"
#include "editor.h"

PublicEditor* PublicEditor::_instance = 0;

PublicEditor::PublicEditor ()
	: Window (GTK_WINDOW_TOPLEVEL),
	  KeyboardTarget (*this, "editor")
{
}

PublicEditor::~PublicEditor()
{
}

gint
PublicEditor::canvas_fade_in_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_fade_in_event() (item, event, data);
}
gint
PublicEditor::canvas_fade_in_handle_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_fade_in_handle_event() (item, event, data);
}
gint
PublicEditor::canvas_fade_out_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_fade_out_event() (item, event, data);
}
gint
PublicEditor::canvas_fade_out_handle_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_fade_out_handle_event() (item, event, data);
}
gint
PublicEditor::canvas_crossfade_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_crossfade_view_event() (item, event, data);
}
gint
PublicEditor::canvas_region_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_region_view_event() (item, event, data);
}
gint
PublicEditor::canvas_region_view_name_highlight_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_region_view_name_highlight_event() (item, event, data);
}
gint
PublicEditor::canvas_region_view_name_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_region_view_name_event() (item, event, data);
}
gint
PublicEditor::canvas_stream_view_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_stream_view_event() (item, event, data);
}
gint
PublicEditor::canvas_automation_track_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_automation_track_event() (item, event, data);
}
gint
PublicEditor::canvas_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_marker_event() (item, event, data);
}
gint
PublicEditor::canvas_zoom_rect_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_zoom_rect_event() (item, event, data);
}
gint
PublicEditor::canvas_selection_rect_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_selection_rect_event() (item, event, data);
}
gint
PublicEditor::canvas_selection_start_trim_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_selection_start_trim_event() (item, event, data);
}
gint
PublicEditor::canvas_selection_end_trim_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_selection_end_trim_event() (item, event, data);
}
gint
PublicEditor::canvas_control_point_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_control_point_event() (item, event, data);
}
gint
PublicEditor::canvas_line_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_line_event() (item, event, data);
}
gint
PublicEditor::canvas_tempo_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_tempo_marker_event() (item, event, data);
}
gint
PublicEditor::canvas_meter_marker_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_meter_marker_event() (item, event, data);
}
gint
PublicEditor::canvas_tempo_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_tempo_bar_event() (item, event, data);
}
gint
PublicEditor::canvas_meter_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_meter_bar_event() (item, event, data);
}
gint
PublicEditor::canvas_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_marker_bar_event() (item, event, data);
}
gint
PublicEditor::canvas_range_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_range_marker_bar_event() (item, event, data);
}
gint
PublicEditor::canvas_transport_marker_bar_event (GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_transport_marker_bar_event() (item, event, data);
}
	
gint
PublicEditor::canvas_imageframe_item_view_event(GnomeCanvasItem *item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_imageframe_item_view_event()(item, event, data);
}
gint
PublicEditor::canvas_imageframe_view_event(GnomeCanvasItem *item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_imageframe_view_event()(item, event, data);
}
gint
PublicEditor::canvas_imageframe_start_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_imageframe_start_handle_event()(item, event, data);
}
gint
PublicEditor::canvas_imageframe_end_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_imageframe_end_handle_event()(item, event, data);
}
gint
PublicEditor::canvas_marker_time_axis_view_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_marker_time_axis_view_event()(item, event, data);
}
gint
PublicEditor::canvas_markerview_item_view_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_markerview_item_view_event()(item, event, data);
}
gint
PublicEditor::canvas_markerview_start_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_markerview_start_handle_event()(item, event, data);
}
gint
PublicEditor::canvas_markerview_end_handle_event(GnomeCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->signal__canvas_markerview_end_handle_event()(item, event, data);
}
