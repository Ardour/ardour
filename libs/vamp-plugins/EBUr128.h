/*
    Vamp

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006 Chris Cannam.

    Permission is hereby granted, free of charge, to any person
    obtaining a copy of this software and associated documentation
    files (the "Software"), to deal in the Software without
    restriction, including without limitation the rights to use, copy,
    modify, merge, publish, distribute, sublicense, and/or sell copies
    of the Software, and to permit persons to whom the Software is
    furnished to do so, subject to the following conditions:

    The above copyright notice and this permission notice shall be
    included in all copies or substantial portions of the Software.

    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
    EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
    MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
    NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR
    ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
    CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
    WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef _EBUR128_PLUGIN_H_
#define _EBUR128_PLUGIN_H_

#include <vamp-sdk/Plugin.h>

#include "ebu_r128_proc.h"

class VampEBUr128 : public Vamp::Plugin
{
public:
	VampEBUr128 (float inputSampleRate);
	virtual ~VampEBUr128 ();

	size_t getMinChannelCount () const
	{
		return 1;
	}
	size_t getMaxChannelCount () const
	{
		return 2;
	}
	bool initialise (size_t channels, size_t stepSize, size_t blockSize);
	void reset ();

	InputDomain getInputDomain () const
	{
		return TimeDomain;
	}

	std::string getIdentifier () const;
	std::string getName () const;
	std::string getDescription () const;
	std::string getMaker () const;
	int         getPluginVersion () const;
	std::string getCopyright () const;

	OutputList getOutputDescriptors () const;

	FeatureSet process (const float* const* inputBuffers, Vamp::RealTime timestamp);

	FeatureSet getRemainingFeatures ();

protected:
	size_t m_stepSize;
	size_t m_channels;

private:
	FonsEBU::Ebu_r128_proc ebu;
};

#endif
