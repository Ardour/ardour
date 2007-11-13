// -*- c++ -*-
#ifndef _GLIBMM_MAIN_H
#define _GLIBMM_MAIN_H

/* $Id: main.h,v 1.9 2005/03/14 15:22:13 murrayc Exp $ */

/* Copyright (C) 2002 The gtkmm Development Team
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

#include <glib/giochannel.h>
#include <glib/gmain.h>

#include <vector>
#include <sigc++/sigc++.h>

#include <glibmmconfig.h>
#include <glibmm/refptr.h>
#include <glibmm/timeval.h>

GLIBMM_USING_STD(vector)


namespace Glib
{

class Cond;
class Mutex;
class IOChannel;


/** @defgroup MainLoop The Main Event Loop
 * Manages all available sources of events.
 * @{
 */

enum
{
  /*! Use this for high priority event sources.  It is not used within
   * GLib or GTK+.<br><br>
   */
  PRIORITY_HIGH = -100,

  /*! Use this for default priority event sources.  In glibmm this
   * priority is used by default when installing timeout handlers with
   * SignalTimeout::connect().  In GDK this priority is used for events
   * from the X server.<br><br>
   */
  PRIORITY_DEFAULT = 0,

  /*! Use this for high priority idle functions.  GTK+ uses
   * <tt>PRIORITY_HIGH_IDLE&nbsp;+&nbsp;10</tt> for resizing operations, and
   * <tt>PRIORITY_HIGH_IDLE&nbsp;+&nbsp;20</tt> for redrawing operations.
   * (This is done to ensure that any pending resizes are processed before
   * any pending redraws, so that widgets are not redrawn twice unnecessarily.)
   * <br><br>
   */
  PRIORITY_HIGH_IDLE = 100,

  /*! Use this for default priority idle functions.  In glibmm this priority is
   * used by default when installing idle handlers with SignalIdle::connect().
   * <br><br>
   */
  PRIORITY_DEFAULT_IDLE = 200,

  /*! Use this for very low priority background tasks.  It is not used within
   * GLib or GTK+.
   */
  PRIORITY_LOW = 300
};


/** A bitwise combination representing an I/O condition to watch for on an
 * event source.
 * The flags correspond to those used by the <tt>%poll()</tt> system call
 * on UNIX (see <tt>man 2 poll</tt>).  To test for individual flags, do
 * something like this:
 * @code
 * if((condition & Glib::IO_OUT) != 0)
 *   do_some_output();
 * @endcode
 * @par Bitwise operators:
 * <tt>IOCondition operator|(IOCondition, IOCondition)</tt><br>
 * <tt>IOCondition operator&(IOCondition, IOCondition)</tt><br>
 * <tt>IOCondition operator^(IOCondition, IOCondition)</tt><br>
 * <tt>IOCondition operator~(IOCondition)</tt><br>
 * <tt>IOCondition& operator|=(IOCondition&, IOCondition)</tt><br>
 * <tt>IOCondition& operator&=(IOCondition&, IOCondition)</tt><br>
 * <tt>IOCondition& operator^=(IOCondition&, IOCondition)</tt><br>
 */
enum IOCondition
{
  IO_IN   = G_IO_IN,  /*!< @hideinitializer There is data to read. */
  IO_OUT  = G_IO_OUT, /*!< @hideinitializer Data can be written (without blocking). */
  IO_PRI  = G_IO_PRI, /*!< @hideinitializer There is urgent data to read. */
  IO_ERR  = G_IO_ERR, /*!< @hideinitializer %Error condition. */
  IO_HUP  = G_IO_HUP, /*!< @hideinitializer Hung up (the connection has been broken,
                                            usually for pipes and sockets). */
  IO_NVAL = G_IO_NVAL /*!< @hideinitializer Invalid request. The file descriptor is not open. */
};

inline IOCondition operator|(IOCondition lhs, IOCondition rhs)
  { return static_cast<IOCondition>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs)); }

inline IOCondition operator&(IOCondition lhs, IOCondition rhs)
  { return static_cast<IOCondition>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs)); }

inline IOCondition operator^(IOCondition lhs, IOCondition rhs)
  { return static_cast<IOCondition>(static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs)); }

