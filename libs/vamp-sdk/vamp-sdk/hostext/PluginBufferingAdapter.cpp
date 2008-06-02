/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vamp

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006-2007 Chris Cannam and QMUL.
    This file by Mark Levy and Chris Cannam.
  
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

#include <vector>
#include <map>

#include "PluginBufferingAdapter.h"

using std::vector;
using std::map;

namespace Vamp {
	
namespace HostExt {
		
class PluginBufferingAdapter::Impl
{
public:
    Impl(Plugin *plugin, float inputSampleRate);
    ~Impl();
		
    bool initialise(size_t channels, size_t stepSize, size_t blockSize);

    OutputList getOutputDescriptors() const;

    void reset();

    FeatureSet process(const float *const *inputBuffers, RealTime timestamp);
		
    FeatureSet getRemainingFeatures();
		
protected:
    class RingBuffer
    {
    public:
        RingBuffer(int n) :
            m_buffer(new float[n+1]), m_writer(0), m_reader(0), m_size(n+1) { }
        virtual ~RingBuffer() { delete[] m_buffer; }

        int getSize() const { return m_size-1; }
        void reset() { m_writer = 0; m_reader = 0; }

        int getReadSpace() const {
            int writer = m_writer, reader = m_reader, space;
            if (writer > reader) space = writer - reader;
            else if (writer < reader) space = (writer + m_size) - reader;
            else space = 0;
            return space;
        }

        int getWriteSpace() const {
            int writer = m_writer;
            int reader = m_reader;
            int space = (reader + m_size - writer - 1);
            if (space >= m_size) space -= m_size;
            return space;
        }
        
        int peek(float *destination, int n) const {

            int available = getReadSpace();

            if (n > available) {
                for (int i = available; i < n; ++i) {
                    destination[i] = 0.f;
                }
                n = available;
            }
            if (n == 0) return n;

            int reader = m_reader;
            int here = m_size - reader;
            const float *const bufbase = m_buffer + reader;

            if (here >= n) {
                for (int i = 0; i < n; ++i) {
                    destination[i] = bufbase[i];
                }
            } else {
                for (int i = 0; i < here; ++i) {
                    destination[i] = bufbase[i];
                }
                float *const destbase = destination + here;
                const int nh = n - here;
                for (int i = 0; i < nh; ++i) {
                    destbase[i] = m_buffer[i];
                }
            }

            return n;
        }

        int skip(int n) {
            
            int available = getReadSpace();
            if (n > available) {
                n = available;
            }
            if (n == 0) return n;

            int reader = m_reader;
            reader += n;
            while (reader >= m_size) reader -= m_size;
            m_reader = reader;
            return n;
        }
        
        int write(const float *source, int n) {

            int available = getWriteSpace();
            if (n > available) {
                n = available;
            }
            if (n == 0) return n;

            int writer = m_writer;
            int here = m_size - writer;
            float *const bufbase = m_buffer + writer;
            
            if (here >= n) {
                for (int i = 0; i < n; ++i) {
                    bufbase[i] = source[i];
                }
            } else {
                for (int i = 0; i < here; ++i) {
                    bufbase[i] = source[i];
                }
                const int nh = n - here;
                const float *const srcbase = source + here;
                float *const buf = m_buffer;
                for (int i = 0; i < nh; ++i) {
                    buf[i] = srcbase[i];
                }
            }

            writer += n;
            while (writer >= m_size) writer -= m_size;
            m_writer = writer;

            return n;
        }

        int zero(int n) {
            
            int available = getWriteSpace();
            if (n > available) {
                n = available;
            }
            if (n == 0) return n;

            int writer = m_writer;
            int here = m_size - writer;
            float *const bufbase = m_buffer + writer;

            if (here >= n) {
                for (int i = 0; i < n; ++i) {
                    bufbase[i] = 0.f;
                }
            } else {
                for (int i = 0; i < here; ++i) {
                    bufbase[i] = 0.f;
                }
                const int nh = n - here;
                for (int i = 0; i < nh; ++i) {
                    m_buffer[i] = 0.f;
                }
            }
            
            writer += n;
            while (writer >= m_size) writer -= m_size;
            m_writer = writer;

            return n;
        }

    protected:
        float *m_buffer;
        int    m_writer;
        int    m_reader;
        int    m_size;

    private:
        RingBuffer(const RingBuffer &); // not provided
        RingBuffer &operator=(const RingBuffer &); // not provided
    };

