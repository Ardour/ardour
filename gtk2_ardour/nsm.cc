
/*******************************************************************************/
/* Copyright (C) 2012 Jonathan Moore Liles                                     */
/*                                                                             */
/* This program is free software; you can redistribute it and/or modify it     */
/* under the terms of the GNU General Public License as published by the       */
/* Free Software Foundation; either version 2 of the License, or (at your      */
/* option) any later version.                                                  */
/*                                                                             */
/* This program is distributed in the hope that it will be useful, but WITHOUT */
/* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or       */
/* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for   */
/* more details.                                                               */
/*                                                                             */
/* You should have received a copy of the GNU General Public License along     */
/* with This program; see the file COPYING.  If not,write to the Free Software */
/* Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */
/*******************************************************************************/


#include "nsm.h"
#include "opts.h"
#include "ardour_ui.h"

#include <stdio.h>
#include <unistd.h>


NSM_Client::NSM_Client()
{
}

int
NSM_Client::command_save(char **out_msg)
{
    (void) out_msg;

    ARDOUR_UI::instance()->save_state();
    int r = ERR_OK;

    return r;
}

int
NSM_Client::command_open(const char *name,
                         const char */*display_name*/,
                         const char *client_id,
                         char **/*out_msg*/)
{
    int r = ERR_OK;

    ARDOUR_COMMAND_LINE::session_name = name;
    ARDOUR_COMMAND_LINE::jack_client_name = client_id;

    if (ARDOUR_UI::instance()->get_session_parameters(true, false, "")) {
        return ERR_GENERAL;
    }
    return r;
}