inline IOCondition operator~(IOCondition flags)
  { return static_cast<IOCondition>(~static_cast<unsigned>(flags)); }

inline IOCondition& operator|=(IOCondition& lhs, IOCondition rhs)
  { return (lhs = static_cast<IOCondition>(static_cast<unsigned>(lhs) | static_cast<unsigned>(rhs))); }

inline IOCondition& operator&=(IOCondition& lhs, IOCondition rhs)
  { return (lhs = static_cast<IOCondition>(static_cast<unsigned>(lhs) & static_cast<unsigned>(rhs))); }

inline IOCondition& operator^=(IOCondition& lhs, IOCondition rhs)
  { return (lhs = static_cast<IOCondition>(static_cast<unsigned>(lhs) ^ static_cast<unsigned>(rhs))); }


class PollFD
{
public:
  PollFD();
  explicit PollFD(int fd);
  PollFD(int fd, IOCondition events);

  void set_fd(int fd) { gobject_.fd = fd;   }
  int  get_fd() const { return gobject_.fd; }

  void set_events(IOCondition events)   { gobject_.events = events; }
  IOCondition get_events() const        { return static_cast<IOCondition>(gobject_.events); }

  void set_revents(IOCondition revents) { gobject_.revents = revents; }
  IOCondition get_revents() const       { return static_cast<IOCondition>(gobject_.revents); }

  GPollFD*       gobj()       { return &gobject_; }
  const GPollFD* gobj() const { return &gobject_; }

private:
  GPollFD gobject_;
};


class SignalTimeout
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  explicit inline SignalTimeout(GMainContext* context);
#endif

  /** Connects a timeout handler.
   * @code
   * Glib::signal_timeout().connect(sigc::ptr_fun(&timeout_handler), 1000);
   * @endcode
   * is equivalent to:
   * @code
   * const Glib::RefPtr<Glib::TimeoutSource> timeout_source = Glib::TimeoutSource::create(1000);
   * timeout_source->connect(sigc::ptr_fun(&timeout_handler));
   * timeout_source->attach(Glib::MainContext::get_default());
   * @endcode
   * @param slot A slot to call when @a interval elapsed.
   * @param interval The timeout in milliseconds.
   * @param priority The priority of the new event source.
   * @return A connection handle, which can be used to disconnect the handler.
   */
  sigc::connection connect(const sigc::slot<bool>& slot, unsigned int interval,
                           int priority = PRIORITY_DEFAULT);
private:
  GMainContext* context_;

  // no copy assignment
  SignalTimeout& operator=(const SignalTimeout&);
};


class SignalIdle
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  explicit inline SignalIdle(GMainContext* context);
#endif

  /** Connects an idle handler.
   * @code
   * Glib::signal_idle().connect(sigc::ptr_fun(&idle_handler));
   * @endcode
   * is equivalent to:
   * @code
   * const Glib::RefPtr<Glib::IdleSource> idle_source = Glib::IdleSource::create();
   * idle_source->connect(sigc::ptr_fun(&idle_handler));
   * idle_source->attach(Glib::MainContext::get_default());
   * @endcode
   * @param slot A slot to call when the main loop is idle.
   * @param priority The priority of the new event source.
   * @return A connection handle, which can be used to disconnect the handler.
   */
  sigc::connection connect(const sigc::slot<bool>& slot, int priority = PRIORITY_DEFAULT_IDLE);

private:
  GMainContext* context_;

  // no copy assignment
  SignalIdle& operator=(const SignalIdle&);
};


