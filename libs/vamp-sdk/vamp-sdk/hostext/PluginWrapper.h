/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Vamp

    An API for audio analysis and feature extraction plugins.

    Centre for Digital Music, Queen Mary, University of London.
    Copyright 2006-2007 Chris Cannam and QMUL.
  
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

#ifndef _VAMP_PLUGIN_WRAPPER_H_
#define _VAMP_PLUGIN_WRAPPER_H_

#include <vamp-sdk/Plugin.h>

namespace Vamp {

namespace HostExt {

/**
 * \class PluginWrapper PluginWrapper.h <vamp-sdk/hostext/PluginWrapper.h>
 * 
 * PluginWrapper is a simple base class for adapter plugins.  It takes
 * a pointer to a "to be wrapped" Vamp plugin on construction, and
 * provides implementations of all the Vamp plugin methods that simply
 * delegate through to the wrapped plugin.  A subclass can therefore
 * override only the methods that are meaningful for the particular
 * adapter.
 *
 * \note This class was introduced in version 1.1 of the Vamp plugin SDK.
 */

class PluginWrapper : public Plugin
{
public:
    virtual ~PluginWrapper();
    
    bool initialise(size_t channels, size_t stepSize, size_t blockSize);
    void reset();

    InputDomain getInputDomain() const;

    unsigned int getVampApiVersion() const;
    std::string getIdentifier() const;
    std::string getName() const;
    std::string getDescription() const;
    std::string getMaker() const;
    int getPluginVersion() const;
    std::string getCopyright() const;

    ParameterList getParameterDescriptors() const;
    float getParameter(std::string) const;
    void setParameter(std::string, float);

    ProgramList getPrograms() const;
    std::string getCurrentProgram() const;
    void selectProgram(std::string);

    size_t getPreferredStepSize() const;
    size_t getPreferredBlockSize() const;

    size_t getMinChannelCount() const;
    size_t getMaxChannelCount() const;

    OutputList getOutputDescriptors() const;

    FeatureSet process(const float *const *inputBuffers, RealTime timestamp);

    FeatureSet getRemainingFeatures();

protected:
    PluginWrapper(Plugin *plugin); // I take ownership of plugin
    Plugin *m_plugin;
};

}

}

#endif
