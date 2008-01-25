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

    FeatureSet process(const float *const *inputBuffers, RealTime timestamp);
		
    FeatureSet getRemainingFeatures();
		
protected:
    Plugin *m_plugin;
    size_t m_inputStepSize;
    size_t m_inputBlockSize;
    size_t m_stepSize;
    size_t m_blockSize;
    size_t m_channels;
    vector<vector<float> > m_queue;
    float **m_buffers;    // in fact an array of pointers into the queue
    size_t m_inputPos;    // start position in the queue of next input block 
    float m_inputSampleRate;
    RealTime m_timestamp;	
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
    m_buffers(0),
    m_inputPos(0),
    m_inputSampleRate(inputSampleRate),
    m_timestamp()
{
    m_outputs = plugin->getOutputDescriptors();
}
		
PluginBufferingAdapter::Impl::~Impl()
{
    // the adapter will delete the plugin
    
    delete [] m_buffers;
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
    
    std::cerr << "PluginBufferingAdapter::initialise: stepSize " << m_inputStepSize << " -> " << m_stepSize 
              << ", blockSize " << m_inputBlockSize << " -> " << m_blockSize << std::endl;			
    
    // current implementation breaks if step is greater than block
    if (m_stepSize > m_blockSize) {
        std::cerr << "PluginBufferingAdapter::initialise: plugin's preferred stepSize greater than blockSize, giving up!" << std::endl;
        return false;
    }
    
    m_queue.resize(m_channels);		
    m_buffers = new float*[m_channels];		
    
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

PluginBufferingAdapter::FeatureSet
PluginBufferingAdapter::Impl::process(const float *const *inputBuffers,
                                      RealTime timestamp)
{
    FeatureSet allFeatureSets;
			
    // queue the new input
    
    //std::cerr << "unread " << m_queue[0].size() - m_inputPos << " samples" << std::endl; 
    //std::cerr << "queueing " << m_inputBlockSize - (m_queue[0].size() - m_inputPos) << " samples" << std::endl; 
    
    for (size_t i = 0; i < m_channels; ++i)
        for (size_t j = m_queue[0].size() - m_inputPos; j < m_inputBlockSize; ++j)
            m_queue[i].push_back(inputBuffers[i][j]);
    
    m_inputPos += m_inputStepSize;
    
    // process as much as we can
    while (m_queue[0].size() >= m_blockSize)
    {
        processBlock(allFeatureSets, timestamp);
        m_inputPos -= m_stepSize;
	
        //std::cerr << m_queue[0].size() << " samples still left in queue" << std::endl;
        //std::cerr << "inputPos = " << m_inputPos << std::endl;
    }	
    
    return allFeatureSets;
}
    
PluginBufferingAdapter::FeatureSet
PluginBufferingAdapter::Impl::getRemainingFeatures() 
{
    FeatureSet allFeatureSets;
    
    // process remaining samples in queue
    while (m_queue[0].size() >= m_blockSize)
    {
        processBlock(allFeatureSets, m_timestamp);
    }
    
    // pad any last samples remaining and process
    if (m_queue[0].size() > 0) 
    {
        for (size_t i = 0; i < m_channels; ++i)
            while (m_queue[i].size() < m_blockSize)
                m_queue[i].push_back(0.0);
        processBlock(allFeatureSets, m_timestamp);
    }			
    
    // get remaining features			
    FeatureSet featureSet = m_plugin->getRemainingFeatures();
    for (map<int, FeatureList>::iterator iter = featureSet.begin();
         iter != featureSet.end(); ++iter)
    {
        FeatureList featureList = iter->second;
        for (size_t i = 0; i < featureList.size(); ++i)
            allFeatureSets[iter->first].push_back(featureList[i]);				
    }
    
    return allFeatureSets;
}
    
void
PluginBufferingAdapter::Impl::processBlock(FeatureSet& allFeatureSets, RealTime timestamp)
{
    //std::cerr << m_queue[0].size() << " samples left in queue" << std::endl;
    
    // point the buffers to the head of the queue
    for (size_t i = 0; i < m_channels; ++i)
        m_buffers[i] = &m_queue[i][0];
    
    FeatureSet featureSet = m_plugin->process(m_buffers, m_timestamp);
    
    for (map<int, FeatureList>::iterator iter = featureSet.begin();
         iter != featureSet.end(); ++iter)
    {
	
        FeatureList featureList = iter->second;
        int outputNo = iter->first;
	
        for (size_t i = 0; i < featureList.size(); ++i) 
        {
            
            // make sure the timestamp is set
            switch (m_outputs[outputNo].sampleType)
            {
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
    for (size_t i = 0; i < m_channels; ++i)
        m_queue[i].erase(m_queue[i].begin(), m_queue[i].begin() + m_stepSize);
    
    // fake up the timestamp each time we step forward
    //std::cerr << m_timestamp;
    long frame = RealTime::realTime2Frame(m_timestamp, int(m_inputSampleRate + 0.5));
    m_timestamp = RealTime::frame2RealTime(frame + m_stepSize, int(m_inputSampleRate + 0.5));
    //std::cerr << "--->" << m_timestamp << std::endl;
}

}
	
}