class SignalIO
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  explicit inline SignalIO(GMainContext* context);
#endif

  /** Connects an I/O handler.
   * @code
   * Glib::signal_io().connect(sigc::ptr_fun(&io_handler), fd, Glib::IO_IN | Glib::IO_HUP);
   * @endcode
   * is equivalent to:
   * @code
   * const Glib::RefPtr<Glib::IOSource> io_source = Glib::IOSource::create(fd, Glib::IO_IN | Glib::IO_HUP);
   * io_source->connect(sigc::ptr_fun(&io_handler));
   * io_source->attach(Glib::MainContext::get_default());
   * @endcode
   * @param slot A slot to call when polling @a fd results in an event that matches @a condition.
   * The event will be passed as a parameter to @a slot.
   * If @a io_handler returns <tt>false</tt> the signal is disconnected.
   * @param fd The file descriptor (or a @c HANDLE on Win32 systems) to watch.
   * @param condition The conditions to watch for.
   * @param priority The priority of the new event source.
   * @return A connection handle, which can be used to disconnect the handler.
   */
  sigc::connection connect(const sigc::slot<bool,IOCondition>& slot, int fd,
                           IOCondition condition, int priority = PRIORITY_DEFAULT);

  /** Connects an I/O channel.
   * @code
   * Glib::signal_io().connect(sigc::ptr_fun(&io_handler), channel, Glib::IO_IN | Glib::IO_HUP);
   * @endcode
   * is equivalent to:
   * @code
   * const Glib::RefPtr<Glib::IOSource> io_source = Glib::IOSource::create(channel, Glib::IO_IN | Glib::IO_HUP);
   * io_source->connect(sigc::ptr_fun(&io_handler));
   * io_source->attach(Glib::MainContext::get_default());
   * @endcode
   * @param slot A slot to call when polling @a channel results in an event that matches @a condition.
   * The event will be passed as a parameter to @a slot.
   * If @a io_handler returns <tt>false</tt> the signal is disconnected.
   * @param channel The IOChannel object to watch.
   * @param condition The conditions to watch for.
   * @param priority The priority of the new event source.
   * @return A connection handle, which can be used to disconnect the handler.
   */
  sigc::connection connect(const sigc::slot<bool,IOCondition>& slot, const Glib::RefPtr<IOChannel>& channel,
                           IOCondition condition, int priority = PRIORITY_DEFAULT);

private:
  GMainContext* context_;

  // no copy assignment
  SignalIO& operator=(const SignalIO&);
};

class SignalChildWatch
{
public:
#ifndef DOXYGEN_SHOULD_SKIP_THIS
  explicit inline SignalChildWatch(GMainContext* context);
#endif
  /** Connects a child watch handler.
   * @code
   * Glib::signal_child_watch().connect(sigc::ptr_fun(&child_watch_handler), pid);
   * @endcode
   * @param slot A slot to call when child @a pid exited.
   * @param pid The child to watch for.
   * @param priority The priority of the new event source.
   * @return A connection handle, which can be used to disconnect the handler.
   */
  sigc::connection connect(const sigc::slot<void,GPid, int>& slot, GPid pid, 
int priority = PRIORITY_DEFAULT);
private:
  GMainContext* context_;

  // no copy assignment
  SignalChildWatch& operator=(const SignalChildWatch&);
};

/** Convenience timeout signal.
 * @return A signal proxy; you want to use SignalTimeout::connect().
 */
SignalTimeout signal_timeout();

/** Convenience idle signal.
 * @return A signal proxy; you want to use SignalIdle::connect().
 */
SignalIdle signal_idle();

/** Convenience I/O signal.
 * @return A signal proxy; you want to use SignalIO::connect().
 */
SignalIO signal_io();

/** Convenience child watch signal.
 * @return A signal proxy; you want to use SignalChildWatch::connect().
 */
SignalChildWatch signal_child_watch();


/** Main context.
 */
class MainContext
{
public:
  typedef Glib::MainContext  CppObjectType;
  typedef GMainContext       BaseObjectType;

  /** Creates a new MainContext.
   * @return The new MainContext.
   */
  static Glib::RefPtr<MainContext> create();
  /** Returns the default main context. 
   * This is the main context used for main loop functions when a main loop is not explicitly specified.
   * @return The new MainContext.
   */
  static Glib::RefPtr<MainContext> get_default();

  /** Runs a single iteration for the given main loop. 
   * This involves checking to see if any event sources are ready to be processed, then if no events sources are 
   * ready and may_block is true, waiting for a source to become ready, then dispatching the highest priority events 
   * sources that are ready. Note that even when may_block is true, it is still possible for iteration() to return 
   * false, since the the wait may be interrupted for other reasons than an event source becoming ready.
   * @param may_block Whether the call may block.
   * @return true if events were dispatched.
   */
  bool iteration(bool may_block);

  /** Checks if any sources have pending events for the given context.
   * @return true if events are pending.
   */
  bool pending();

  /** If context is currently waiting in a poll(), interrupt the poll(), and continue the iteration process.
   */
  void wakeup();

