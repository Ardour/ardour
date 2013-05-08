/*
  Copyright (C) 2010 Paul Davis
  Author: Torben Hohn

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifdef WAF_BUILD
#include "libpbd-config.h"
#endif

#ifdef __linux__
#include <unistd.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <stddef.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#endif

#include "pbd/cpus.h"

uint32_t
hardware_concurrency()
{
#if defined(PTW32_VERSION) || defined(__hpux)
        return pthread_num_processors_np();
#elif defined(__APPLE__) || defined(__FreeBSD__)
        int count;
        size_t size=sizeof(count);
        return sysctlbyname("hw.physicalcpu",&count,&size,NULL,0)?0:count;
#elif defined(HAVE_UNISTD) && defined(_SC_NPROCESSORS_ONLN)
        int const count=sysconf(_SC_NPROCESSORS_ONLN);
        return (count>0)?count:0;
#else
        return 0;
#endif
}
