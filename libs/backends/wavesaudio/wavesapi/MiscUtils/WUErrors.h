/*
    Copyright (C) 2014 Waves Audio Ltd.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __WUErrors_h__
    #define __WUErrors_h__

/* Copy to include:
#include "WUErrors.h"
*/

#include "BasicTypes/WUTypes.h"

// General errors
//const WTErr eNoErr =  0; // moved to #include "WavesPublicAPI/WTErr.h"
const WTErr eGenericErr						= -1;
const WTErr eUserCanceled 	                = -2;
const WTErr eUnknownErr						= -3;
const WTErr eExceptionErr					= -4;
const WTErr eEndianError					= -5;
const WTErr eThreadSafeError				= -6;
const WTErr eSomeThingNotInitailzed			= -7;
const WTErr eWrongObjectState    			= -8; //!< object was not in an acceptable state
const WTErr eUninitalized					= -9;
const WTErr eDeprecated						= -10;
const WTErr eCommandLineParameter			= -11;
const WTErr eNotANumber						= -12; //!< expected a number but none was found
const WTErr eNotJustANumber					= -13; //!< expected a number and found one but also other stuff (e.g. "123XYZ")
const WTErr eNegativeNumber					= -14; //!< expected a positive number and found a negative
const WTErr eTimeOut                        = -15; //!< something timed out
const WTErr eCoreAudioFailed				= -16; //!< Error in a core audio call
const WTErr eSomeThingInitailzedTwice		= -17; 
const WTErr eGenerateHelpInfo				= -18; 
const WTErr eOutOfRangeNumber				= -19; 
const WTErr eMacOnlyCode    				= -20; 
const WTErr eWinOnlyCode				    = -21; 
const WTErr eAppLaunchFailed     		    = -22; //!< failed to launch an application
const WTErr eAppTerminateFailed    		    = -23; //!< failed to terminate an application
const WTErr eAppReturnedError               = -24; //!< Non zero exit code from application
const WTErr eNotImplemented                 = -25; //!< Function is not implmemented
const WTErr eNotEmpty		                = -26; //!< Something was expected to be empty but is not
const WTErr eAsioFailed						= -27;

// File Manager errors
const WTErr eFMNoSuchVolume					= -1001; 
const WTErr eFMFileNotFound					= -1002;
const WTErr eFMFileAllreadyExists			= -1003;
const WTErr eFMAllreadyOpenWithWritePerm	= -1004;
const WTErr eFMEndOfFile					= -1005;
const WTErr eFMPermissionErr				= -1006;
const WTErr eFMBusyErr						= -1007;
const WTErr eFMOpenFailed					= -1008;
const WTErr eFMTranslateFileNameFailed		= -1009;
const WTErr eFMWTPathRefCreationFailed  	= -1010;
const WTErr eFMReadFailed					= -1011;
const WTErr eFMIllegalPathRef				= -1012;
const WTErr eFMFileNotOpened				= -1013;
const WTErr eFMFileSizeTooBig				= -1014;
const WTErr eFMNoSuchDomain					= -1015; 
const WTErr eFMNoSuchSystemFolder			= -1016; 
const WTErr eFMWrongParameters				= -1017;
const WTErr eFMIsNotAFolder					= -1018;
const WTErr eFMIsAFolder					= -1019;
const WTErr eFMIsNotAFile					= -1020;
const WTErr eFMIsAFile						= -1021;
const WTErr eFMDeleteFailed					= -1022;
const WTErr eFMCreateFailed					= -1023;
const WTErr eFMPathTooLong					= -1024;
const WTErr eFMIOError						= -1025;
const WTErr eFMIllegalOpenFileRef			= -1026;
const WTErr eFMDiskFull						= -1027;
const WTErr eFMFileNotEmpty					= -1028;
const WTErr eFMEndOfFolder					= -1029;
const WTErr eFMSamePath						= -1030;
const WTErr eFMPathTooShort					= -1031;
const WTErr eFMIncompletePath				= -1032;
const WTErr eFMIsNoAFileSystemLink			= -1033;
const WTErr eFMSymlinkBroken				= -1034;
const WTErr eFMMoveFailed					= -1035;
const WTErr eFMWriteFailed                  = -1036;
const WTErr eFMTooManyOpenFiles				= -1037;
const WTErr eFMTooManySymlinks				= -1038;

