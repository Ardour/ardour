#include <iostream>
#include "pbd/event_loop.h"
#include "pbd/stacktrace.h"

using namespace PBD;
using namespace std;

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

/** Called when a sigc::trackable that was connected to using the invalidator() macro
 *  is destroyed.
 */
void* 
EventLoop::invalidate_request (void* data)
{
        InvalidationRecord* ir = (InvalidationRecord*) data;

        if (ir->event_loop) {
                Glib::Mutex::Lock lm (ir->event_loop->slot_invalidation_mutex());
                if (ir->request) {
                        cerr << "Object deleted had outstanding event loop request, IR created @ "
                             << ir->file << ':' << ir->line
                             << endl;
                        ir->request->valid = false;
                        ir->request->invalidation = 0;
                } else {
                        cerr << "No queued request associated with object deletion from "
                             << ir->file << ':' << ir->line
                             << endl;
                        
                }

                delete ir;
        }

        return 0;
}

