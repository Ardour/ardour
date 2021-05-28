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

#include "EBUr128.h"

using std::cerr;
using std::endl;
using std::string;
using std::vector;

VampEBUr128::VampEBUr128 (float inputSampleRate)
	: Plugin (inputSampleRate)
	, m_stepSize (0)
{
}

VampEBUr128::~VampEBUr128 ()
{
}

string
VampEBUr128::getIdentifier () const
{
	return "ebur128";
}

string
VampEBUr128::getName () const
{
	return "EBU R128 Loudness";
}

string
VampEBUr128::getDescription () const
{
	return "Loudness measurements according to the EBU Recommendation 128";
}

string
VampEBUr128::getMaker () const
{
	return "Harrison Consoles";
}

int
VampEBUr128::getPluginVersion () const
{
	return 2;
}

string
VampEBUr128::getCopyright () const
{
	return "GPL version 2 or later";
}

bool
VampEBUr128::initialise (size_t channels, size_t stepSize, size_t blockSize)
{
	if (channels < getMinChannelCount () || channels > getMaxChannelCount ()) {
		return false;
	}

	m_stepSize = std::min (stepSize, blockSize);
	m_channels = channels;

	ebu.init (m_channels, m_inputSampleRate);

	return true;
}

void
VampEBUr128::reset ()
{
	ebu.reset ();
}

VampEBUr128::OutputList
VampEBUr128::getOutputDescriptors () const
{
	OutputList list;

	OutputDescriptor zc;

	zc.identifier       = "loundless";
	zc.name             = "Loudness";
	zc.description      = "Loudness (integrated, short, momentary)";
	zc.unit             = "LUFS";
	zc.hasFixedBinCount = true;
	zc.binCount         = 0;
	zc.hasKnownExtents  = false;
	zc.isQuantized      = false;
	zc.sampleType       = OutputDescriptor::OneSamplePerStep;
	list.push_back (zc);

	zc.identifier       = "range";
	zc.name             = "Integrated Loudness Range";
	zc.description      = "Dynamic Range of the Audio";
	zc.unit             = "LU";
	zc.hasFixedBinCount = true;
	zc.binCount         = 0;
	zc.hasKnownExtents  = false;
	zc.isQuantized      = false;
	zc.sampleType       = OutputDescriptor::OneSamplePerStep;
	list.push_back (zc);

	zc.identifier       = "histogram";
	zc.name             = "Loudness Histogram";
	zc.description      = "Dynamic Range of the audio";
	zc.unit             = "";
	zc.hasFixedBinCount = false;
	zc.binCount         = 0;
	zc.hasKnownExtents  = false;
	zc.isQuantized      = false;
	zc.sampleType       = OutputDescriptor::OneSamplePerStep;
	list.push_back (zc);

	return list;
}

VampEBUr128::FeatureSet
VampEBUr128::process (const float* const* inputBuffers,
                      Vamp::RealTime      timestamp)
{
	if (m_stepSize == 0) {
		cerr << "ERROR: VampEBUr128::process: "
		     << "VampEBUr128 has not been initialised"
		     << endl;
		return FeatureSet ();
	}

	ebu.integr_start (); // noop if already started
	ebu.process (m_stepSize, inputBuffers);

	FeatureSet returnFeatures;

	Feature loudness_integrated;
	loudness_integrated.hasTimestamp = false;
	loudness_integrated.values.push_back (ebu.integrated ());

	Feature loudness_short;
	loudness_short.hasTimestamp = false;
	loudness_short.values.push_back (ebu.loudness_S ());

	Feature loudness_momentary;
	loudness_momentary.hasTimestamp = false;
	loudness_momentary.values.push_back (ebu.loudness_M ());

	returnFeatures[0].push_back (loudness_integrated);
	returnFeatures[0].push_back (loudness_short);
	returnFeatures[0].push_back (loudness_momentary);

	return returnFeatures;
}

VampEBUr128::FeatureSet
VampEBUr128::getRemainingFeatures ()
{
	FeatureSet returnFeatures;

	Feature loudness_integrated;
	loudness_integrated.hasTimestamp = false;
	loudness_integrated.values.push_back (ebu.integrated ());

	Feature loudness_short;
	loudness_short.hasTimestamp = false;
	loudness_short.values.push_back (ebu.maxloudn_S ());

	Feature loudness_momentary;
	loudness_momentary.hasTimestamp = false;
	loudness_momentary.values.push_back (ebu.maxloudn_M ());

	returnFeatures[0].push_back (loudness_integrated);
	returnFeatures[0].push_back (loudness_short);
	returnFeatures[0].push_back (loudness_momentary);

	Feature range;
	range.hasTimestamp = false;
	range.values.push_back (ebu.range_max () - ebu.range_min ());
	returnFeatures[1].push_back (range);

	Feature hist;
	hist.hasTimestamp = false;
	const int* hist_S = ebu.histogram_S ();
	for (int i = 110; i < 650; ++i) {
		hist.values.push_back (hist_S[i]);
	}
	returnFeatures[2].push_back (hist);

	return returnFeatures;
}
