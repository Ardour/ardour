/* VST3 interface
 * This is a subset of https://github.com/steinbergmedia/vst3sdk/
 * which should be sufficient to implement a VST3 plugin host.
 *
 * Compat check:
 * g++ -std=c++98 -c -o /tmp/vst3.o -I libs/vst3/ libs/vst3/vst3.h
 *
 * GPLv3
 */
#ifndef _VST3_HEADERS_
#define _VST3_HEADERS_

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#endif

#if (__cplusplus < 201103L)
#  define nullptr 0
#endif

#include "pluginterfaces/base/ftypes.h"
#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/base/ibstream.h"

#include "pluginterfaces/vst/ivstattributes.h"
#include "pluginterfaces/vst/ivstaudioprocessor.h"
#include "pluginterfaces/vst/ivstautomationstate.h"
#include "pluginterfaces/vst/ivstchannelcontextinfo.h"
#include "pluginterfaces/vst/ivstcomponent.h"
//#include "pluginterfaces/vst/ivstcontextmenu.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstevents.h"
#include "pluginterfaces/vst/ivsthostapplication.h"
//#include "pluginterfaces/vst/ivstinterappaudio.h"
#include "pluginterfaces/vst/ivstmessage.h"
#include "pluginterfaces/vst/ivstmidicontrollers.h"
#include "pluginterfaces/vst/ivstmidilearn.h"
//#include "pluginterfaces/vst/ivstnoteexpression.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"
//#include "pluginterfaces/vst/ivstphysicalui.h"
#include "pluginterfaces/vst/ivstpluginterfacesupport.h"
//#include "pluginterfaces/vst/ivstplugview.h"
//#include "pluginterfaces/vst/ivstprefetchablesupport.h"
#include "pluginterfaces/vst/ivstprocesscontext.h"
//#include "pluginterfaces/vst/ivstrepresentation.h"
//#include "pluginterfaces/vst/ivsttestplugprovider.h"
#include "pluginterfaces/vst/ivstunits.h"
//#include "pluginterfaces/vst/vstpresetkeys.h"
//#include "pluginterfaces/vst/vstpshpack4.h"
//#include "pluginterfaces/vst/vstspeaker.h"
#include "pluginterfaces/vst/vsttypes.h"

#include "pluginterfaces/gui/iplugview.h"
//#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"

//#include "pluginterfaces/base/conststringtable.cpp"
//#include "pluginterfaces/base/funknown.cpp"

/* PSL Extensions */
#include "pslextensions/ipslcontextinfo.h"
#include "pslextensions/ipsleditcontroller.h"
#include "pslextensions/ipslviewembedding.h"
#include "pslextensions/ipslviewscaling.h"
//#include "pslextensions/ipslgainreduction.h"
//#include "pslextensions/ipslhostcommands.h"

#if defined(__clang__)
#    pragma clang diagnostic pop
#elif __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)
#    pragma GCC diagnostic pop
#endif

#endif
