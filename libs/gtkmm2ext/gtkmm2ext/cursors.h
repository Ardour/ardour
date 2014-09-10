#ifndef __gtkmm2ext_cursor_info_h___
#define __gtkmm2ext_cursor_info_h___

#include <string>
#include <map>

namespace Gtkmm2ext {

class CursorInfo 
{
    public:
        static CursorInfo* lookup_cursor_info (const std::string& image_name);
        static int load_cursor_info (const std::string& path);
        static void drop_cursor_info ();

    private:
        CursorInfo (const std::string& image_name, int hotspot_x, int hotspot_y);
        
        typedef std::map<std::string,CursorInfo*> Infos;
        static Infos infos;

        std::string name;
        int x;
        int y;
};

} /* namespace */

#endif /* __gtkmm2ext_cursor_info_h___ */