    Plugin *m_plugin;
    size_t m_inputStepSize;
    size_t m_inputBlockSize;
    size_t m_stepSize;
    size_t m_blockSize;
    size_t m_channels;
    vector<RingBuffer *> m_queue;
    float **m_buffers;
    float m_inputSampleRate;
    RealTime m_timestamp;
    bool m_unrun;
    OutputList m_outputs;
		
    void processBlock(FeatureSet& allFeatureSets, RealTime timestamp);
};
		
PluginBufferingAdapter::PluginBufferingAdapter(Plugin *plugin) :
    PluginWrapper(plugin)
{
    m_impl = new Impl(plugin, m_inputSampleRate);
}
		
PluginBufferingAdapter::~PluginBufferingAdapter()
{
    delete m_impl;
}
		
bool
PluginBufferingAdapter::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    return m_impl->initialise(channels, stepSize, blockSize);
}

PluginBufferingAdapter::OutputList
PluginBufferingAdapter::getOutputDescriptors() const
{
    return m_impl->getOutputDescriptors();
}

void
PluginBufferingAdapter::reset()
{
    m_impl->reset();
}
		
PluginBufferingAdapter::FeatureSet
PluginBufferingAdapter::process(const float *const *inputBuffers,
                                RealTime timestamp)
{
    return m_impl->process(inputBuffers, timestamp);
}
		
PluginBufferingAdapter::FeatureSet
PluginBufferingAdapter::getRemainingFeatures()
{
    return m_impl->getRemainingFeatures();
}
		
PluginBufferingAdapter::Impl::Impl(Plugin *plugin, float inputSampleRate) :
    m_plugin(plugin),
    m_inputStepSize(0),
    m_inputBlockSize(0),
    m_stepSize(0),
    m_blockSize(0),
    m_channels(0), 
    m_queue(0),
    m_buffers(0),
    m_inputSampleRate(inputSampleRate),
    m_timestamp(RealTime::zeroTime),
    m_unrun(true)
{
    m_outputs = plugin->getOutputDescriptors();
}
		
PluginBufferingAdapter::Impl::~Impl()
{
    // the adapter will delete the plugin

    for (size_t i = 0; i < m_channels; ++i) {
        delete m_queue[i];
        delete[] m_buffers[i];
    }
    delete[] m_buffers;
}

size_t
PluginBufferingAdapter::getPreferredStepSize() const
{
    return getPreferredBlockSize();
}
		
bool
PluginBufferingAdapter::Impl::initialise(size_t channels, size_t stepSize, size_t blockSize)
{
    if (stepSize != blockSize) {
        std::cerr << "PluginBufferingAdapter::initialise: input stepSize must be equal to blockSize for this adapter (stepSize = " << stepSize << ", blockSize = " << blockSize << ")" << std::endl;
        return false;
    }

    m_channels = channels;	
    m_inputStepSize = stepSize;
    m_inputBlockSize = blockSize;
    
    // use the step and block sizes which the plugin prefers
    m_stepSize = m_plugin->getPreferredStepSize();
    m_blockSize = m_plugin->getPreferredBlockSize();
    
    // or sensible defaults if it has no preference
    if (m_blockSize == 0) {
        m_blockSize = 1024;
    }
    if (m_stepSize == 0) {
        if (m_plugin->getInputDomain() == Vamp::Plugin::FrequencyDomain) {
            m_stepSize = m_blockSize/2;
        } else {
            m_stepSize = m_blockSize;
        }
    } else if (m_stepSize > m_blockSize) {
        if (m_plugin->getInputDomain() == Vamp::Plugin::FrequencyDomain) {
            m_blockSize = m_stepSize * 2;
        } else {
            m_blockSize = m_stepSize;
        }
    }
    
    // std::cerr << "PluginBufferingAdapter::initialise: stepSize " << m_inputStepSize << " -> " << m_stepSize 
    // << ", blockSize " << m_inputBlockSize << " -> " << m_blockSize << std::endl;			
    
    // current implementation breaks if step is greater than block
    if (m_stepSize > m_blockSize) {
        std::cerr << "PluginBufferingAdapter::initialise: plugin's preferred stepSize greater than blockSize, giving up!" << std::endl;
        return false;
    }

    m_buffers = new float *[m_channels];

    for (size_t i = 0; i < m_channels; ++i) {
        m_queue.push_back(new RingBuffer(m_blockSize + m_inputBlockSize));
        m_buffers[i] = new float[m_blockSize];
    }
    
    return m_plugin->initialise(m_channels, m_stepSize, m_blockSize);
}
		
PluginBufferingAdapter::OutputList
PluginBufferingAdapter::Impl::getOutputDescriptors() const
{
    OutputList outs = m_plugin->getOutputDescriptors();
    for (size_t i = 0; i < outs.size(); ++i) {
        if (outs[i].sampleType == OutputDescriptor::OneSamplePerStep) {
            outs[i].sampleRate = 1.f / m_stepSize;
        }
        outs[i].sampleType = OutputDescriptor::VariableSampleRate;
    }
    return outs;
}

void
PluginBufferingAdapter::Impl::reset()
{
    m_timestamp = RealTime::zeroTime;
    m_unrun = true;

    for (size_t i = 0; i < m_queue.size(); ++i) {
        m_queue[i]->reset();
    }
}

PluginBufferingAdapter::FeatureSet
PluginBufferingAdapter::Impl::process(const float *const *inputBuffers,
                                      RealTime timestamp)
{
    FeatureSet allFeatureSets;

    if (m_unrun) {
        m_timestamp = timestamp;
        m_unrun = false;
    }
			
    // queue the new input
    
    for (size_t i = 0; i < m_channels; ++i) {
        int written = m_queue[i]->write(inputBuffers[i], m_inputBlockSize);
        if (written < int(m_inputBlockSize) && i == 0) {
            std::cerr << "WARNING: PluginBufferingAdapter::Impl::process: "
                      << "Buffer overflow: wrote " << written 
                      << " of " << m_inputBlockSize 
                      << " input samples (for plugin step size "
                      << m_stepSize << ", block size " << m_blockSize << ")"
                      << std::endl;
        }
    }    
    
    // process as much as we can

    while (m_queue[0]->getReadSpace() >= int(m_blockSize)) {
        processBlock(allFeatureSets, timestamp);
    }	
    
    return allFeatureSets;
}
    
PluginBufferingAdapter::FeatureSet
PluginBufferingAdapter::Impl::getRemainingFeatures() 
{
    FeatureSet allFeatureSets;
    
    // process remaining samples in queue
    while (m_queue[0]->getReadSpace() >= int(m_blockSize)) {
        processBlock(allFeatureSets, m_timestamp);
    }
    
    // pad any last samples remaining and process
    if (m_queue[0]->getReadSpace() > 0) {
        for (size_t i = 0; i < m_channels; ++i) {
            m_queue[i]->zero(m_blockSize - m_queue[i]->getReadSpace());
        }
        processBlock(allFeatureSets, m_timestamp);
    }			
    
    // get remaining features			

    FeatureSet featureSet = m_plugin->getRemainingFeatures();

    for (map<int, FeatureList>::iterator iter = featureSet.begin();
         iter != featureSet.end(); ++iter) {
        FeatureList featureList = iter->second;
        for (size_t i = 0; i < featureList.size(); ++i) {
            allFeatureSets[iter->first].push_back(featureList[i]);
        }
    }
    
    return allFeatureSets;
}
    
void
PluginBufferingAdapter::Impl::processBlock(FeatureSet& allFeatureSets,
                                           RealTime timestamp)
{
    for (size_t i = 0; i < m_channels; ++i) {
        m_queue[i]->peek(m_buffers[i], m_blockSize);
    }

    FeatureSet featureSet = m_plugin->process(m_buffers, m_timestamp);
    
    for (map<int, FeatureList>::iterator iter = featureSet.begin();
         iter != featureSet.end(); ++iter) {
	
        FeatureList featureList = iter->second;
        int outputNo = iter->first;
	
        for (size_t i = 0; i < featureList.size(); ++i) {
            
            // make sure the timestamp is set
            switch (m_outputs[outputNo].sampleType) {

            case OutputDescriptor::OneSamplePerStep:
		// use our internal timestamp - OK????
                featureList[i].timestamp = m_timestamp;
                break;

            case OutputDescriptor::FixedSampleRate:
		// use our internal timestamp
                featureList[i].timestamp = m_timestamp;
                break;

            case OutputDescriptor::VariableSampleRate:
                break;		// plugin must set timestamp

            default:
                break;
            }
            
            allFeatureSets[outputNo].push_back(featureList[i]);		
        }
    }
    
    // step forward

    for (size_t i = 0; i < m_channels; ++i) {
        m_queue[i]->skip(m_stepSize);
    }
    
    // fake up the timestamp each time we step forward

    long frame = RealTime::realTime2Frame(m_timestamp,
                                          int(m_inputSampleRate + 0.5));
    m_timestamp = RealTime::frame2RealTime(frame + m_stepSize,
                                           int(m_inputSampleRate + 0.5));
}

}
	
}


