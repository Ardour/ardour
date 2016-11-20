/*
    Copyright (C) 2009 Paul Davis
    Author: Hans Baier

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

#ifndef __SYSEX_H__
#define __SYSEX_H__

class MidiRegionView;

namespace ArdourCanvas {
	class Flag;
}

class SysEx
{
public:
	SysEx (
			MidiRegionView& region,
			ArdourCanvas::Container* parent,
			std::string&    text,
			double          height,
			double          x,
			double          y);

	~SysEx ();

	void hide ();
	void show ();

        ArdourCanvas::Item& item() const { return *_flag; }

private:
	bool event_handler (GdkEvent* ev);
	SysEx(const SysEx& rhs){}
	ArdourCanvas::Flag* _flag;
};

#endif /* __SYSEX_H__ */