// System errors
const WTErr	eGenericSystemError				= -2000;
const WTErr eSysNoEnvironmentVariable	    = -2001;
const WTErr eDLLLoadingFailed				= -2002;
const WTErr eFuncPoinerNotFound				= -2003;
const WTErr eDLLNotFound					= -2004;
const WTErr eBundleNotLoaded    			= -2005;
const WTErr eBundleCreateFailed 			= -2006;
const WTErr eBundleExecutableNotFound		= -2007;
const WTErr	eNotABundle						= -2008;
const WTErr	eInvalideDate					= -2009;
const WTErr eNoNetDevice                    = -2010;
const WTErr eCacheCreatedFromResource       = -2011;
const WTErr eNotAValidApplication           = -2012;

// Resource Manager errors
const WTErr eRMResNotFound   	  	= -3000;
const WTErr eRMResExists     		= -3001; //!< a resource exist even though it's not expected to 
const WTErr eRMContainerNotFound   	= -3002; //!< The container was not found in the list of containers
const WTErr eRMResRefNotFound    	= -3003; //!< The resRef was not found in container's resource list
const WTErr eRMInvalidResRef			= -3004;
const WTErr eRMInvalidResContainer  = -3005;
const WTErr eRMInvalidNativeResContainer  = -3006;
const WTErr eRMAttachResContainerFailed   = -3007;
const WTErr eRMInvalidResID			= -3008;
const WTErr eRMResUpdateFailed		= -3009;

// Graphic Manager & GUI errors
const WTErr eGMIsNotInitailzed		= -3500;
const WTErr eGMInvalidImage			= -3501;
const WTErr eGMGenericErr			= -3502;
const WTErr eGMNoCurrentContext		= -3503;
const WTErr eGUISkinNotFound  		= -3504;
const WTErr eGMNoVertices           = -3505;
const WTErr eGMNoColors             = -3506;
const WTErr eGMNoTexture            = -3507;
const WTErr eGMIncompatibleOGLVersion 	= -3508;
const WTErr eGMNoDeviceContext      	= -3509;
const WTErr eGMNoPixelFormat        	= -3510;
const WTErr eGMNoOGLContext         	= -3511;
const WTErr eGMNoOGLContextSharing  	= -3512;
const WTErr eGMUnsupportedImageFormat  	= -3513;
const WTErr eGMUninitializedContext  	= -3514;
const WTErr eControlOutOfRange		 	= -3515;
const WTErr eGMUninitializedFont    	= -3516;
const WTErr eGMInvalidFontDrawMethod    = -3517;
const WTErr eGMUnreleasedTextures       = -3518;
const WTErr eGMWrongThread		        = -3519;
const WTErr eGMDontCommitDraw			= -3520;
// Errors in the -5000 -> -5999 are defined in Waves-incs.h

// Memory errors
const WTErr eMemNewFailed           = -4001; //!< Something = new CSomething, returned null
const WTErr eMemNewTPtrFailed       = -4002; //!< NewTPtr or NewTPtrClear failed
const WTErr eMemNullPointer         = -4003; //!< a null pointer was encountered where it should not
const WTErr eMemObjNotInitialized   = -4004;
const WTErr eMemBuffTooShort        = -4005; //!< the buffer in question did not have enough space for the operation
const WTErr eInstanciationFailed    = -4006;
const WTErr eMemAddressSpaceError   = -4007; //!< memory falls outside the legal address space
const WTErr eMemBadPointer          = -4008; 
const WTErr eMemOutOfMemory         = -4009; 

// XML Errors
const WTErr eXMLParserFailed        = -6001;
const WTErr eXMLTreeNotValid        = -6002;
const WTErr eXMLTreeEmpty     	    = -6003;
const WTErr eXMLElementMissing      = -6004;
const WTErr eXMLElementUninitalized  = -6005; //!< element was default constructed it has not element name, etc..
const WTErr eXMLElementIncomplete  = -6006;		//!< XML parser did not complete building the element
const WTErr eXMLAttribMissing      = -6007;

