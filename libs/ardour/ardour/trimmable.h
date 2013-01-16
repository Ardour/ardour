/*
    Copyright (C) 2012 Paul Davis 

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

#ifndef __libardour_trimmable_h__
#define __libardour_trimmable_h__

namespace ARDOUR {

class Trimmable {
  public:
	Trimmable() {}
	virtual ~Trimmable() {}

	enum CanTrim {
		FrontTrimEarlier = 0x1,
		FrontTrimLater = 0x2,
		EndTrimEarlier = 0x4,
		EndTrimLater = 0x8,
		TopTrimUp = 0x10,
		TopTrimDown = 0x20,
		BottomTrimUp = 0x40,
		BottomTrimDown = 0x80
	} ;

	virtual CanTrim can_trim() const {
		return CanTrim (FrontTrimEarlier |
		                FrontTrimLater |
		                EndTrimEarlier |
		                EndTrimLater |
		                TopTrimUp |
		                TopTrimDown |
		                BottomTrimUp |
		                BottomTrimDown);
	}
};

}

#endif /* __libardour_trimmable_h__ */
