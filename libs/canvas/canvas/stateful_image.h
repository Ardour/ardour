#include <string>
#include <vector>
#include <map>

#include <cairomm/cairomm.h>

#include "canvas/item.h"

class StatefulImage : public class Item
{
  private:
    class State {
	ImageHandle image;
    };
    
    typedef Cairo::RefPtr<Cairo::ImageSurface> ImageHandle;
    typedef std::vector<State> Images;

  public:

    StatefulImage (const XMLNode&);
    ~StatefulImage ();

    bool set_state (States::size_type);
    void set_text (const std::string&);

    void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
    void compute_bounding_box () const;

    static void set_image_search_path (const std::string&);

  private:
    States                  _states;
    Images::size_type       _state;
    std::string             _text;
    Pango::FontDescription* _font;
    uint32_t                _text_color;
    double                  _text_x;
    double                  _text_y;

    int load_states (const XMLNode&);
    
    typedef std::map<std::string,Cairo::RefPtr<Cairo::ImageSurface> > ImageCache;
    static ImageCache _image_cache;
    static PBD::Searchpath _image_search_path;

    static ImageHandle find_image (const std::string&);
};
