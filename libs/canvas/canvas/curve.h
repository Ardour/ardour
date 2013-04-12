#ifndef __CANVAS_CURVE_H__
#define __CANVAS_CURVE_H__

#include "canvas/poly_item.h"

namespace ArdourCanvas {

class Curve : public PolyItem
{
public:
    Curve (Group *);
    
    void compute_bounding_box () const;

    void render (Rect const & area, Cairo::RefPtr<Cairo::Context>) const;
    XMLNode* get_state () const;
    void set_state (XMLNode const *);

    void set (Points const &);

  protected:
    void render_path (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
    void render_curve (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
    
  private:
    Points first_control_points;
    Points second_control_points;

    
    static void compute_control_points (Points const &,
					Points&, Points&);
    static double* solve (std::vector<double> const&);
};
	
}

#endif
