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

#include "sysutils.h"

#ifdef _WIN32
#include <windows.h>
#else /* !_WIN32 */
#ifdef __APPLE__
#include <sys/sysctl.h>
#else /* !__APPLE__, !_WIN32 */
#include <cstdio>
#include <cstring>
#endif /* !__APPLE__, !_WIN32 */
#endif /* !_WIN32 */

#include <cstdlib>
#include <iostream>

namespace RubberBand {

bool
system_is_multiprocessor()
{
    static bool tested = false, mp = false;

    if (tested) return mp;
    int count = 0;

#ifdef _WIN32

    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    count = sysinfo.dwNumberOfProcessors;

#else /* !_WIN32 */
#ifdef __APPLE__
    
    size_t sz = sizeof(count);
    if (sysctlbyname("hw.ncpu", &count, &sz, NULL, 0)) {
        mp = false;
    } else {
        mp = (count > 1);
    }

#else /* !__APPLE__, !_WIN32 */
    
    //...

    FILE *cpuinfo = fopen("/proc/cpuinfo", "r");
    if (!cpuinfo) return false;

    char buf[256];
    while (!feof(cpuinfo)) {
        fgets(buf, 256, cpuinfo);
        if (!strncmp(buf, "processor", 9)) {
            ++count;
        }
        if (count > 1) break;
    }

    fclose(cpuinfo);

#endif /* !__APPLE__, !_WIN32 */
#endif /* !_WIN32 */

    mp = (count > 1);
    tested = true;
    return mp;
}

#ifdef _WIN32

int gettimeofday(struct timeval *tv, void *tz)
{
    union { 
	long long ns100;  
	FILETIME ft; 
    } now; 
    
    ::GetSystemTimeAsFileTime(&now.ft); 
    tv->tv_usec = (long)((now.ns100 / 10LL) % 1000000LL); 
    tv->tv_sec = (long)((now.ns100 - 116444736000000000LL) / 10000000LL); 
    return 0;
}

void usleep(unsigned long usec)
{
    ::Sleep(usec == 0 ? 0 : usec < 1000 ? 1 : usec / 1000);
}

#endif


float *allocFloat(float *ptr, int count)
{
    if (ptr) free((void *)ptr);
    void *allocated;
#ifndef _WIN32
#ifndef __APPLE__
    if (posix_memalign(&allocated, 16, count * sizeof(float)))
#endif
#endif
        allocated = malloc(count * sizeof(float));
    for (int i = 0; i < count; ++i) ((float *)allocated)[i] = 0.f;
    return (float *)allocated;
}

float *allocFloat(int count)
{
    return allocFloat(0, count);
}

void freeFloat(float *ptr)
{
    if (ptr) free(ptr);
}
      
double *allocDouble(double *ptr, int count)
{
    if (ptr) free((void *)ptr);
    void *allocated;
#ifndef _WIN32
#ifndef __APPLE__
    if (posix_memalign(&allocated, 16, count * sizeof(double)))
#endif
#endif
        allocated = malloc(count * sizeof(double));
    for (int i = 0; i < count; ++i) ((double *)allocated)[i] = 0.f;
    return (double *)allocated;
}

double *allocDouble(int count)
{
    return allocDouble(0, count);
}

void freeDouble(double *ptr)
{
    if (ptr) free(ptr);
}
 

}



