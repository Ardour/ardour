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

#ifndef _RUBBERBAND_WINDOW_H_
#define _RUBBERBAND_WINDOW_H_

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <cstdlib>
#include <map>

#include "sysutils.h"

namespace RubberBand {

enum WindowType {
    RectangularWindow,
    BartlettWindow,
    HammingWindow,
    HanningWindow,
    BlackmanWindow,
    GaussianWindow,
    ParzenWindow,
    NuttallWindow,
    BlackmanHarrisWindow
};

template <typename T>
class Window
{
public:
    /**
     * Construct a windower of the given type.
     */
    Window(WindowType type, int size) : m_type(type), m_size(size) { encache(); }
    Window(const Window &w) : m_type(w.m_type), m_size(w.m_size) { encache(); }
    Window &operator=(const Window &w) {
	if (&w == this) return *this;
	m_type = w.m_type;
	m_size = w.m_size;
	encache();
	return *this;
    }
    virtual ~Window() { delete[] m_cache; }
    
    void cut(T *R__ src) const
    {
        const int sz = m_size;
        for (int i = 0; i < sz; ++i) {
            src[i] *= m_cache[i];
        }
    }

    void cut(T *R__ src, T *dst) const {
        const int sz = m_size;
	for (int i = 0; i < sz; ++i) {
            dst[i] = src[i];
        }
	for (int i = 0; i < sz; ++i) {
            dst[i] *= m_cache[i];
        }
    }

    T getArea() { return m_area; }
    T getValue(int i) { return m_cache[i]; }

    WindowType getType() const { return m_type; }
    int getSize() const { return m_size; }

protected:
    WindowType m_type;
    int m_size;
    T *R__ m_cache;
    T m_area;
    
    void encache();
    void cosinewin(T *, T, T, T, T);
};

template <typename T>
void Window<T>::encache()
{
    int n = int(m_size);
    T *mult = new T[n];
    int i;
    for (i = 0; i < n; ++i) mult[i] = 1.0;

    switch (m_type) {
		
    case RectangularWindow:
	for (i = 0; i < n; ++i) {
	    mult[i] *= 0.5;
	}
	break;
	    
    case BartlettWindow:
	for (i = 0; i < n/2; ++i) {
	    mult[i] *= (i / T(n/2));
	    mult[i + n/2] *= (1.0 - (i / T(n/2)));
	}
	break;
	    
    case HammingWindow:
        cosinewin(mult, 0.54, 0.46, 0.0, 0.0);
	break;
	    
    case HanningWindow:
        cosinewin(mult, 0.50, 0.50, 0.0, 0.0);
	break;
	    
    case BlackmanWindow:
        cosinewin(mult, 0.42, 0.50, 0.08, 0.0);
	break;
	    
    case GaussianWindow:
	for (i = 0; i < n; ++i) {
            mult[i] *= pow(2, - pow((i - (n-1)/2.0) / ((n-1)/2.0 / 3), 2));
	}
	break;
	    
    case ParzenWindow:
    {
        int N = n-1;
        for (i = 0; i < N/4; ++i) {
            T m = 2 * pow(1.0 - (T(N)/2 - i) / (T(N)/2), 3);
            mult[i] *= m;
            mult[N-i] *= m;
        }
        for (i = N/4; i <= N/2; ++i) {
            int wn = i - N/2;
            T m = 1.0 - 6 * pow(wn / (T(N)/2), 2) * (1.0 - abs(wn) / (T(N)/2));
            mult[i] *= m;
            mult[N-i] *= m;
        }            
        break;
    }

    case NuttallWindow:
        cosinewin(mult, 0.3635819, 0.4891775, 0.1365995, 0.0106411);
	break;

    case BlackmanHarrisWindow:
        cosinewin(mult, 0.35875, 0.48829, 0.14128, 0.01168);
        break;
    }
	
    m_cache = mult;

    m_area = 0;
    for (int i = 0; i < n; ++i) {
        m_area += m_cache[i];
    }
    m_area /= n;
}

template <typename T>
void Window<T>::cosinewin(T *mult, T a0, T a1, T a2, T a3)
{
    int n = int(m_size);
    for (int i = 0; i < n; ++i) {
        mult[i] *= (a0
                    - a1 * cos(2 * M_PI * i / n)
                    + a2 * cos(4 * M_PI * i / n)
                    - a3 * cos(6 * M_PI * i / n));
    }
}

}

#endif
