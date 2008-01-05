#include "dummy_port.h"

#include "midi_byte_array.h"

#include <midi++/port.h>
#include <midi++/types.h>

#include <iostream>

using namespace Mackie;
using namespace std;

DummyPort::DummyPort()
{
}

DummyPort::~DummyPort()
{
}

	
void DummyPort::open()
{
	cout << "DummyPort::open" << endl;
}

	
void DummyPort::close()
{
	cout << "DummyPort::close" << endl;
}


MidiByteArray DummyPort::read()
{
	cout << "DummyPort::read" << endl;
	return MidiByteArray();
}

	
void DummyPort::write( const MidiByteArray & mba )
{
	cout << "DummyPort::write " << mba << endl;
}

MidiByteArray empty_midi_byte_array;

const MidiByteArray & DummyPort::sysex_hdr() const
{
	cout << "DummyPort::sysex_hdr" << endl;
	return empty_midi_byte_array;
}

int DummyPort::strips() const
{
	cout << "DummyPort::strips" << endl;
	return 0;
}
