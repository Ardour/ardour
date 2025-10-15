//------------------------------------------------------------------------------
//! \file       ARAVST3.h
//!             Celemony extension to the Steinberg VST3 SDK to integrate ARA into VST3 plug-ins
//! \project    ARA API Specification
//! \copyright  Copyright (c) 2012-2025, Celemony Software GmbH, All Rights Reserved.
//!             Developed in cooperation with PreSonus Software Ltd.
//! \license    Licensed under the Apache License, Version 2.0 (the "License");
//!             you may not use this file except in compliance with the License.
//!             You may obtain a copy of the License at
//!
//!               http://www.apache.org/licenses/LICENSE-2.0
//!
//!             Unless required by applicable law or agreed to in writing, software
//!             distributed under the License is distributed on an "AS IS" BASIS,
//!             WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//!             See the License for the specific language governing permissions and
//!             limitations under the License.
//------------------------------------------------------------------------------

#ifndef ARAVST3_h
#define ARAVST3_h

/***************************************************************************************************/
// This is a Celemony extension to the Steinberg VST3 SDK to integrate ARA into VST3 plug-ins
// VST is a trademark of Steinberg Media Technologies GmbH
/***************************************************************************************************/

#include "ARAInterface.h"


//! Locally suppress some warnings that may be enabled in some context these headers are compiled.
//@{

#if !defined (ARA_DISABLE_VST3_WARNINGS_BEGIN) || !defined (ARA_DISABLE_VST3_WARNINGS_END)
    #if defined (_MSC_VER)
        #define ARA_DISABLE_VST3_WARNINGS_BEGIN \
            __pragma (warning(push)) \
            __pragma (warning(disable : 4365)) /*signed/unsigned mismatch*/
        #define ARA_DISABLE_VST3_WARNINGS_END \
            __pragma (warning(pop))
    #elif defined (__GNUC__)
        #define ARA_DISABLE_VST3_PRAGMA_PACK_WARNINGS
        #if defined (__has_warning)
            #if __has_warning ("-Wpragma-pack")
                #undef ARA_DISABLE_VST3_PRAGMA_PACK_WARNINGS
                #define ARA_DISABLE_VST3_PRAGMA_PACK_WARNINGS _Pragma ("GCC diagnostic ignored \"-Wpragma-pack\"")
            #endif
        #endif
        #if defined (__clang__)
            #define ARA_DISABLE_VST3_GCC_VARIANT_WARNINGS \
                _Pragma ("GCC diagnostic ignored \"-Wshadow-field-in-constructor\"") \
                _Pragma ("GCC diagnostic ignored \"-Wzero-as-null-pointer-constant\"") \
                _Pragma ("GCC diagnostic ignored \"-Wreserved-id-macro\"")
        #else
            #define ARA_DISABLE_VST3_GCC_VARIANT_WARNINGS \
                _Pragma ("GCC diagnostic ignored \"-Wshadow\"")
        #endif
        #define ARA_DISABLE_VST3_WARNINGS_BEGIN \
            _Pragma ("GCC diagnostic push") \
            _Pragma ("GCC diagnostic ignored \"-Wnon-virtual-dtor\"") \
            _Pragma ("GCC diagnostic ignored \"-Wsign-conversion\"") \
            _Pragma ("GCC diagnostic ignored \"-Wold-style-cast\"") \
            _Pragma ("GCC diagnostic ignored \"-Wextra-semi\"") \
            _Pragma ("GCC diagnostic ignored \"-Wundef\"") \
            ARA_DISABLE_VST3_PRAGMA_PACK_WARNINGS \
            ARA_DISABLE_VST3_GCC_VARIANT_WARNINGS
        #define ARA_DISABLE_VST3_WARNINGS_END \
            _Pragma ("GCC diagnostic pop")
    #else
        #define ARA_DISABLE_VST3_WARNINGS_BEGIN
        #define ARA_DISABLE_VST3_WARNINGS_END
    #endif
#endif

//@}

ARA_DISABLE_VST3_WARNINGS_BEGIN

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/base/falignpush.h"


