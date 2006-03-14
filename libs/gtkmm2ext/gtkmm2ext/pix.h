#ifndef __pbd_gtkmm_pix_h__
#define __pbd_gtkmm_pix_h__

#include <string>
#include <map>
#include <vector>
#include <gtkmm.h>

namespace Gtkmm2ext {

class Pix
{
	typedef std::map<std::string, Pix *> PixCache;
	static PixCache *cache;

	PixCache::iterator cache_position;
	int refcnt;
        bool generated;
	std::vector<std::string *> *files;
	std::vector<const char* const*>   data;
	bool from_files;
	int pixmap_count;
	int last_pixmap;
	Glib::RefPtr<Gdk::Pixmap> *pixmaps;
	Glib::RefPtr<Gdk::Bitmap> *bitmaps;
	int max_pixwidth;
	int max_pixheight;
	bool _homegenous;

        Pix (const std::string &dirpath, const std::string &regexp, 
	     bool homogenous = true);
	Pix (std::vector<const char* const*> xpm_data, bool homogenous = true);
        virtual ~Pix();

	friend  Pix *get_pix (const std::string &dirpath, 
			      const std::string &regexp,
			      bool homogenous);
	friend  Pix *get_pix (std::string name, 
			      std::vector<const char* const*> xpm_data,
			      bool homogenous);
	friend  void finish_pix (Pix *);

  public:      
	Pix (bool homogenous = true);

        void generate (Glib::RefPtr<Gdk::Drawable>&);
	int n_pixmaps() { return pixmap_count; }
	int max_pixmap() { return last_pixmap; }
	bool homogenous () { return _homegenous; }

	/* ref/unref should really be protected, but we don't know the
	   name of the class that should have access to them.  
	*/

	void ref () { refcnt++; }
	void unref () { if (refcnt) refcnt--; }

        Glib::RefPtr<Gdk::Bitmap>* shape_mask (int n) {
		if (n < pixmap_count) {
			return &bitmaps[n];
		} 
		return 0;
	}

	Glib::RefPtr<Gdk::Pixmap>* pixmap(int n) {
		if (n < pixmap_count) {
			return &pixmaps[n];
		} 
		return 0;
	}

	int max_width() { return max_pixwidth; }
	int max_height() { return max_pixheight; }
};

extern Pix *get_pix (const std::string &dirpath, 
		     const std::string &regexp, 
		     bool homog = false);

extern Pix *get_pix (std::string, 
		     std::vector<const char **>,
		     bool homog = false);
extern void finish_pix (Pix *);

} /* namespace */

#endif  // __pbd_gtkmm_pix_h__
