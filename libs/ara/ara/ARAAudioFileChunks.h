//------------------------------------------------------------------------------
//! \file       ARAAudioFileChunks.h
//!             definition of the audio file chunks related to ARA partial persistency
//! \project    ARA API Specification
//! \copyright  Copyright (c) 2018-2025, Celemony Software GmbH, All Rights Reserved.
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


#ifndef ARAAudioFileChunks_h
#define ARAAudioFileChunks_h

#include "ARAInterface.h"

#if defined(__cplusplus) && !(ARA_DOXYGEN_BUILD)
namespace ARA
{
extern "C"
{
#endif

/***************************************************************************************************/
//! @defgroup ARAAudioFileChunks ARA Audio File Chunks
//!
//! To allow for distributing persistent ARA audio source state information together with the
//! underlying audio file in a way that is transparent to the plug-ins and can be supported by all
//! hosts, ARA 2.0 defines a format for embedding such states into standardized audio file chunks.
//! From there, they can be imported into any ARA document using @ref Partial_Document_Persistency.
//! \br
//! The most obvious use case for this is that it enables audio content providers to ship audio
//! files with properly validated, ready-to-use audio source analysis for multiple plug-ins (or
//! incompatible versions of a plug-in if needed).
//! For example, this allows for loading polyphonic audio loops into Melodyne without time-consuming
//! analysis and very quickly adjusting them to follow the song key and chord progression, making
//! the published audio material much more versatile to use in various productions.
//! Other scenarios where such file chunks are used include exporting data from one plug-in to
//! another, or adding ARA objects to a host document via dragging and dropping audio files from
//! a plug-in that either generates these files on the fly (e.g. export of layers in SpectraLayers)
//! or copies them from a built-in sound library.
//! \br
//! The ARA chunk should be evaluated by the host both when adding a new audio file to the
//! arrangement and when applying a new/different ARA plug-in for a region/file already used in
//! the arrangement.
//! Note that after loading the data, ARA content readers can be used to extract more information
//! about the audio source - such as tempo map, time and key signatures, etc.
//! \br
//! Plug-in vendors shall optimize the encoding of the audio source state information for audio file
//! chunks according to very different criteria compared to encoding the same state for regular
//! ARA song document archives:
//! The audio file states are going to be widely distributed and will be used over a long period
//! of time in very different contexts, whereas song documents are typically only used on a single
//! machine for a rather short time. Audio file archives therefore should emphasize small data size
//! over en-/decoding speed - encoding is only done once, and decoding only happens for a single
//! audio source at a time (compared to hundreds of audio sources in a typical song archive).
//! Even more important, audio file archives are likely going to be used across a wide range of
//! products versions and shall be stable across a long time. The encoding should therefore be as
//! much backwards compatible as possible, potentially even using different encoding based on the
//! current state of the audio source: if e.g. a particular non-backwards compatible feature of
//! the plug-in is not used in the given state, the plug-in can choose an older format to store
//! the data than if that particular feature was utilized.
//! For these reasons, audio file chunks will typically not use the ARAFactory::documentArchiveID
//! but instead one of the IDs listed in ARAFactory::compatibleDocumentArchiveIDs.
//! \br
//! Creating audio file chunks may not be meaningful nor supported for any given plug-in. If for
//! example the plug-in does not perform any costly analysis and has no relevant editable audio
//! source state, there is no reason to create audio file chunks for it. Therefore, creating such
//! chunks is currently done only through dedicated authoring tools (such as Melodyne's standalone
//! version) and not directly available in ARA host applications.
//! \br
//! Covering both AIFF and WAVE formats, ARA stores its data by extending iXML chunks as specified
//! here: http://www.ixml.info
//! Inside the iXML document, there's a custom tag \<ARA\> that encloses a dictionary of audio
//! source archives, encoded as array tagged \<audioSources\>. Each entry in the array is intended
//! for a different plug-in (or incompatible version of a plug-in) and contains the tag
//! \<documentArchiveID\> which also functions as the key for the dictionary, and associated data
//! which includes the actual binary archive and meta information, for example:
//! \code{.xml}
//! <ARA>
//!     <audioSources>
//!         <audioSource>
//!             <documentArchiveID>com.celemony.ara.audiosourcedescription.13</documentArchiveID>
//!             <openAutomatically>false</openAutomatically>
//!             <suggestedPlugIn>
//!                 <plugInName>Melodyne</plugInName>
//!                 <lowestSupportedVersion>5.0.0</lowestSupportedVersion>
//!                 <manufacturerName>Celemony</manufacturerName>
//!                 <informationURL>https://www.celemony.com</informationURL>
//!             </suggestedPlugIn>
//!             <persistentID>59D4874F-FA5A-4FE8-BAC6-0E8BC5F6184A</persistentID>
//!             <archiveData>TW9pbiBEdQ==</archiveData>
//!         </audioSource>
//!         <!-- ... potentially more archives keyed by different documentArchiveIDs here ... -->
//!     </audioSources>
//! </ARA>
//! \endcode
//! @{

#if defined(__cplusplus)
    //! Name of the XML element that contains the vendor-specific iXML sub-tree for ARA.
    constexpr auto kARAXMLName_ARAVendorKeyword { "ARA" };

    //! Name of the XML element that contains the dictionary of audio source archives inside the ARA sub-tree.
    constexpr auto kARAXMLName_AudioSources { "audioSources" };

    //! Name of each XML element inside the dictionary of audio source archives.
    constexpr auto kARAXMLName_AudioSource { "audioSource" };

    //! Name of the XML element inside an audio source archive that acts as unique dictionary key
    //! for the list of audio source archives and identifies the opaque archive content.
    //! string value, see ARAFactory::documentArchiveID and ARAFactory::compatibleDocumentArchiveIDs.
    constexpr auto kARAXMLName_DocumentArchiveID { "documentArchiveID" };
    //! Name of the XML element inside an audio source archive that indicates whether the host should
    //! immediately load the archive data into a new audio source object and create an audio modification
    //! and playback region for it, or else import the audio file without ARA initially and only load
    //! the ARA archive later on demand when the user manually requests it by adding a matching plug-in.
    //! boolean value ("true" or "false").
    constexpr auto kARAXMLName_OpenAutomatically { "openAutomatically" };
    //! Name of the XML element inside an audio source archive that indicates whether the host should
    //! create a new audio modification each time the file/audio source is dragged into the song,
    //! or re-use the initial audio modification created upon the first drag.
    //! boolean value ("true" or "false").
    constexpr auto kARAXMLName_CreateDistinctAudioModification { "createDistinctAudioModification" };
    //! Name of the XML element inside an audio source archive that provides user-readable information
    //! about the plug-in for which the archive was originally created. This can be used for proper
    //! error messages, e.g. if openAutomatically is true but no plug-in compatible with the archive's
    //! given documentArchiveID is installed.
    constexpr auto kARAXMLName_SuggestedPlugIn { "suggestedPlugIn" };
    //! Name of the XML element inside an audio source archive that encodes the persistent ID that
    //! was assigned to the audio source when creating the archive. When loading the archive, the
    //! plug-in will use this persistent ID to find the target object to extract the state to.
    //! string value, see ARAAudioSourceProperties::persistentID, see ARARestoreObjectsFilter.
    constexpr auto kARAXMLName_PersistentID { "persistentID" };
    //! Name of the XML element inside an audio source archive that encodes the actual binary data
    //! of the archive in Base64 format, with the possible addition of line feeds as allowed by MIME.
    //! Note that it is preferred to encode without line feeds, but decoders must handle both cases.
    //! string value, see ARAArchivingControllerInterface, see https://tools.ietf.org/html/rfc4648.
    constexpr auto kARAXMLName_ArchiveData { "archiveData" };

    //! Name of the XML element inside a suggested plug-in element that encodes the plug-in name as string.
    constexpr auto kARAXMLName_PlugInName { "plugInName" };
    //! Name of the XML element inside a suggested plug-in element that encodes the minimum version
    //! of the plug-in that is compatible with this archive as string.
    constexpr auto kARAXMLName_LowestSupportedVersion { "lowestSupportedVersion" };
    //! Name of the XML element inside a suggested plug-in element that encodes the plug-in manufacturer as string.
    constexpr auto kARAXMLName_ManufacturerName { "manufacturerName" };
    //! Name of the XML element inside a suggested plug-in element that encodes the plug-in information URL as string.
    constexpr auto kARAXMLName_InformationURL { "informationURL" };
#else
    #define kARAXMLName_ARAVendorKeyword "ARA"

    #define kARAXMLName_AudioSources "audioSources"

    #define kARAXMLName_AudioSource "audioSource"

    #define kARAXMLName_DocumentArchiveID "documentArchiveID"
    #define kARAXMLName_OpenAutomatically "openAutomatically"
    #define kARAXMLName_SuggestedPlugIn "suggestedPlugIn"
    #define kARAXMLName_PersistentID "persistentID"
    #define kARAXMLName_ArchiveData "archiveData"

    #define kARAXMLName_PlugInName "plugInName"
    #define kARAXMLName_LowestSupportedVersion "lowestSupportedVersion"
    #define kARAXMLName_ManufacturerName "manufacturerName"
    #define kARAXMLName_InformationURL "informationURL"
#endif

//! @}

#if defined(__cplusplus) && !(ARA_DOXYGEN_BUILD)
}   // extern "C"
}   // namespace ARA
#endif

#endif // ARAAudioFileChunks_h