  /** Tries to become the owner of the specified context. 
   * If some other context is the owner of the context, returns FALSE immediately. Ownership is properly recursive: 
   * the owner can require ownership again and will release ownership when release() is called as many times as 
   * acquire().
   * You must be the owner of a context before you can call prepare(), query(), check(), dispatch().
   * @return true if the operation succeeded, and this thread is now the owner of context.
   */
  bool acquire();


  /** Tries to become the owner of the specified context, as with acquire(). But if another thread is the owner, 
   * atomically drop mutex and wait on cond until that owner releases ownership or until cond is signaled, then try 
   * again (once) to become the owner.
   * @param cond A condition variable.
   * @param mutex A mutex, currently held.
   * @return true if the operation succeeded, and this thread is now the owner of context.
   */
  bool wait(Glib::Cond& cond, Glib::Mutex& mutex);

  /** Releases ownership of a context previously acquired by this thread with acquire(). If the context was acquired 
   * multiple times, the only release ownership when release() is called as many times as it was acquired.
   */
  void release();



  /** Prepares to poll sources within a main loop. The resulting information for polling is determined by calling query().
   * @param priority Location to store priority of highest priority source already ready.
   * @return true if some source is ready to be dispatched prior to polling.
   */
  bool prepare(int& priority);
  /** Prepares to poll sources within a main loop. The resulting information for polling is determined by calling query().
   * @return true if some source is ready to be dispatched prior to polling.
   */
  bool prepare();

  /** Determines information necessary to poll this main loop.
   * @param max_priority Maximum priority source to check.
   * @param timeout Location to store timeout to be used in polling.
   * @param fds Location to store Glib::PollFD records that need to be polled.
   * @return the number of records actually stored in fds, or, if more than n_fds records need to be stored, the number of records that need to be stored.
   */
  void query(int max_priority, int& timeout, std::vector<PollFD>& fds);

  /** Passes the results of polling back to the main loop.
   * @param max_priority Maximum numerical priority of sources to check.
   * @param fds Vector of Glib::PollFD's that was passed to the last call to query()
   * @return true if some sources are ready to be dispatched.
   */
  bool check(int max_priority, std::vector<PollFD>& fds);

  /** Dispatches all pending sources.
   */
  void dispatch();

  //TODO: Use slot instead?
  /** Sets the function to use to handle polling of file descriptors. It will be used instead of the poll() system call (or GLib's replacement function, which is used where poll() isn't available).
   * This function could possibly be used to integrate the GLib event loop with an external event loop.
   * @param poll_func The function to call to poll all file descriptors.
   */
  void set_poll_func(GPollFunc poll_func);

  /** Gets the poll function set by g_main_context_set_poll_func().
   * @return The poll function
   */
  GPollFunc get_poll_func();

  /** Adds a file descriptor to the set of file descriptors polled for this context. This will very seldomly be used directly. Instead a typical event source will use Glib::Source::add_poll() instead.
   * @param fd A PollFD structure holding information about a file descriptor to watch.
   * @param priority The priority for this file descriptor which should be the same as the priority used for Glib::Source::attach() to ensure that the file descriptor is polled whenever the results may be needed.
   */
  void add_poll(PollFD& fd, int priority);
  
  /** Removes file descriptor from the set of file descriptors to be polled for a particular context.
   * @param fd A PollFD structure holding information about a file descriptor.
   */
  void remove_poll(PollFD& fd);

  /** Timeout signal, attached to this MainContext.
   * @return A signal proxy; you want to use SignalTimeout::connect().
   */
  SignalTimeout signal_timeout();

  /** Idle signal, attached to this MainContext.
   * @return A signal proxy; you want to use SignalIdle::connect().
   */
  SignalIdle signal_idle();

  /** I/O signal, attached to this MainContext.
   * @return A signal proxy; you want to use SignalIO::connect().
   */
  SignalIO signal_io();

  /** child watch signal, attached to this MainContext.
   * @return A signal proxy; you want to use SignalChildWatch::connect().
   */
  SignalChildWatch signal_child_watch();

  void reference()   const;
  void unreference() const;

  GMainContext*       gobj();
  const GMainContext* gobj() const;
  GMainContext*       gobj_copy() const;

private:
  // Glib::MainContext can neither be constructed nor deleted.
  MainContext();
  void operator delete(void*, size_t);

