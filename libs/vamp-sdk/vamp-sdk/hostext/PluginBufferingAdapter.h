/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vamp

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006-2007 Chris Cannam and QMUL.
    This file by Mark Levy, Copyright 2007 QMUL.
  
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

#ifndef _VAMP_PLUGIN_BUFFERING_ADAPTER_H_
#define _VAMP_PLUGIN_BUFFERING_ADAPTER_H_

#include "PluginWrapper.h"

namespace Vamp {
	
namespace HostExt {
		
/**
 * \class PluginBufferingAdapter PluginBufferingAdapter.h <vamp-sdk/hostext/PluginBufferingAdapter.h>
 *
 * PluginBufferingAdapter is a Vamp plugin adapter that allows plugins
 * to be used by a host supplying an audio stream in non-overlapping
 * buffers of arbitrary size.
 *
 * A host using PluginBufferingAdapter may ignore the preferred step
 * and block size reported by the plugin, and still expect the plugin
 * to run.  The value of blockSize and stepSize passed to initialise
 * should be the size of the buffer which the host will supply; the
 * stepSize should be equal to the blockSize.
 *
 * If the internal step size used for the plugin differs from that
 * supplied by the host, the adapter will modify the sample rate
 * specifications for the plugin outputs (setting them all to
 * VariableSampleRate) and set timestamps on the output features for
 * outputs that formerly used a different sample rate specification.
 * This is necessary in order to obtain correct time stamping.
 * 
 * In other respects, the PluginBufferingAdapter behaves identically
 * to the plugin that it wraps. The wrapped plugin will be deleted
 * when the wrapper is deleted.
 */
		
class PluginBufferingAdapter : public PluginWrapper
{
public:
    PluginBufferingAdapter(Plugin *plugin); // I take ownership of plugin
    virtual ~PluginBufferingAdapter();
    
    bool initialise(size_t channels, size_t stepSize, size_t blockSize);

    size_t getPreferredStepSize() const;
    
    OutputList getOutputDescriptors() const;

    FeatureSet process(const float *const *inputBuffers, RealTime timestamp);
    
    FeatureSet getRemainingFeatures();
    
protected:
    class Impl;
    Impl *m_impl;
};
    
}

}

#endif
