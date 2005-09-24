#include <stdint.h>

template<class AutomatedObject>
struct AutomationEvent {
    uint32_t frame;
    AutomatedObject *object;
    void (AutomatedObject::*function) (void *);
    void *arg;

    void operator() (){ 
	    object->function (arg);
    }
};