namespace ARA
{

//! @defgroup Companion_APIs Companion APIs
//! @{

//! @defgroup VST3 VST3
//! @{

/***************************************************************************************************/
//! Interface class to be implemented by an object provided by the VST3 factory.
//! The host can use the VST3 factory to directly obtain the ARA factory, which allows for creating
//! and maintaining the model independently of any IAudioProcessor instances, enabling tasks such as
//! automatic tempo detection or audio-to-MIDI conversion.
//! For rendering and editing the model however, there must be an associated IAudioProcessor class
//! provided in the same binary.
//! This match is usually trivial because there typically is only one such class in the binary, but
//! there are cases such as WaveShell where multiple plug-ins live in the same binary, and only a
//! subset of those plug-ins support ARA. In this scenario, the plug-in must use the same class name
//! for the matching pair of ARA::IMainFactory and IAudioProcessor classes - this enables the host
//! to quickly identify the matching pairs without having to create instances of all the
//! IAudioProcessor classes to query their IPlugInEntryPoint::getFactory ()->factoryID to perform
//! the matching.
class IMainFactory: public Steinberg::FUnknown
{
public:
    //! Get the ARA factory.
    //! The returned pointer must remain valid throughout the lifetime of the object that provided it.
    //! The returned ARAFactory must be equal to the ARAFactory provided by the associated
    //! IAudioProcessor class through its IPlugInEntryPoint.
    virtual const ARAFactory* PLUGIN_API getFactory () = 0;

    static const Steinberg::FUID iid;
};

//! Class category name for the ARA::IMainFactory.
#if !defined (kARAMainFactoryClass)
    #define kARAMainFactoryClass "ARA Main Factory Class"
#endif

DECLARE_CLASS_IID (IMainFactory, 0xDB2A1669, 0xFAFD42A5, 0xA82F864F, 0x7B6872EA)


/***************************************************************************************************/
//! Interface class to be implemented by the VST3 IAudioProcessor component (kVstAudioEffectClass).
class IPlugInEntryPoint: public Steinberg::FUnknown
{
public:
    //! Get the ARA factory.
    //! The returned pointer must remain valid throughout the lifetime of the object that provided it.
    //! The returned ARAFactory must be equal to the ARAFactory provided by the associated IMainFactory.
    //! To prevent ambiguities, the name of the plug-in as stored in the PClassInfo.name of this
    //! class must match the ARAFactory.plugInName returned here.
    virtual const ARAFactory* PLUGIN_API getFactory () = 0;

    //! Bind the VST3 instance to an ARA document controller, switching it from "normal" operation
    //! to ARA mode, and exposing the ARA plug-in extension.
    //! Note that since ARA 2.0, this call has been deprecated and replaced with
    //! bindToDocumentControllerWithRoles ().
    //! This deprecated call is equivalent to the new call with no known roles set, however all
    //! ARA 1.x hosts are in fact using all instances with playback renderer, edit renderer and
    //! editor view role enabled, so plug-ins implementing ARA 1 backwards compatibility can
    //! safely assume those three roles to be enabled if this call was made.
    //! Same call order rules as bindToDocumentControllerWithRoles () apply.
    ARA_DEPRECATED (2_0_Draft) virtual const ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentController (ARADocumentControllerRef documentControllerRef) = 0;

    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IPlugInEntryPoint, 0x12814E54, 0xA1CE4076, 0x82B96813, 0x16950BD6)

//! ARA 2 extension of IPlugInEntryPoint
class ARA_ADDENDUM (2_0_Draft) IPlugInEntryPoint2: public Steinberg::FUnknown
{
public:
    //! Extended version of bindToDocumentController ():
    //! bind the VST3 instance to an ARA document controller, switching it from "normal" operation
    //! to ARA mode with the assigned roles, and exposing the ARA plug-in extension.
    //! \p knownRoles encodes all roles that the host considered in its implementation and will explicitly
    //! assign to some plug-in instance(s), while \p assignedRoles describes the roles that this specific
    //! instance will fulfill.
    //! This may be called only once during the lifetime of the IAudioProcessor component, before
    //! the first call to setActive () or setState () or getProcessContextRequirements () or the
    //! creation of the GUI (see IPlugView).
    //! The ARA document controller must remain valid as long as the plug-in is in use - rendering,
    //! showing its UI, etc. However, when tearing down the plug-in, the actual order for deleting
    //! the IAudioProcessor instance and for deleting ARA document controller is undefined.
    //! Plug-ins must handle both potential destruction orders to allow for a simpler reference
    //! counting implementation on the host side.
    virtual const ARAPlugInExtensionInstance* PLUGIN_API bindToDocumentControllerWithRoles (ARADocumentControllerRef documentControllerRef,
                                                                        ARAPlugInInstanceRoleFlags knownRoles, ARAPlugInInstanceRoleFlags assignedRoles) = 0;
    static const Steinberg::FUID iid;
};

DECLARE_CLASS_IID (IPlugInEntryPoint2, 0xCD9A5913, 0xC9EB46D7, 0x96CA53AD, 0xD1DB89F5)

//! @}

//! @}

}   // namespace ARA

#include "pluginterfaces/base/falignpop.h"

ARA_DISABLE_VST3_WARNINGS_END

#endif // ARAVST3_h
