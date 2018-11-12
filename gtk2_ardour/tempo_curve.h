#ifndef __gtk_ardour_tempo_curve_h__
#define __gtk_ardour_tempo_curve_h__

#include <string>
#include <glib.h>

#include <sigc++/signal.h>

#include "ardour/ardour.h"
#include "pbd/signals.h"

#include "canvas/types.h"
#include "canvas/framed_curve.h"
#include "canvas/text.h"

namespace ARDOUR {
	class TempoSection;
}
class PublicEditor;

class TempoCurve : public sigc::trackable
{
public:
	TempoCurve (PublicEditor& editor, ArdourCanvas::Container &, guint32 rgba, ARDOUR::TempoSection& temp, samplepos_t sample, bool handle_events);
	~TempoCurve ();

	static PBD::Signal1<void,TempoCurve*> CatchDeletion;

	static void setup_sizes (const double timebar_height);

	ArdourCanvas::Item& the_item() const;
	void canvas_height_set (double);

	void set_position (samplepos_t lower, samplepos_t upper);
	void set_color_rgba (uint32_t rgba);
	samplepos_t position() const { return sample_position; }

	ArdourCanvas::Container * get_parent() { return _parent; }
	void reparent (ArdourCanvas::Container & parent);

	void hide ();
	void show ();

	ARDOUR::TempoSection& tempo () const { return _tempo; }

	void set_max_tempo (const double& max) { _max_tempo = max; }
	void set_min_tempo (const double& min) { _min_tempo = min; }

protected:
	PublicEditor& editor;

	ArdourCanvas::Container* _parent;
	ArdourCanvas::Container *group;
	ArdourCanvas::Points *points;
	ArdourCanvas::FramedCurve* _curve;

	double        unit_position;
	samplepos_t    sample_position;
	samplepos_t    _end_sample;
	bool         _shown;
	double       _canvas_height;
	uint32_t     _color;

	void reposition ();
private:
	double       _min_tempo;
	double       _max_tempo;
	/* disallow copy construction */
	TempoCurve (TempoCurve const &);
	TempoCurve & operator= (TempoCurve const &);
	ARDOUR::TempoSection& _tempo;
	ArdourCanvas::Text *_start_text;
	ArdourCanvas::Text *_end_text;

};
#endif /* __gtk_ardour_tempo_curve_h__ */
