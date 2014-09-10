#include <sstream>
#include <fstream>

#include "gtkmm2ext/cursors.h"

using namespace Gtkmm2ext;

CursorInfo::Infos CursorInfo::infos;

CursorInfo::CursorInfo (const std::string& n, int hotspot_x, int hotspot_y)
        : name (n)
        , x (hotspot_x)
        , y (hotspot_y)
{
}

int
CursorInfo::load_cursor_info (const std::string& path)
{
        std::ifstream infofile (path.c_str());

        if (!infofile) {
                return -1;
        }

        std::stringstream s;
        std::string name;
        int x;
        int y;

        do {
                s << infofile;
                if (!infofile) {
                        break;
                }
                s >> name;
                s >> x;
                s >> y;
                if (!s) {
                        break;
                }
                
                CursorInfo* ci = new CursorInfo (name, x, y);
                infos[name] = ci;

        } while (true);

        return 0;
}

void
CursorInfo::drop_cursor_info ()
{
        infos.clear ();
}

CursorInfo*
CursorInfo::lookup_cursor_info (const std::string& name)
{
        Infos::iterator i = infos.find (name);

        if (i == infos.end()) {
                return 0;
        }
        return i->second;
}
