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

#ifndef _RUBBERBAND_SCAVENGER_H_
#define _RUBBERBAND_SCAVENGER_H_

#include <vector>
#include <list>
#include <iostream>

#ifndef WIN32
#include <sys/time.h>
#endif

#include "Thread.h"
#include "sysutils.h"

namespace RubberBand {

/**
 * A very simple class that facilitates running things like plugins
 * without locking, by collecting unwanted objects and deleting them
 * after a delay so as to be sure nobody's in the middle of using
 * them.  Requires scavenge() to be called regularly from a non-RT
 * thread.
 *
 * This is currently not at all suitable for large numbers of objects
 * -- it's just a quick hack for use with things like plugins.
 */

template <typename T>
class Scavenger
{
public:
    Scavenger(int sec = 2, int defaultObjectListSize = 200);
    ~Scavenger();

    /**
     * Call from an RT thread etc., to pass ownership of t to us.
     * Only one thread should be calling this on any given scavenger.
     */
    void claim(T *t);

    /**
     * Call from a non-RT thread.
     * Only one thread should be calling this on any given scavenger.
     */
    void scavenge(bool clearNow = false);

protected:
    typedef std::pair<T *, int> ObjectTimePair;
    typedef std::vector<ObjectTimePair> ObjectTimeList;
    ObjectTimeList m_objects;
    int m_sec;

    typedef std::list<T *> ObjectList;
    ObjectList m_excess;
    int m_lastExcess;
    Mutex m_excessMutex;
    void pushExcess(T *);
    void clearExcess(int);

    unsigned int m_claimed;
    unsigned int m_scavenged;
};

/**
 * A wrapper to permit arrays to be scavenged.
 */

template <typename T>
class ScavengerArrayWrapper
{
public:
    ScavengerArrayWrapper(T *array) : m_array(array) { }
    ~ScavengerArrayWrapper() { delete[] m_array; }

private:
    T *m_array;
};


template <typename T>
Scavenger<T>::Scavenger(int sec, int defaultObjectListSize) :
    m_objects(ObjectTimeList(defaultObjectListSize)),
    m_sec(sec),
    m_claimed(0),
    m_scavenged(0)
{
}

template <typename T>
Scavenger<T>::~Scavenger()
{
    if (m_scavenged < m_claimed) {
	for (size_t i = 0; i < m_objects.size(); ++i) {
	    ObjectTimePair &pair = m_objects[i];
	    if (pair.first != 0) {
		T *ot = pair.first;
		pair.first = 0;
		delete ot;
		++m_scavenged;
	    }
	}
    }

    clearExcess(0);
}

template <typename T>
void
Scavenger<T>::claim(T *t)
{
//    std::cerr << "Scavenger::claim(" << t << ")" << std::endl;

    struct timeval tv;
    (void)gettimeofday(&tv, 0);
    int sec = tv.tv_sec;

    for (size_t i = 0; i < m_objects.size(); ++i) {
	ObjectTimePair &pair = m_objects[i];
	if (pair.first == 0) {
	    pair.second = sec;
	    pair.first = t;
	    ++m_claimed;
	    return;
	}
    }

    std::cerr << "WARNING: Scavenger::claim(" << t << "): run out of slots, "
	      << "using non-RT-safe method" << std::endl;
    pushExcess(t);
}

template <typename T>
void
Scavenger<T>::scavenge(bool clearNow)
{
//    std::cerr << "Scavenger::scavenge: scavenged " << m_scavenged << ", claimed " << m_claimed << std::endl;

    if (m_scavenged >= m_claimed) return;
    
    struct timeval tv;
    (void)gettimeofday(&tv, 0);
    int sec = tv.tv_sec;

    for (size_t i = 0; i < m_objects.size(); ++i) {
	ObjectTimePair &pair = m_objects[i];
	if (clearNow ||
	    (pair.first != 0 && pair.second + m_sec < sec)) {
	    T *ot = pair.first;
	    pair.first = 0;
	    delete ot;
	    ++m_scavenged;
	}
    }

    if (sec > m_lastExcess + m_sec) {
        clearExcess(sec);
    }
}

template <typename T>
void
Scavenger<T>::pushExcess(T *t)
{
    m_excessMutex.lock();
    m_excess.push_back(t);
    struct timeval tv;
    (void)gettimeofday(&tv, 0);
    m_lastExcess = tv.tv_sec;
    m_excessMutex.unlock();
}

template <typename T>
void
Scavenger<T>::clearExcess(int sec)
{
    m_excessMutex.lock();
    for (typename ObjectList::iterator i = m_excess.begin();
	 i != m_excess.end(); ++i) {
	delete *i;
    }
    m_excess.clear();
    m_lastExcess = sec;
    m_excessMutex.unlock();
}

}

#endif
