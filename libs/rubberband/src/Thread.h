/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2008 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _RUBBERBAND_THREAD_H_
#define _RUBBERBAND_THREAD_H_

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else /* !PLATFORM_WINDOWS */
#include <pthread.h>
#endif /* !PLATFORM_WINDOWS */

#include <string>

//#define DEBUG_THREAD 1
//#define DEBUG_MUTEX 1
//#define DEBUG_CONDITION 1

namespace RubberBand
{

class Thread
{
public:
#ifdef PLATFORM_WINDOWS
    typedef HANDLE Id;
#else
    typedef pthread_t Id;
#endif

    Thread();
    virtual ~Thread();

    Id id();

    void start();
    void wait();

    static bool threadingAvailable();

protected:
    virtual void run() = 0;

private:
#ifdef PLATFORM_WINDOWS
    HANDLE m_id;
    bool m_extant;
    static DWORD WINAPI staticRun(LPVOID lpParam);
#else
    pthread_t m_id;
    bool m_extant;
    static void *staticRun(void *);
#endif
};

class Mutex
{
public:
    Mutex();
    ~Mutex();

    void lock();
    void unlock();
    bool trylock();

private:
#ifdef PLATFORM_WINDOWS
    HANDLE m_mutex;
#ifndef NO_THREAD_CHECKS
    DWORD m_lockedBy;
#endif
#else
    pthread_mutex_t m_mutex;
#ifndef NO_THREAD_CHECKS
    pthread_t m_lockedBy;
    bool m_locked;
#endif
#endif
};

class MutexLocker
{
public:
    MutexLocker(Mutex *);
    ~MutexLocker();

private:
    Mutex *m_mutex;
};

class Condition
{
public:
    Condition(std::string name);
    ~Condition();
    
    // To wait on a condition, either simply call wait(), or call
    // lock() and then wait() (perhaps testing some state in between).
    // To signal a condition, call signal().

    // Although any thread may signal on a given condition, only one
    // thread should ever wait on any given condition object --
    // otherwise there will be a race conditions in the logic that
    // avoids the thread code having to track whether the condition's
    // mutex is locked or not.  If that is your requirement, this
    // Condition wrapper is not for you.
    void lock();
    void unlock();
    void wait(int us = 0);

    void signal();
    
private:

#ifdef PLATFORM_WINDOWS
    HANDLE m_mutex;
    HANDLE m_condition;
    bool m_locked;
#else
    pthread_mutex_t m_mutex;
    pthread_cond_t m_condition;
    bool m_locked;
#endif
#ifdef DEBUG_CONDITION
    std::string m_name;
#endif
};

}

#endif