// Preset errors
const WTErr ePresetFileProblem          	= -7860; 
const WTErr eInvalidFileFormatProblem   	= -7861; 
const WTErr ePresetLockedProblem        	= -7862; 
const WTErr ePresetInfoNotFound         	= -7863; 
const WTErr eDuplicatePluginSpecificTag     = -7959; 
const WTErr ePluginSpecifcNotExisting       = -7960; 
const WTErr eBuffSizeToSmall                = -7961; 
const WTErr eCreatingPopupWhereAnItemExists = -7962; 
const WTErr eDeletePluginSpecifcFailed      = -7963; 
const WTErr eFactoryPresetNumOutOfRange     = -7964; 
const WTErr eNoFactoryPresets               = -7965; 
const WTErr eLoadPresetToPlugin_vec_empty   = -7966; 
const WTErr eFactoryPresetNotFound          = -7967; 
const WTErr eCantCreateUserPrefFile         = -7968; 
const WTErr eDataFormatNotSupported         = -7969; 
const WTErr eCantLoadProcessFunction        = -7970; 
const WTErr eIllegalChunkIndex				= -7971; 
const WTErr eIllegalChunkID					= -7972; 
const WTErr	eIllegalChunkVersion            = -7973;


// Shell errors
const WTErr eNotAPluginFile                 = -8001;
const WTErr eFaildToLoadPluginDLL			= -8002;
const WTErr eNoPluginManager                = -8003;
const WTErr eGetAvailablePluginsFailed      = -8004;
const WTErr eNoPluginsAvailable             = -8005;
const WTErr ePluginSubComponentNotFound     = -8006;
const WTErr ePluginOpenFailed               = -8007;
const WTErr eSubComponentRejected    		= -8009; //!< user did not want this sub-component - probably through preferences
const WTErr eIncompatibleNumOfIOs           = -8010; //!< e.g. surround sub-component in stereo only shell
const WTErr eStemProblem					= -8011; //!< Some problem with stems
const WTErr	eComponentTypeNotSupported		= -8012;
const WTErr	ePluginNotLoaded				= -8013;
const WTErr	ePluginInstanceNotCreate		= -8014;
const WTErr	ePluginAlgNotCreate				= -8015;
const WTErr	ePluginGUINotCreate				= -8016;
const WTErr	eMissmatchChannelCount			= -8017;
const WTErr eIncompatibleVersion            = -8018;
const WTErr eIncompatibleAffiliation        = -8019;
const WTErr eNoSubComponentsFound           = -8020;

// Net-shell errors
const WTErr eNetShellInitFailed             = -9001;

// Protection errors
const WTErr eWLSLicenseFileNotFound  = -10001;
const WTErr eWLSPluginNotAuthorized  = -10002;
const WTErr eWLSNoLicenseForPlugin   = -10003;
const WTErr eWLSInvalidLicenseFileName   = -10004;
const WTErr eWLSInvalidLicenseFileContents   = -10005;
const WTErr eWLSInvalidDeviceID     = -10006;
const WTErr eWLSInvalidClientID     = -10007;
const WTErr eWLSLicenseFileDownloadFailed     = -10008;
const WTErr eWLSNoLicensesForClientOrDevice   = -10009;
const WTErr eWLSNoLicensesForSomePlugins   = -10010;

// Communication errors
const WTErr eCommEndOfRecievedMessage		= -11001;
const WTErr eCommSocketDisconnected			= -11002;

// Window Manager Errors
const WTErr eWMEventNotHandled				= -12001;
const WTErr eWMDisposeViewFailed			= -12002;

// Plugin View Manager Errors
const WTErr ePVMPlatformNotSupported            = -13001;
const WTErr ePVMAlreadyInitialized              = -13002;
const WTErr ePVMIllegalParent                   = -13003;
const WTErr ePVMCannotCreateView                = -13004;
const WTErr ePVMNothingSelected                 = -13005;
const WTErr ePVMDisabledItemChosen              = -13006;
const WTErr ePVMMenuItemNotFound                = -13007;
const WTErr ePVMMenuItemNotASubMenu             = -13008;
const WTErr ePVMUnknownMenu                     = -13009;
const WTErr ePVMEmptyNativeViewRef              = -13010;
const WTErr ePVMGenericError                    = -13011;
const WTErr ePVMFunctionNotImplemented          = -13012;

// Plugin View Manager  - Menu Errors
const WTErr ePVMCannotCreateMenu                = -13501;
const WTErr ePVMCannotSetMenuFont               = -13502;
const WTErr ePVMCannotSetMenu                   = -13503;
const WTErr ePVMItemParentNotExists             = -13504;

