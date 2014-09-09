/*
    Copyright (C) 2014 John Emmas 

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

#ifndef __waves_pthread_utils__
#define __waves_pthread_utils__

/* Accommodate thread setting (and testing) for both
 * 'libpthread' and 'libpthread_win32' (whose implementations
 * of 'pthread_t' are subtlely different)
 */
#ifndef PTHREAD_MACROS_DEFINED
#define PTHREAD_MACROS_DEFINED
#ifdef  PTW32_VERSION  /* pthread_win32 */
#define mark_pthread_inactive(threadID)  threadID.p=0
#define is_pthread_active(threadID)      threadID.p!=0
#else                 /* normal pthread */
#define mark_pthread_inactive(threadID)  threadID=0
#define is_pthread_active(threadID)      threadID!=0
#endif  /* PTW32_VERSION */

#endif  /* PTHREAD_MACROS_DEFINED */
#endif  /* __waves_pthread_utils__ */
