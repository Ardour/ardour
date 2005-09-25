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
PublicEditor::canvas_fade_in_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_fade_in_event (item, event, data);
}
gint
PublicEditor::canvas_fade_in_handle_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_fade_in_handle_event (item, event, data);
}
gint
PublicEditor::canvas_fade_out_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_fade_out_event (item, event, data);
}
gint
PublicEditor::canvas_fade_out_handle_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_fade_out_handle_event (item, event, data);
}
gint
PublicEditor::canvas_crossfade_view_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_crossfade_view_event (item, event, data);
}
gint
PublicEditor::canvas_region_view_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_region_view_event (item, event, data);
}
gint
PublicEditor::canvas_region_view_name_highlight_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_region_view_name_highlight_event (item, event, data);
}
gint
PublicEditor::canvas_region_view_name_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_region_view_name_event (item, event, data);
}
gint
PublicEditor::canvas_stream_view_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_stream_view_event (item, event, data);
}
gint
PublicEditor::canvas_automation_track_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_automation_track_event (item, event, data);
}
gint
PublicEditor::canvas_marker_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_marker_event (item, event, data);
}
gint
PublicEditor::canvas_zoom_rect_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_zoom_rect_event (item, event, data);
}
gint
PublicEditor::canvas_selection_rect_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_selection_rect_event (item, event, data);
}
gint
PublicEditor::canvas_selection_start_trim_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_selection_start_trim_event (item, event, data);
}
gint
PublicEditor::canvas_selection_end_trim_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_selection_end_trim_event (item, event, data);
}
gint
PublicEditor::canvas_control_point_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_control_point_event (item, event, data);
}
gint
PublicEditor::canvas_line_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_line_event (item, event, data);
}
gint
PublicEditor::canvas_tempo_marker_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_tempo_marker_event (item, event, data);
}
gint
PublicEditor::canvas_meter_marker_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_meter_marker_event (item, event, data);
}
gint
PublicEditor::canvas_tempo_bar_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_tempo_bar_event (item, event, data);
}
gint
PublicEditor::canvas_meter_bar_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_meter_bar_event (item, event, data);
}
gint
PublicEditor::canvas_marker_bar_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_marker_bar_event (item, event, data);
}
gint
PublicEditor::canvas_range_marker_bar_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_range_marker_bar_event (item, event, data);
}
gint
PublicEditor::canvas_transport_marker_bar_event (GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_transport_marker_bar_event (item, event, data);
}
	
gint
PublicEditor::canvas_imageframe_item_view_event(GtkCanvasItem *item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_imageframe_item_view_event(item, event, data);
}
gint
PublicEditor::canvas_imageframe_view_event(GtkCanvasItem *item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_imageframe_view_event(item, event, data);
}
gint
PublicEditor::canvas_imageframe_start_handle_event(GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_imageframe_start_handle_event(item, event, data);
}
gint
PublicEditor::canvas_imageframe_end_handle_event(GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_imageframe_end_handle_event(item, event, data);
}
gint
PublicEditor::canvas_marker_time_axis_view_event(GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_marker_time_axis_view_event(item, event, data);
}
gint
PublicEditor::canvas_markerview_item_view_event(GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_markerview_item_view_event(item, event, data);
}
gint
PublicEditor::canvas_markerview_start_handle_event(GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_markerview_start_handle_event(item, event, data);
}
gint
PublicEditor::canvas_markerview_end_handle_event(GtkCanvasItem* item, GdkEvent* event, gpointer data) {
	return instance()->_canvas_markerview_end_handle_event(item, event, data);
}