// Plugin View Manager  - TextField Errors
const WTErr ePVMCannotCreateTextField           = -13553;
const WTErr ePVMCannotEmbedTextField            = -13554;
const WTErr ePVMNoTextToValidate                = -13555;
const WTErr ePVMTextTooLong                     = -13556;
const WTErr ePVMIllegalCharacter                = -13557;


// Meter Manager Errors
const WTErr eMM_MeterGetMeterValueForParameterNotConnected	= -14000 ;


//Surface Driver Manager Errors
const WTErr eSDM_SurfaceDriverAPIFailed = -14101;

// IPC Errors
const WTErr eIPC_CreateNamedPipeFailed         = -14200;
const WTErr eIPC_OpenPipeTimeout               = -14201;
const WTErr eIPC_DeleteNamedPipeFailed         = -14202;
const WTErr eIPC_SelectOnNamedPipeFailed       = -14203;
const WTErr eIPC_ReadFromNamedPipeFailed       = -14204;
const WTErr eIPC_ReadEndOfFileFromNamedPipe    = -14205;
const WTErr eIPC_CloseNamedPipeFailed          = -14206;
const WTErr eIPC_ParseArgsFailed               = -14207;
const WTErr eIPC_OpenPipeFailed                = -14208;
const WTErr eIPC_SendMsgFailed                 = -14209;
const WTErr eIPC_SendCommandInvalid            = -14210;
const WTErr eIPC_QtTestMode				       = -14211;	
const WTErr eIPC_ChangePermissionOnPipe   	   = -14212;	
const WTErr eIPC_ConnectionLost         	   = -14213;	

const WTErr eIPC_InvalidRole             	   = -14213;	
const WTErr eIPC_CreateNamedPipeM2SFailed      = -14214;
const WTErr eIPC_CreateNamedPipeS2MFailed      = -14215;
const WTErr eIPC_ChangePermissionOnPipeM2S     = -14216;	
const WTErr eIPC_ChangePermissionOnPipeS2M     = -14217;	
const WTErr eIPC_OpenReadPipeFailed            = -14218;	
const WTErr eIPC_OpenReadPipeDIsableSigPipe    = -14219;	
const WTErr eIPC_OpenWritePipeFailed           = -14220;	
const WTErr eIPC_WritePipeFailed               = -14221;	
const WTErr eIPC_WritePipeNotOpen              = -14222;	
const WTErr eIPC_WriteBufferResizeFailed       = -14223;	
const WTErr eIPC_NotConnectedSendMsgFailed     = -14224;	
const WTErr eIPC_OpenWritePipeWorkerStoping    = -14225;	
const WTErr eIPC_SoketSendFailed               = -14226;	
const WTErr eIPC_PtonFailed                    = -14227;	
const WTErr eIPC_SocketFailed                  = -14228;	
const WTErr eIPC_BindFailed                    = -14229;	
const WTErr eIPC_ListenFailed                  = -14230;	
const WTErr eIPC_ConnectFailed                 = -14231;	
const WTErr eIPC_WsaStartupFailed              = -14232;
const WTErr eIPC_UdpSocketCreateFailed         = -14233;
const WTErr eIPC_UdpSocketConnectFailed        = -14234;
const WTErr eIPC_UdpSocketBinFailed            = -14235;
const WTErr eIPC_SetBufferPreambleFailed       = -14226;	

// Database errors
const WTErr eDB_BatchRollback = -15501;

// inventory related errors
const WTErr eUnknown_Device = -16001;
const WTErr eInvNoDevice    = -16002;

// SG protocol service errors
const WTErr eSGProtocolService_Not_Running      = -17001;
const WTErr eSGProtocolService_Version_MisMatch = -17002;

// Error code related to Param
const WTErr eInvalidParam  = -18001;

#define WUIsError(theErrorCode) (eNoErr != (theErrorCode))
#define WUNoError(theErrorCode) (eNoErr == (theErrorCode))
#define WUThrowError(theErrorCode) {if(WUIsError(theErrorCode))throw (theErrorCode);}
#define WUThrowErrorIfNil(thePtr , theErrorCode) {if (0 == thePtr )throw (theErrorCode);}
#define WUThrowErrorIfFalse(theBool , theErrorCode) {if (!(theBool))throw (theErrorCode);}
#define WUThrowErrorCodeIfError(err,theErrorCode) {if(WUIsError(err))throw (theErrorCode);}

// Get the error string that match the error code.
DllExport const char* WTErrName(WTErr wtErr);

#endif //__WUErrors_h__:
