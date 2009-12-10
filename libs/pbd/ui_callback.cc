#include "pbd/ui_callback.h"

using namespace PBD;

Glib::StaticPrivate<UICallback> UICallback::thread_ui;

static void do_not_delete_the_ui_pointer (void*) { }

UICallback* 
UICallback::get_ui_for_thread() {
	return thread_ui.get ();
}

void 
UICallback::set_ui_for_thread (UICallback* ui) 
{
	thread_ui.set (ui, do_not_delete_the_ui_pointer); 
}

