#include "pbd/event_loop.h"

using namespace PBD;

Glib::StaticPrivate<EventLoop> EventLoop::thread_event_loop;

static void do_not_delete_the_loop_pointer (void*) { }

EventLoop* 
EventLoop::get_event_loop_for_thread() {
	return thread_event_loop.get ();
}

void 
EventLoop::set_event_loop_for_thread (EventLoop* loop) 
{
	thread_event_loop.set (loop, do_not_delete_the_loop_pointer); 
}

