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
