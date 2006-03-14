/*
    Copyright (C) 1998-99 Paul Barton-Davis
 
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

    $Id$
*/

#include <algorithm>

#include <gdkmm.h>
#include <gdkmm/pixmap.h>

#include <pbd/pathscanner.h>
#include <pbd/stl_delete.h>
#include <pbd/failed_constructor.h>

/* as of gcc 2.95.2, some of the stl_functors in this header are not
   handled correctly. it sucks, but we put them inline here instead.

   #include <pbd/stl_functors.h>
*/

#include <gtkmm2ext/pix.h>
#include <gtkmm2ext/utils.h>

namespace std
{
	template<> struct less<string *> {
	    bool operator()(string *s1, string *s2) const {
		    return *s1 < *s2;
	    }
	};
}

using namespace std;
using namespace Gtkmm2ext;

Pix::PixCache *Pix::cache;

Pix::Pix (bool homog)

{
	pixmap_count = 0;
	_homegenous = homog;
}

Pix::Pix (vector<const char **> xpm_data, bool homog)
{
	if (xpm_data.size() == 0) {
		throw failed_constructor();
	}

	pixmap_count = xpm_data.size();
	last_pixmap = pixmap_count - 1;
	refcnt = 0;
	generated = false;
	max_pixwidth = 0;
	max_pixheight = 0;
	_homegenous = homog;

	data = xpm_data;
	from_files = false;

	pixmaps = new Glib::RefPtr<Gdk::Pixmap> [pixmap_count];
	bitmaps = new Glib::RefPtr<Gdk::Bitmap> [pixmap_count];
	memset (pixmaps, 0, sizeof (Glib::RefPtr<Gdk::Pixmap>) * pixmap_count);
	memset (bitmaps, 0, sizeof (Glib::RefPtr<Gdk::Bitmap>) * pixmap_count);
}

Pix::Pix (const string &dirpath, const string &regexp, bool homog)

{
	PathScanner scanner;
	less<string *> cmp;

	pixmap_count = 0;
	last_pixmap = 0;
	refcnt = 0;
	generated = false;
	max_pixwidth = 0;
	max_pixheight = 0;
	_homegenous = homog;

	pixmaps = 0;
	bitmaps = 0;
	
	files = scanner (dirpath, regexp, false, true);

	sort (files->begin(), files->end(), cmp);

	if (files == 0) {
		return;
	}
	
	/* create handy reference */

	if ((pixmap_count = files->size()) == 0) {
		return;
	}

	from_files = true;
	pixmaps = new Glib::RefPtr<Gdk::Pixmap> [pixmap_count];
	bitmaps = new Glib::RefPtr<Gdk::Bitmap> [pixmap_count];
	memset (pixmaps, 0, sizeof (Glib::RefPtr<Gdk::Pixmap>) * pixmap_count);
	memset (bitmaps, 0, sizeof (Glib::RefPtr<Gdk::Bitmap>) * pixmap_count);

	last_pixmap = pixmap_count - 1;
}

Pix::~Pix ()

{
	if (from_files) {
		vector_delete (files);
	}

	if (pixmap_count) delete [] pixmaps;
	if (pixmap_count) delete [] bitmaps;
}

void
Pix::generate ()

{
	if (generated) {
		return;
	}

	for (int i = 0; i < pixmap_count; i++) {
		if (from_files) {
			pixmaps[i] = Gdk::Pixmap::create_from_xpm (get_bogus_drawable(), Gdk::Colormap::get_system(),
								bitmaps[i], Gdk::Color(), *(*files)[i]);
		} else {
			gchar **xpm;
			xpm = const_cast<gchar **> (data[i]);
			
			pixmaps[i] = Gdk::Pixmap::create_from_xpm(Gdk::Colormap::get_system(), 
								bitmaps[i], Gdk::Color(), xpm);
		}
			
		int w, h;
		pixmaps[i]->get_size(w, h);
		if (w > max_pixwidth) max_pixwidth = w;
		if (h > max_pixheight) max_pixheight = h;
	}

	generated = true;
}

Pix *
Gtkmm2ext::get_pix (string name, vector<const char **> xpm_data, bool homog)
{
	Pix *ret = 0;
	Pix::PixCache::iterator iter;
	pair<string, Pix *> newpair;

	if (Pix::cache == 0) {
		Pix::cache = new Pix::PixCache;
	}

	if ((iter = Pix::cache->find (name)) == Pix::cache->end()) {
		ret = new Pix (xpm_data, homog);
		if (ret->pixmap_count == 0) {
			delete ret;
			return 0;
		}
		newpair.first = name;
		newpair.second = ret;
		ret->cache_position = (Pix::cache->insert (newpair)).first;
		ret->refcnt++;
		return ret;
	} else {
		(*iter).second->refcnt++;
		return (*iter).second;
	}
}

Pix *
Gtkmm2ext::get_pix (const string &dirpath, const string &regexp, bool homog)
{
	Pix *ret = 0;
	Pix::PixCache::iterator iter;
	pair<string, Pix *> newpair;

	if (Pix::cache == 0) {
		Pix::cache = new Pix::PixCache;
	}

	if ((iter = Pix::cache->find (regexp)) == Pix::cache->end()) {
		ret = new Pix (dirpath, regexp, homog);
		if (ret->pixmap_count == 0) {
			delete ret;
			return 0;
		}
		newpair.first = regexp;
		newpair.second = ret;
		ret->cache_position = (Pix::cache->insert (newpair)).first;
		ret->refcnt++;
		return ret;
	} else {
		(*iter).second->refcnt++;
		return (*iter).second;
	}
}

void 
Gtkmm2ext::finish_pix (Pix *pix)

{
	pix->refcnt--;
	if (pix->refcnt == 0) {
		Pix::cache->erase (pix->cache_position);
		delete pix;
	}
}
