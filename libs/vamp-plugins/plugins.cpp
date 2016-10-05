/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

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

    Except as contained in this notice, the names of the Centre for
    Digital Music; Queen Mary, University of London; and Chris Cannam
    shall not be used in advertising or otherwise to promote the sale,
    use or other dealings in this Software without prior written
    authorization.
*/

#ifdef WAF_BUILD
#include "libvampplugins-config.h"
#endif

#include <vamp/vamp.h>
#include <vamp-sdk/PluginAdapter.h>

#include "AmplitudeFollower.h"
#include "BarBeatTrack.h"
#include "BeatTrack.h"
#include "ChromagramPlugin.h"
#include "EBUr128.h"
#include "KeyDetect.h"
#include "OnsetDetect.h"
#include "PercussionOnsetDetector.h"
#include "SimilarityPlugin.h"
#include "SpectralCentroid.h"
#include "TonalChangeDetect.h"
#include "Transcription.h"
#include "TruePeak.h"
#include "ZeroCrossing.h"

#ifdef HAVE_AUBIO
#include "Onset.h"
#endif

static Vamp::PluginAdapter<AmplitudeFollower> AmplitudeFollowerAdapter;
static Vamp::PluginAdapter<BarBeatTracker> BarBeatTrackerAdapter;
static Vamp::PluginAdapter<BeatTracker> BeatTrackerAdapter;
static Vamp::PluginAdapter<ChromagramPlugin> ChromagramPluginAdapter;
static Vamp::PluginAdapter<VampEBUr128> EBUr128Adapter;
static Vamp::PluginAdapter<KeyDetector> KeyDetectorAdapter;
static Vamp::PluginAdapter<OnsetDetector> OnsetDetectorAdapter;
static Vamp::PluginAdapter<PercussionOnsetDetector> PercussionOnsetDetectorAdapter;
static Vamp::PluginAdapter<SimilarityPlugin> SimilarityPluginAdapter;
static Vamp::PluginAdapter<SpectralCentroid> SpectralCentroidAdapter;
static Vamp::PluginAdapter<TonalChangeDetect> TonalChangeDetectAdapter;
static Vamp::PluginAdapter<Transcription> TranscriptionAdapter;
static Vamp::PluginAdapter<VampTruePeak> TruePeakAdapter;
static Vamp::PluginAdapter<ZeroCrossing> ZeroCrossingAdapter;

#ifdef HAVE_AUBIO
static Vamp::PluginAdapter<Onset> OnsetAdapter;
#endif

const VampPluginDescriptor *vampGetPluginDescriptor(unsigned int version,
                                                    unsigned int index)
{
    if (version < 1) return 0;

    switch (index) {
    case  0: return AmplitudeFollowerAdapter.getDescriptor();
    case  1: return BarBeatTrackerAdapter.getDescriptor();
    case  2: return BeatTrackerAdapter.getDescriptor();
    case  3: return ChromagramPluginAdapter.getDescriptor();
    case  4: return EBUr128Adapter.getDescriptor();
    case  5: return KeyDetectorAdapter.getDescriptor();
    case  6: return OnsetDetectorAdapter.getDescriptor();
    case  7: return PercussionOnsetDetectorAdapter.getDescriptor();
    case  8: return SimilarityPluginAdapter.getDescriptor();
    case  9: return SpectralCentroidAdapter.getDescriptor();
    case 10: return TonalChangeDetectAdapter.getDescriptor();
    case 11: return TranscriptionAdapter.getDescriptor();
    case 12: return TruePeakAdapter.getDescriptor();
    case 13: return ZeroCrossingAdapter.getDescriptor();
#ifdef HAVE_AUBIO
    case 14: return OnsetAdapter.getDescriptor();
#endif
    default: return 0;
    }
}
