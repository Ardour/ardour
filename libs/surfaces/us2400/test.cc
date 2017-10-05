#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cstdarg>
#include <iomanip>

#include "midi_byte_array.h"

using namespace std;

namespace MIDI {
	typedef unsigned char byte;
	byte sysex = 0xf0;
	byte eox = 0xf7;
}

int main()
{
	MidiByteArray bytes( 4, 0xf0, 0x01, 0x03, 0x7f );
	cout << bytes << endl;
	return 0;
}

