/*
 * Glib::Dispatcher example -- cross thread signalling
 * by Daniel Elstner  <daniel.elstner@gmx.net>
 *
 * modified to only use glibmm
 * by J. Abelardo Gutierrez <jabelardo@cantv.net>
 *
 * Copyright (c) 2002-2003  Free Software Foundation
 */

#include <glibmm.h>

#include <algorithm>
#include <functional>
#include <iostream>
#include <list>
#include <memory>


namespace
{

class ThreadProgress : public sigc::trackable
{
public:
  explicit ThreadProgress(int id);
  virtual ~ThreadProgress();

  void launch();
  void join();

  sigc::signal<void>& signal_finished();
  int id() const;

  virtual void reference() const { ++ref_count_; }
  virtual void unreference() const { if (!(--ref_count_)) delete this; }

private:
  Glib::Thread*       thread_;
  int                 id_;
  unsigned int        progress_;
  Glib::Dispatcher    signal_increment_;
  sigc::signal<void>  signal_finished_;

  void progress_increment();
  void thread_function();

  mutable int ref_count_;
 
};

class Application : public sigc::trackable
{
public:
  Application();
  virtual ~Application();

  void launch_threads();
  void run();

private:
  Glib::RefPtr<Glib::MainLoop>  main_loop_;
  std::list<ThreadProgress*>    progress_list_;
  std::list<Glib::RefPtr<ThreadProgress> > progress_ref_list_;

  void on_progress_finished(ThreadProgress* thread_progress);
};


ThreadProgress::ThreadProgress(int id)
:
  thread_   (0),
  id_       (id),
  progress_ (0),
  ref_count_(0)
{
  // Increment the reference count
  reference();
  // Connect to the cross-thread signal.
  signal_increment_.connect(sigc::mem_fun(*this, &ThreadProgress::progress_increment));
}

ThreadProgress::~ThreadProgress()
{}

void ThreadProgress::launch()
{
  // Create a joinable thread.
  thread_ = Glib::Thread::create(sigc::mem_fun(*this, &ThreadProgress::thread_function), true);
}

void ThreadProgress::join()
{
  thread_->join();
}

sigc::signal<void>& ThreadProgress::signal_finished()
{
  return signal_finished_;
}

int ThreadProgress::id() const
{
  return id_;
}

void ThreadProgress::progress_increment()
{
  // Use an integer because floating point arithmetic is inaccurate --
  // we want to finish *exactly* after the 100th increment.
  ++progress_;

  std::cout << "Thread " << id_ << ": " << progress_ << '%' << std::endl;

  if(progress_ >= 100)
    signal_finished_();
}

void ThreadProgress::thread_function()
{
  Glib::Rand rand;
  int usecs = 5000;

  for(int i = 0; i < 100; ++i)
  {
    usecs = rand.get_int_range(std::max(0, usecs - 1000 - i), std::min(20000, usecs + 1000 + i));
    Glib::usleep(usecs);

    // Tell the main thread to increment the progress value.
    signal_increment_();
  }
}

Application::Application()
:
  main_loop_ (Glib::MainLoop::create())
{
  std::cout << "Thread Dispatcher Example." << std::endl;

  for(int i = 1; i <= 5; ++i)
  {
    ThreadProgress* progress=new ThreadProgress(i);
    progress_list_.push_back(progress);
    progress_ref_list_.push_back(Glib::RefPtr<ThreadProgress>(progress));

    progress->signal_finished().connect(
        sigc::bind<1>(sigc::mem_fun(*this, &Application::on_progress_finished), progress));
  }
}

Application::~Application()
{
}

void Application::launch_threads()
{
  std::for_each(progress_list_.begin(), progress_list_.end(),
                std::mem_fun(&ThreadProgress::launch));
}

void Application::run()
{
  main_loop_->run();
}

void Application::on_progress_finished(ThreadProgress* thread_progress)
{
  {
    progress_list_.remove(thread_progress);
    thread_progress->join();

    std::cout << "Thread " << thread_progress->id() 
              << ": finished." << std::endl;
  }

  if(progress_list_.empty())
    main_loop_->quit();
}

} // anonymous namespace


int main(int, char**)
{
  Glib::thread_init();

  Application application;

  // Install a one-shot idle handler to launch the threads
  Glib::signal_idle().connect(
      sigc::bind_return(sigc::mem_fun(application, &Application::launch_threads), false));

  application.run();

  return 0;
}

