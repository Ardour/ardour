/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __forkexec_h__
#define __forkexec_h__

#include <unistd.h>

#include "pbd/libpbd_visibility.h"

LIBPBD_API pid_t forkexec(char **argv, char **envp, int outpipe[2], int inpipe[2]);
LIBPBD_API pid_t forkexec_cmd(char *cmd, char **envp, int outpipe[2], int inpipe[2]);

#endif // __forkexec_h__
