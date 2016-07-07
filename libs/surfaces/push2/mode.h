#ifndef __ardour_push2_mode_h__
#define __ardour_push2_mode_h__

#include <vector>

class MusicalMode
{
  public:
	enum Type {
		Dorian,
		IonianMajor,
		Minor,
		HarmonicMinor,
		MelodicMinorAscending,
		MelodicMinorDescending,
		Phrygian,
		Lydian,
		Mixolydian,
		Aeolian,
		Locrian,
		PentatonicMajor,
		PentatonicMinor,
		Chromatic,
		BluesScale,
		NeapolitanMinor,
		NeapolitanMajor,
		Oriental,
		DoubleHarmonic,
		Enigmatic,
		Hirajoshi,
		HungarianMinor,
		HungarianMajor,
		Kumoi,
		Iwato,
		Hindu,
		Spanish8Tone,
		Pelog,
		HungarianGypsy,
		Overtone,
		LeadingWholeTone,
		Arabian,
		Balinese,
		Gypsy,
		Mohammedan,
		Javanese,
		Persian,
		Algerian
	};

	MusicalMode (Type t);
	~MusicalMode ();

	std::vector<float> steps;

  private:
	static void fill (MusicalMode&, Type);
};

#endif /* __ardour_push2_mode_h__ */
