#ifndef __libardour_trimmable_h__
#define __libardour_trimmable_h__

namespace ARDOUR { 

class Trimmable {
  public:
        Trimmable() {}
        virtual ~Trimmable() {}

        enum CanTrim { 
                FrontTrimEarlier,
                FrontTrimLater,
                EndTrimEarlier,
                EndTrimLater,
                TopTrimUp,
                TopTrimDown,
                BottomTrimUp,
                BottomTrimDown
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
