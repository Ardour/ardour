// -*- c++ -*-
/* $Id$ */

/* Copyright 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/dispatcher.h>
#include <glibmm/exceptionhandler.h>
#include <glibmm/fileutils.h>
#include <glibmm/main.h>
#include <glibmm/thread.h>

#include <cerrno>
#include <fcntl.h>
#include <glib.h>

#ifndef G_OS_WIN32
#include <unistd.h>

#if defined(_tru64) //TODO: Use the real define
//EINTR is not defined on Tru64
//I have tried including these
//#include <sys/types.h>
//#include <sys/statvfs.h>
//#include <signal.h>
  #ifndef EINTR
  #define EINTR 0
  #endif
#endif

#else
#include <windows.h>
#include <io.h>
#include <direct.h>
#include <list>
#endif /* G_OS_WIN32 */


namespace
{

struct DispatchNotifyData
{
  unsigned long           tag;
  Glib::Dispatcher*       dispatcher;
  Glib::DispatchNotifier* notifier;

  DispatchNotifyData()
    : tag (0), dispatcher (0), notifier (0) {}

  DispatchNotifyData(unsigned long tag_, Glib::Dispatcher* dispatcher_, Glib::DispatchNotifier* notifier_)
    : tag (tag_), dispatcher (dispatcher_), notifier (notifier_) {}
};

static void warn_failed_pipe_io(const char* what, int err_no)
{
#ifdef G_OS_WIN32
  const char *const message = g_win32_error_message(err_no);
#else
  const char *const message = g_strerror(err_no);
#endif
  g_critical("Error in inter-thread communication: %s() failed: %s", what, message);
}

#ifndef G_OS_WIN32
/*
 * Try to set the close-on-exec flag of the file descriptor,
 * so that it won't be leaked if a new process is spawned.
 */
static void fd_set_close_on_exec(int fd)
{
  const int flags = fcntl(fd, F_GETFD, 0);
  g_return_if_fail(flags >= 0);

  fcntl(fd, F_SETFD, flags | FD_CLOEXEC);
}
#endif /* !G_OS_WIN32 */

/*
 * One word: paranoia.
 */
#ifdef G_OS_WIN32
static void fd_close_and_invalidate(HANDLE& fd)
{
  if(fd != 0)
  {
    if(!CloseHandle(fd))
      warn_failed_pipe_io("CloseHandle", GetLastError());

    fd = 0;
  }
}
#else /* !G_OS_WIN32 */
static void fd_close_and_invalidate(int& fd)
{
  if(fd >= 0)
  {
    int result;

    do
      result = close(fd);
    while(result < 0 && errno == EINTR);

    if(result < 0)
      warn_failed_pipe_io("close", errno);

    fd = -1;
  }
}
#endif /* !G_OS_WIN32 */

} // anonymous namespace


namespace Glib
{

class DispatchNotifier
{
public:
  ~DispatchNotifier();

  static DispatchNotifier* reference_instance(const Glib::RefPtr<MainContext>& context);
  static void unreference_instance(DispatchNotifier* notifier);

  void send_notification(Dispatcher* dispatcher);

protected:
  // Only used by reference_instance().  Should be private, but that triggers
  // a silly gcc warning even though DispatchNotifier has static methods.
  explicit DispatchNotifier(const Glib::RefPtr<MainContext>& context);

private:
  static Glib::StaticPrivate<DispatchNotifier> thread_specific_instance_;

  Glib::RefPtr<MainContext> context_;
  int                       ref_count_;
#ifdef G_OS_WIN32
  HANDLE                        fd_receiver_;
  Glib::Mutex                   mutex_;
  std::list<DispatchNotifyData> notify_queue_;
#else
  int                       fd_receiver_;
  int                       fd_sender_;
#endif /* !G_OS_WIN32 */
  sigc::connection          conn_io_handler_;

  void create_pipe();
  bool pipe_io_handler(Glib::IOCondition condition);

