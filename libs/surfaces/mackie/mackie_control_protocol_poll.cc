#include "mackie_control_protocol.h"

#include "midi_byte_array.h"
#include "surface_port.h"

#include "pbd/pthread_utils.h"
#include "pbd/error.h"

#include "midi++/types.h"
#include "midi++/port.h"
#include "midi++/manager.h"
#include "i18n.h"

#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <string.h>

#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace Mackie;
using namespace PBD;

