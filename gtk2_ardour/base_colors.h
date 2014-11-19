/*
    Copyright (C) 2014 Paul Davis 

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

/* no guard #ifdef's - this file is intended to be included many times */

/* note that because the integer values are interpreted as RGBA, we must include a A component, which is just 0xff
   for all colors here. The alpha component is not used.
*/

CANVAS_BASE_COLOR(colorA,"colorA", 0xff0000ff) /* red */
CANVAS_BASE_COLOR(colorAlight,"colorAlight", 0xff00e6ff) /* pink */

/* oranges */

CANVAS_BASE_COLOR(colorAB,"colorAB", 0xff4d00ff) /* redder/darker orange */
CANVAS_BASE_COLOR(colorABlight,"colorABlight", 0xff9900ff) /* lighter orange */

/* yellows */
CANVAS_BASE_COLOR(colorB,"colorB", 0xffe600ff) /* bright yellow */

/* greens */
CANVAS_BASE_COLOR(colorA2,"colorC", 0x00ff1aff) /* darker green */
CANVAS_BASE_COLOR(colorClight,"colorClight", 0x80ff00ff) /* light green */

/* cyan */
CANVAS_BASE_COLOR(colorCD,"colorCD", 0x00ffb3ff) /* cyan */

/* blues */
CANVAS_BASE_COLOR(colorD,"colorD", 0x00ffffff) /* light blue */
CANVAS_BASE_COLOR(colorDdark,"colorDdark", 0x001affff) /* darkest blue */
CANVAS_BASE_COLOR(colorDlight,"colorDlight", 0x00b3ffff) /* lightest blue */

/* purple */
CANVAS_BASE_COLOR(colorDA,"colorDA", 0x8000ffff) /* purple */




