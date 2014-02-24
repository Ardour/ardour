#ifndef __hardour_misc_h__
#define __hardour_misc_h__

#include "pbd/transmitter.h"
#include "pbd/receiver.h"

class TestReceiver : public Receiver 
{
  protected:
    void receive (Transmitter::Channel chn, const char * str);
};

#endif /* __hardour_misc_h__ */