  // noncopyable
  DispatchNotifier(const DispatchNotifier&);
  DispatchNotifier& operator=(const DispatchNotifier&);
};


/**** Glib::DispatchNotifier ***********************************************/

Glib::StaticPrivate<DispatchNotifier>
DispatchNotifier::thread_specific_instance_ = GLIBMM_STATIC_PRIVATE_INIT;

DispatchNotifier::DispatchNotifier(const Glib::RefPtr<MainContext>& context)
:
  context_      (context),
  ref_count_    (0),
#ifdef G_OS_WIN32
  fd_receiver_  (0),
  mutex_        (),
  notify_queue_ ()
#else
  fd_receiver_  (-1),
  fd_sender_    (-1)
#endif
{
  create_pipe();

  try
  {
#ifdef G_OS_WIN32
    conn_io_handler_ = context_->signal_io().connect(
        sigc::mem_fun(*this, &DispatchNotifier::pipe_io_handler),
        GPOINTER_TO_INT(fd_receiver_), Glib::IO_IN);
#else /* !G_OS_WIN32 */
    conn_io_handler_ = context_->signal_io().connect(
        sigc::mem_fun(*this, &DispatchNotifier::pipe_io_handler),
        fd_receiver_, Glib::IO_IN);
#endif /* !G_OS_WIN32 */
  }
  catch(...)
  {
#ifndef G_OS_WIN32
    fd_close_and_invalidate(fd_sender_);
#endif /* !G_OS_WIN32 */
    fd_close_and_invalidate(fd_receiver_);

    throw;
  }
}

DispatchNotifier::~DispatchNotifier()
{
  // Disconnect manually because we don't inherit from sigc::trackable
  conn_io_handler_.disconnect();

#ifndef G_OS_WIN32
  fd_close_and_invalidate(fd_sender_);
#endif /* !G_OS_WIN32 */
  fd_close_and_invalidate(fd_receiver_);
}

void DispatchNotifier::create_pipe()
{
#ifdef G_OS_WIN32
  // On Win32 we are using synchronization object instead of pipe
  // thus storing its handle as fd_receiver_.
  fd_receiver_ = CreateEvent(0, FALSE, FALSE, 0);

  if(!fd_receiver_)
  {
    GError* const error = g_error_new(G_FILE_ERROR, G_FILE_ERROR_FAILED,
                                      "Failed to create event for inter-thread communication: %s",
                                      g_win32_error_message(GetLastError()));
    throw Glib::FileError(error);
  }
#else /* !G_OS_WIN32 */
  int filedes[2] = { -1, -1 };

  if(pipe(filedes) < 0)
  {
    GError* const error = g_error_new(G_FILE_ERROR, g_file_error_from_errno(errno),
                                      "Failed to create pipe for inter-thread communication: %s",
                                      g_strerror(errno));
    throw Glib::FileError(error);
  }

  fd_set_close_on_exec(filedes[0]);
  fd_set_close_on_exec(filedes[1]);

  fd_receiver_ = filedes[0];
  fd_sender_   = filedes[1];
#endif /* !G_OS_WIN32 */
}

// static
DispatchNotifier* DispatchNotifier::reference_instance(const Glib::RefPtr<MainContext>& context)
{
  DispatchNotifier* instance = thread_specific_instance_.get();

  if(!instance)
  {
    instance = new DispatchNotifier(context);
    thread_specific_instance_.set(instance);
  }
  else
  {
    // Prevent massive mess-up.
    g_return_val_if_fail(instance->context_ == context, 0);
  }

  ++instance->ref_count_; // initially 0

  return instance;
}

// static
void DispatchNotifier::unreference_instance(DispatchNotifier* notifier)
{
  DispatchNotifier *const instance = thread_specific_instance_.get();

  // Yes, the notifier argument is only used to check for sanity.
  g_return_if_fail(instance == notifier);

  if(--instance->ref_count_ <= 0)
  {
    g_return_if_fail(instance->ref_count_ == 0); // could be < 0 if messed up

    // This will cause deletion of the notifier object.
    thread_specific_instance_.set(0);
  }
}

void DispatchNotifier::send_notification(Dispatcher* dispatcher)
{
#ifdef G_OS_WIN32
  {
    Glib::Mutex::Lock lock (mutex_);
    notify_queue_.push_back(DispatchNotifyData(0xdeadbeef, dispatcher, this));
  }

  // Send notification event to GUI-thread.
  if(!SetEvent(fd_receiver_))
  {
    warn_failed_pipe_io("SetEvent", GetLastError());
    return;
  }
#else /* !G_OS_WIN32 */
  DispatchNotifyData data (0xdeadbeef, dispatcher, this);
  gssize n_written;

  do
    n_written = write(fd_sender_, &data, sizeof(data));
  while(n_written < 0 && errno == EINTR);

  if(n_written < 0)
  {
    warn_failed_pipe_io("write", errno);
    return;
  }

  // All data must be written in a single call to write(), otherwise we can't
  // guarantee reentrancy since another thread might be scheduled between two
  // write() calls.  The manpage is a bit unclear about this -- but I hope
  // it's safe to assume immediate success for the tiny amount of data we're
  // writing.
  g_return_if_fail(n_written == sizeof(data));
#endif /* !G_OS_WIN32 */
}

bool DispatchNotifier::pipe_io_handler(Glib::IOCondition)
{
#ifdef G_OS_WIN32
  DispatchNotifyData data;

  for(;;)
  {
    {
      Glib::Mutex::Lock lock (mutex_);

      if(notify_queue_.empty())
        break;

      data = notify_queue_.front();
      notify_queue_.pop_front();
    }

    g_return_val_if_fail(data.tag == 0xdeadbeef, true);
    g_return_val_if_fail(data.notifier == this,  true);

    // Actually, we wouldn't need the try/catch block because the Glib::Source
    // C callback already does it for us.  However, we do it anyway because the
    // default return value is 'false', which is not what we want.
    try
    {
      data.dispatcher->signal_();
    }
    catch(...)
    {
      Glib::exception_handlers_invoke();
    }
  }
#else /* !G_OS_WIN32 */
  DispatchNotifyData data;
  gsize n_read = 0;

  do
  {
    void * const buffer = reinterpret_cast<guint8*>(&data) + n_read;
    const gssize result = read(fd_receiver_, buffer, sizeof(data) - n_read);

    if(result < 0)
    {
      if(errno == EINTR)
        continue;

      warn_failed_pipe_io("read", errno);
      return true;
    }

    n_read += result;
  }
  while(n_read < sizeof(data));

  g_return_val_if_fail(data.tag == 0xdeadbeef, true);
  g_return_val_if_fail(data.notifier == this,  true);

  // Actually, we wouldn't need the try/catch block because the Glib::Source
  // C callback already does it for us.  However, we do it anyway because the
  // default return value is 'false', which is not what we want.
  try
  {
    data.dispatcher->signal_(); // emit
  }
  catch(...)
  {
    Glib::exception_handlers_invoke();
  }
#endif /* !G_OS_WIN32 */

  return true;
}


/**** Glib::Dispatcher *****************************************************/

Dispatcher::Dispatcher()
:
  signal_   (),
  notifier_ (DispatchNotifier::reference_instance(MainContext::get_default()))
{}

Dispatcher::Dispatcher(const Glib::RefPtr<MainContext>& context)
:
  signal_   (),
  notifier_ (DispatchNotifier::reference_instance(context))
{}

Dispatcher::~Dispatcher()
{
  DispatchNotifier::unreference_instance(notifier_);
}

void Dispatcher::emit()
{
  notifier_->send_notification(this);
}

void Dispatcher::operator()()
{
  emit();
}

sigc::connection Dispatcher::connect(const sigc::slot<void>& slot)
{
  return signal_.connect(slot);
}

} // namespace Glib

