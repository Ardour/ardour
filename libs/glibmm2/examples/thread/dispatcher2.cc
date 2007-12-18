/*
 * original Glib::Dispatcher example -- cross thread signalling
 * by Daniel Elstner  <daniel.elstner@gmx.net>
 * 
 * Modified by Stephan Puchegger <stephan.puchegger@ap.univie.ac.at>
 * to contain 2 mainloops in 2 different threads, that communicate 
 * via cross thread signalling in both directions. The timer thread
 * sends the UI thread a cross thread signal every second, which in turn
 * updates the label stating how many seconds have passed since the start
 * of the program.
 *
 * Modified by J. Abelardo Gutierrez <jabelardo@cantv.net>
 * to cast all gtkmm out and make it glimm only
 *
 * Note:  This example is special stuff that's seldomly needed by the
 * vast majority of applications.  Don't bother working out what this
 * code does unless you know for sure you need 2 main loops running in
 * 2 distinct main contexts.
 *
 * Copyright (c) 2002-2003  Free Software Foundation
 */

#include <glibmm.h>
#include <sstream>
#include <iostream>


namespace
{
Glib::RefPtr<Glib::MainLoop> main_loop;

class ThreadTimer : public sigc::trackable
{
public:
  ThreadTimer();
  ~ThreadTimer();

  void launch();
  void signal_finished_emit();
  void print() const;

  typedef sigc::signal<void> type_signal_end;
  static type_signal_end& signal_end();

private:
  unsigned int      time_;
  Glib::Dispatcher  signal_increment_;  
  Glib::Dispatcher* signal_finished_ptr_;

  Glib::Mutex       startup_mutex_;
  Glib::Cond        startup_cond_;
  Glib::Thread*     thread_;
  
  static type_signal_end signal_end_;

  void timer_increment();
  bool timeout_handler();
  static void finished_handler(Glib::RefPtr<Glib::MainLoop> mainloop);
  void thread_function();
};

//TODO: Rename to avoid confusion with Glib::Dispatcher. murrayc
class Dispatcher : public sigc::trackable
{
public:
  Dispatcher();

  void launch_thread();
  void end();

private:
  ThreadTimer* timer_;
};

ThreadTimer::ThreadTimer() 
:
  time_ (0),
  // Create a new dispatcher that is attached to the default main context,
  signal_increment_ (),
  // This pointer will be initialized later by the 2nd thread.
  signal_finished_ptr_ (NULL)
{
  // Connect the cross-thread signal.
  signal_increment_.connect(sigc::mem_fun(*this, &ThreadTimer::timer_increment));
}

ThreadTimer::~ThreadTimer()
{}

void ThreadTimer::launch()
{
  // Unfortunately, the thread creation has to be fully synchronized in
  // order to access the Dispatcher object instantiated by the 2nd thread.
  // So, let's do some kind of hand-shake using a mutex and a condition
  // variable.
  Glib::Mutex::Lock lock (startup_mutex_);

  // Create a joinable thread -- it needs to be joined, otherwise it's a memory leak.
  thread_ = Glib::Thread::create(
      sigc::mem_fun(*this, &ThreadTimer::thread_function), true);

  // Wait for the 2nd thread's startup notification.
  while(signal_finished_ptr_ == NULL)
    startup_cond_.wait(startup_mutex_);
}

void ThreadTimer::signal_finished_emit()
{
  // Cause the 2nd thread's main loop to quit.
  signal_finished_ptr_->emit();

  // wait for the thread to join
  if(thread_ != NULL)
    thread_->join();

  signal_finished_ptr_ = NULL;
}

void ThreadTimer::print() const
{
  std::cout << time_ << " seconds since start" << std::endl;
}

sigc::signal< void >& ThreadTimer::signal_end()
{
  return signal_end_;
}

void ThreadTimer::timer_increment()
{
  // another second has passed since the start of the program
  ++time_;
  print();

  if(time_ >= 10)
    signal_finished_emit();
}

// static
void ThreadTimer::finished_handler(Glib::RefPtr<Glib::MainLoop> mainloop)
{
  // quit the timer thread mainloop
  mainloop->quit();
  std::cout << "timer thread mainloop finished" << std::endl;
  ThreadTimer::signal_end().emit();
}

bool ThreadTimer::timeout_handler()
{
  // inform the printing thread that another second has passed
  signal_increment_();

  // this timer should stay alive
  return true;
}

void ThreadTimer::thread_function()
{
  // create a new Main Context
  Glib::RefPtr<Glib::MainContext> context = Glib::MainContext::create();
  // create a new Main Loop
  Glib::RefPtr<Glib::MainLoop> mainloop = Glib::MainLoop::create(context, true);

  // attach a timeout handler, that is called every second, to the
  // newly created MainContext
  context->signal_timeout().connect(sigc::mem_fun(*this, &ThreadTimer::timeout_handler), 1000);

  // We need to lock while creating the Dispatcher instance,
  // in order to ensure memory visibility.
  Glib::Mutex::Lock lock (startup_mutex_);

  // create a new dispatcher, that is connected to the newly
  // created MainContext
  Glib::Dispatcher signal_finished (context);
  
  signal_finished.connect(sigc::bind(sigc::ptr_fun(&ThreadTimer::finished_handler), mainloop));

  signal_finished_ptr_ = &signal_finished;

  // Tell the launcher thread that everything is in place now.
  startup_cond_.signal();
  lock.release();

  // start the mainloop
  mainloop->run();
}

// initialize static member:
ThreadTimer::type_signal_end ThreadTimer::signal_end_;

Dispatcher::Dispatcher()
: 
  timer_ (NULL)
{
  std::cout << "Thread Dispatcher Example #2" << std::endl;

  timer_ = new ThreadTimer();
  timer_->signal_end().connect(sigc::mem_fun(*this, &Dispatcher::end));
  timer_->print();
}

void Dispatcher::launch_thread()
{
  // launch the timer thread
  timer_->launch();
}

void Dispatcher::end()
{
  // quit the main mainloop
  main_loop->quit();
}

} // anonymous namespace


int main(int, char**)
{
  Glib::thread_init();
  main_loop = Glib::MainLoop::create();

  Dispatcher dispatcher;

  // Install a one-shot idle handler to launch the threads
  Glib::signal_idle().connect(
      sigc::bind_return(sigc::mem_fun(dispatcher, &Dispatcher::launch_thread), false));

  main_loop->run();

  return 0;
}