  // noncopyable
  MainContext(const MainContext& other);
  MainContext& operator=(const MainContext& other);

};

/** @relates Glib::MainContext */
Glib::RefPtr<MainContext> wrap(GMainContext* gobject, bool take_copy = false);


class MainLoop
{
public:
  typedef Glib::MainLoop  CppObjectType;
  typedef GMainLoop       BaseObjectType;

  static Glib::RefPtr<MainLoop> create(bool is_running = false);
  static Glib::RefPtr<MainLoop> create(const Glib::RefPtr<MainContext>& context,
                                       bool is_running = false);

  /** Runs a main loop until quit() is called on the loop. 
   * If this is called for the thread of the loop's MainContext, it will process events from the loop, otherwise it will simply wait.
   */
  void run();

  /** Stops a MainLoop from running. Any calls to run() for the loop will return.
   */
  void quit();

  /** Checks to see if the main loop is currently being run via run().
   * @return true if the mainloop is currently being run.
   */
  bool is_running();

  /** Returns the MainContext of loop.
   * @return The MainContext of loop.
   */
  Glib::RefPtr<MainContext> get_context();

  //TODO: C++ize the (big) g_main_depth docs here.
  static int depth();

  /** Increases the reference count on a MainLoop object by one.
   */
  void reference()   const;

  /** Decreases the reference count on a MainLoop object by one. 
   * If the result is zero, free the loop and free all associated memory.
   */
  void unreference() const;

  GMainLoop*       gobj();
  const GMainLoop* gobj() const;
  GMainLoop*       gobj_copy() const;

private:
  // Glib::MainLoop can neither be constructed nor deleted.
  MainLoop();
  void operator delete(void*, size_t);

  MainLoop(const MainLoop&);
  MainLoop& operator=(const MainLoop&);
};

/** @relates Glib::MainLoop */
Glib::RefPtr<MainLoop> wrap(GMainLoop* gobject, bool take_copy = false);


class Source
{
public:
  typedef Glib::Source  CppObjectType;
  typedef GSource       BaseObjectType;

  static Glib::RefPtr<Source> create() /* = 0 */;

  /** Adds a Source to a context so that it will be executed within that context.
   * @param context A MainContext.
   * @return The ID for the source within the MainContext.
   */
  unsigned int attach(const Glib::RefPtr<MainContext>& context);

  /** Adds a Source to a context so that it will be executed within that context.
   * The default context will be used.
   * @return The ID for the source within the MainContext.
   */
  unsigned int attach();

  //TODO: Does this destroy step make sense in C++? Should it just be something that happens in a destructor?
  
  /** Removes a source from its MainContext, if any, and marks it as destroyed. 
   * The source cannot be subsequently added to another context.
   */
  void destroy();

  /** Sets the priority of a source. While the main loop is being run, a source will be dispatched if it is ready to be dispatched and no sources at a higher (numerically smaller) priority are ready to be dispatched.
   * @param priority The new priority.
   */
  void set_priority(int priority);
  
  /** Gets the priority of a source.
   * @return The priority of the source.
   */
  int  get_priority() const;

  /** Sets whether a source can be called recursively. 
   * If @a can_recurse is true, then while the source is being dispatched then this source will be processed normally. Otherwise, all processing of this source is blocked until the dispatch function returns.
   * @param can_recurse Whether recursion is allowed for this source.
   */
  void set_can_recurse(bool can_recurse);
  
  /** Checks whether a source is allowed to be called recursively. see set_can_recurse().
   * @return Whether recursion is allowed.
   */
  bool get_can_recurse() const;

  /** Returns the numeric ID for a particular source. 
   * The ID of a source is unique within a particular main loop context. The reverse mapping from ID to source is done by MainContext::find_source_by_id().
   * @return The ID for the source.
   */
  unsigned int get_id() const;

  //TODO: Add a const version of this method?
  /** Gets the MainContext with which the source is associated. 
   * Calling this function on a destroyed source is an error.
   * @return The MainContext with which the source is associated, or a null RefPtr if the context has not yet been added to a source. 
   */
  Glib::RefPtr<MainContext> get_context();

