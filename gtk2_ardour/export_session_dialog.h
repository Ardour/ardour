/*
    Copyright (C) 2006 Andre Raue

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

#ifndef __export_session_dialog_h__
#define __export_session_dialog_h__

#include "export_dialog.h"


class ExportSessionDialog : public ExportDialog 
{
  public:
	ExportSessionDialog (PublicEditor&);
  	void set_range (nframes_t start, nframes_t end);
  
  protected:
	void export_audio_data();
};


#endif // __export_session_dialog_h__