  GSource*       gobj()       { return gobject_; }
  const GSource* gobj() const { return gobject_; }
  GSource*       gobj_copy() const;

  void reference()   const;
  void unreference() const;

protected:
  /** Construct an object that uses the virtual functions prepare(), check() and dispatch().
   */
  Source();

  /** Wrap an existing GSource object and install the given callback function.
   * The constructed object doesn't use the virtual functions prepare(), check() and dispatch().
   * This constructor is for use by derived types that need to wrap a GSource object.
   * The callback function can be a static member function. But beware -
   * depending on the actual implementation of the GSource's virtual functions
   * the expected type of the callback function can differ from GSourceFunc.
   */
  Source(GSource* cast_item, GSourceFunc callback_func);

  virtual ~Source();

  sigc::connection connect_generic(const sigc::slot_base& slot);

  /** Adds a file descriptor to the set of file descriptors polled for this source. 
   * The event source's check function will typically test the revents field in the PollFD  and return true if events need to be processed.
   * @param poll_fd A PollFD object holding information about a file descriptor to watch.
   */
  void add_poll(PollFD& poll_fd);
  
  /** Removes a file descriptor from the set of file descriptors polled for this source.
   * @param poll_fd A PollFD object previously passed to add_poll(). 
   */
  void remove_poll(PollFD& poll_fd);

  /** Gets the "current time" to be used when checking this source. The advantage of calling this function over calling get_current_time() directly is that when checking multiple sources, GLib can cache a single value instead of having to repeatedly get the system time.
   * @param current_time Glib::TimeVal in which to store current time
   */
  void get_current_time(Glib::TimeVal& current_time);

  virtual bool prepare(int& timeout) = 0;
  virtual bool check() = 0;
  virtual bool dispatch(sigc::slot_base* slot) = 0;

private:
  GSource* gobject_;

#ifndef DOXGEN_SHOULD_SKIP_THIS

  static inline Source* get_wrapper(GSource* source);

  static const GSourceFuncs vfunc_table_;

  static gboolean prepare_vfunc(GSource* source, int* timeout);
  static gboolean check_vfunc(GSource* source);
  static gboolean dispatch_vfunc(GSource* source, GSourceFunc callback, void* user_data);
public:
  static void destroy_notify_callback(void* data);
private:

#endif /* DOXGEN_SHOULD_SKIP_THIS */

  // noncopyable
  Source(const Source&);
  Source& operator=(const Source&);
};


class TimeoutSource : public Glib::Source
{
public:
  typedef Glib::TimeoutSource CppObjectType;

  static Glib::RefPtr<TimeoutSource> create(unsigned int interval);
  sigc::connection connect(const sigc::slot<bool>& slot);

protected:
  explicit TimeoutSource(unsigned int interval);
  virtual ~TimeoutSource();

  virtual bool prepare(int& timeout);
  virtual bool check();
  virtual bool dispatch(sigc::slot_base* slot);

private:
  Glib::TimeVal expiration_;
  unsigned int  interval_;
};


class IdleSource : public Glib::Source
{
public:
  typedef Glib::IdleSource CppObjectType;

  static Glib::RefPtr<IdleSource> create();
  sigc::connection connect(const sigc::slot<bool>& slot);

protected:
  IdleSource();
  virtual ~IdleSource();

  virtual bool prepare(int& timeout);
  virtual bool check();
  virtual bool dispatch(sigc::slot_base* slot_data);
};


class IOSource : public Glib::Source
{
public:
  typedef Glib::IOSource CppObjectType;

  static Glib::RefPtr<IOSource> create(int fd, IOCondition condition);
  static Glib::RefPtr<IOSource> create(const Glib::RefPtr<IOChannel>& channel, IOCondition condition);
  sigc::connection connect(const sigc::slot<bool,IOCondition>& slot);

protected:
  IOSource(int fd, IOCondition condition);
  IOSource(const Glib::RefPtr<IOChannel>& channel, IOCondition condition);
  virtual ~IOSource();

  virtual bool prepare(int& timeout);
  virtual bool check();
  virtual bool dispatch(sigc::slot_base* slot);

private:
  PollFD poll_fd_;
};

/** @} group MainLoop */

} // namespace Glib


#endif /* _GLIBMM_MAIN_H */

