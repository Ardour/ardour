#ifndef __AAFPropertyIDs_h__
#define __AAFPropertyIDs_h__

// https://github.com/nevali/aaf/blob/a03404ad8dc371757f3847b8dae0a9d70fff6a4e/ref-impl/include/OM/OMDictionary.h
#define PID_Root_MetaDictionary                                 0x0001
#define PID_Root_Header                                         0x0002




// AAF property identifiers (PIDs).
//

// A property is identified by a globally unique 16-byte
// identifier. To save space in an AAF file we store a
// 2-byte file unique PID with each property.
// The mapping for a particular file is recorded in the
// dictionary contained in that file.
// For the predefined properties we optimize by using a
// fixed, compiled-in mapping.
// This file defines that mapping.
//
#define PID_InterchangeObject_ObjClass                            0x0101
#define PID_InterchangeObject_Generation                          0x0102
#define PID_Component_DataDefinition                              0x0201
#define PID_Component_Length                                      0x0202
#define PID_Component_KLVData                                     0x0203
#define PID_Component_UserComments                                0x0204
#define PID_Component_Attributes                                  0x0205
#define PID_EdgeCode_Start                                        0x0401
#define PID_EdgeCode_FilmKind                                     0x0402
#define PID_EdgeCode_CodeFormat                                   0x0403
#define PID_EdgeCode_Header                                       0x0404
#define PID_EssenceGroup_Choices                                  0x0501
#define PID_EssenceGroup_StillFrame                               0x0502
#define PID_Event_Position                                        0x0601
#define PID_Event_Comment                                         0x0602
#define PID_GPITrigger_ActiveState                                0x0801
#define PID_CommentMarker_Annotation                              0x0901
#define PID_OperationGroup_Operation                              0x0B01
#define PID_OperationGroup_InputSegments                          0x0B02
#define PID_OperationGroup_Parameters                             0x0B03
#define PID_OperationGroup_BypassOverride                         0x0B04
#define PID_OperationGroup_Rendering                              0x0B05
#define PID_NestedScope_Slots                                     0x0C01
#define PID_Pulldown_InputSegment                                 0x0D01
#define PID_Pulldown_PulldownKind                                 0x0D02
#define PID_Pulldown_PulldownDirection                            0x0D03
#define PID_Pulldown_PhaseFrame                                   0x0D04
#define PID_ScopeReference_RelativeScope                          0x0E01
#define PID_ScopeReference_RelativeSlot                           0x0E02
#define PID_Selector_Selected                                     0x0F01
#define PID_Selector_Alternates                                   0x0F02
#define PID_Sequence_Components                                   0x1001
#define PID_SourceReference_SourceID                              0x1101
#define PID_SourceReference_SourceMobSlotID                       0x1102
#define PID_SourceReference_ChannelIDs                            0x1103
#define PID_SourceReference_MonoSourceSlotIDs                     0x1104
#define PID_SourceClip_StartTime                                  0x1201
#define PID_SourceClip_FadeInLength                               0x1202
#define PID_SourceClip_FadeInType                                 0x1203
#define PID_SourceClip_FadeOutLength                              0x1204
#define PID_SourceClip_FadeOutType                                0x1205
#define PID_HTMLClip_BeginAnchor                                  0x1401
#define PID_HTMLClip_EndAnchor                                    0x1402
#define PID_Timecode_Start                                        0x1501
#define PID_Timecode_FPS                                          0x1502
#define PID_Timecode_Drop                                         0x1503
#define PID_TimecodeStream_SampleRate                             0x1601
#define PID_TimecodeStream_Source                                 0x1602
#define PID_TimecodeStream_SourceType                             0x1603
#define PID_TimecodeStream12M_IncludeSync                         0x1701
#define PID_Transition_OperationGroup                             0x1801
#define PID_Transition_CutPoint                                   0x1802
#define PID_ContentStorage_Mobs                                   0x1901
#define PID_ContentStorage_EssenceData                            0x1902
#define PID_ControlPoint_Value                                    0x1A02
#define PID_ControlPoint_Time                                     0x1A03
#define PID_ControlPoint_EditHint                                 0x1A04
#define PID_DefinitionObject_Identification                       0x1B01
#define PID_DefinitionObject_Name                                 0x1B02
#define PID_DefinitionObject_Description                          0x1B03
#define PID_OperationDefinition_DataDefinition                    0x1E01
#define PID_OperationDefinition_IsTimeWarp                        0x1E02
#define PID_OperationDefinition_DegradeTo                         0x1E03
#define PID_OperationDefinition_OperationCategory                 0x1E06
#define PID_OperationDefinition_NumberInputs                      0x1E07
#define PID_OperationDefinition_Bypass                            0x1E08
#define PID_OperationDefinition_ParametersDefined                 0x1E09
#define PID_ParameterDefinition_Type                              0x1F01
#define PID_ParameterDefinition_DisplayUnits                      0x1F03
#define PID_PluginDefinition_PluginCategory                       0x2203
#define PID_PluginDefinition_VersionNumber                        0x2204
#define PID_PluginDefinition_VersionString                        0x2205
#define PID_PluginDefinition_Manufacturer                         0x2206
#define PID_PluginDefinition_ManufacturerInfo                     0x2207
#define PID_PluginDefinition_ManufacturerID                       0x2208
#define PID_PluginDefinition_Platform                             0x2209
#define PID_PluginDefinition_MinPlatformVersion                   0x220A
#define PID_PluginDefinition_MaxPlatformVersion                   0x220B
#define PID_PluginDefinition_Engine                               0x220C
#define PID_PluginDefinition_MinEngineVersion                     0x220D
#define PID_PluginDefinition_MaxEngineVersion                     0x220E
#define PID_PluginDefinition_PluginAPI                            0x220F
#define PID_PluginDefinition_MinPluginAPI                         0x2210
#define PID_PluginDefinition_MaxPluginAPI                         0x2211
#define PID_PluginDefinition_SoftwareOnly                         0x2212
#define PID_PluginDefinition_Accelerator                          0x2213
#define PID_PluginDefinition_Locators                             0x2214
#define PID_PluginDefinition_Authentication                       0x2215
#define PID_PluginDefinition_DefinitionObject                     0x2216
#define PID_CodecDefinition_FileDescriptorClass                   0x2301
#define PID_CodecDefinition_DataDefinitions                       0x2302
#define PID_ContainerDefinition_EssenceIsIdentified               0x2401
#define PID_Dictionary_OperationDefinitions                       0x2603
#define PID_Dictionary_ParameterDefinitions                       0x2604
#define PID_Dictionary_DataDefinitions                            0x2605
#define PID_Dictionary_PluginDefinitions                          0x2606
#define PID_Dictionary_CodecDefinitions                           0x2607
#define PID_Dictionary_ContainerDefinitions                       0x2608
#define PID_Dictionary_InterpolationDefinitions                   0x2609
#define PID_Dictionary_KLVDataDefinitions                         0x260A
#define PID_Dictionary_TaggedValueDefinitions                     0x260B
#define PID_EssenceData_MobID                                     0x2701
#define PID_EssenceData_Data                                      0x2702
#define PID_EssenceData_SampleIndex                               0x2B01
#define PID_EssenceDescriptor_Locator                             0x2F01
#define PID_FileDescriptor_SampleRate                             0x3001
#define PID_FileDescriptor_Length                                 0x3002
#define PID_FileDescriptor_ContainerFormat                        0x3004
#define PID_FileDescriptor_CodecDefinition                        0x3005
#define PID_FileDescriptor_LinkedSlotID                           0x3006
#define PID_AIFCDescriptor_Summary                                0x3101
#define PID_DigitalImageDescriptor_Compression                    0x3201
#define PID_DigitalImageDescriptor_StoredHeight                   0x3202
#define PID_DigitalImageDescriptor_StoredWidth                    0x3203
#define PID_DigitalImageDescriptor_SampledHeight                  0x3204
#define PID_DigitalImageDescriptor_SampledWidth                   0x3205
#define PID_DigitalImageDescriptor_SampledXOffset                 0x3206
#define PID_DigitalImageDescriptor_SampledYOffset                 0x3207
#define PID_DigitalImageDescriptor_DisplayHeight                  0x3208
#define PID_DigitalImageDescriptor_DisplayWidth                   0x3209
#define PID_DigitalImageDescriptor_DisplayXOffset                 0x320A
#define PID_DigitalImageDescriptor_DisplayYOffset                 0x320B
#define PID_DigitalImageDescriptor_FrameLayout                    0x320C
#define PID_DigitalImageDescriptor_VideoLineMap                   0x320D
#define PID_DigitalImageDescriptor_ImageAspectRatio               0x320E
#define PID_DigitalImageDescriptor_AlphaTransparency              0x320F
#define PID_DigitalImageDescriptor_TransferCharacteristic         0x3210
#define PID_DigitalImageDescriptor_ColorPrimaries                 0x3219
#define PID_DigitalImageDescriptor_CodingEquations                0x321A
#define PID_DigitalImageDescriptor_ImageAlignmentFactor           0x3211
#define PID_DigitalImageDescriptor_FieldDominance                 0x3212
#define PID_DigitalImageDescriptor_FieldStartOffset               0x3213
#define PID_DigitalImageDescriptor_FieldEndOffset                 0x3214
#define PID_DigitalImageDescriptor_SignalStandard                 0x3215
#define PID_DigitalImageDescriptor_StoredF2Offset                 0x3216
#define PID_DigitalImageDescriptor_DisplayF2Offset                0x3217
#define PID_DigitalImageDescriptor_ActiveFormatDescriptor         0x3218
#define PID_CDCIDescriptor_ComponentWidth                         0x3301
#define PID_CDCIDescriptor_HorizontalSubsampling                  0x3302
#define PID_CDCIDescriptor_ColorSiting                            0x3303
#define PID_CDCIDescriptor_BlackReferenceLevel                    0x3304
#define PID_CDCIDescriptor_WhiteReferenceLevel                    0x3305
#define PID_CDCIDescriptor_ColorRange                             0x3306
#define PID_CDCIDescriptor_PaddingBits                            0x3307
#define PID_CDCIDescriptor_VerticalSubsampling                    0x3308
#define PID_CDCIDescriptor_AlphaSamplingWidth                     0x3309
#define PID_CDCIDescriptor_ReversedByteOrder                      0x330B
#define PID_RGBADescriptor_PixelLayout                            0x3401
#define PID_RGBADescriptor_Palette                                0x3403
#define PID_RGBADescriptor_PaletteLayout                          0x3404
#define PID_RGBADescriptor_ScanningDirection                      0x3405
#define PID_RGBADescriptor_ComponentMaxRef                        0x3406
#define PID_RGBADescriptor_ComponentMinRef                        0x3407
#define PID_RGBADescriptor_AlphaMaxRef                            0x3408
#define PID_RGBADescriptor_AlphaMinRef                            0x3409
#define PID_TIFFDescriptor_IsUniform                              0x3701
#define PID_TIFFDescriptor_IsContiguous                           0x3702
#define PID_TIFFDescriptor_LeadingLines                           0x3703
#define PID_TIFFDescriptor_TrailingLines                          0x3704
#define PID_TIFFDescriptor_JPEGTableID                            0x3705
#define PID_TIFFDescriptor_Summary                                0x3706
#define PID_WAVEDescriptor_Summary                                0x3801
#define PID_FilmDescriptor_FilmFormat                             0x3901
#define PID_FilmDescriptor_FrameRate                              0x3902
#define PID_FilmDescriptor_PerforationsPerFrame                   0x3903
#define PID_FilmDescriptor_FilmAspectRatio                        0x3904
#define PID_FilmDescriptor_Manufacturer                           0x3905
#define PID_FilmDescriptor_Model                                  0x3906
#define PID_FilmDescriptor_FilmGaugeFormat                        0x3907
#define PID_FilmDescriptor_FilmBatchNumber                        0x3908
#define PID_TapeDescriptor_FormFactor                             0x3A01
#define PID_TapeDescriptor_VideoSignal                            0x3A02
#define PID_TapeDescriptor_TapeFormat                             0x3A03
#define PID_TapeDescriptor_Length                                 0x3A04
#define PID_TapeDescriptor_ManufacturerID                         0x3A05
#define PID_TapeDescriptor_Model                                  0x3A06
#define PID_TapeDescriptor_TapeBatchNumber                        0x3A07
#define PID_TapeDescriptor_TapeStock                              0x3A08
#define PID_Header_ByteOrder                                      0x3B01
#define PID_Header_LastModified                                   0x3B02
#define PID_Header_Content                                        0x3B03
#define PID_Header_Dictionary                                     0x3B04
#define PID_Header_Version                                        0x3B05
#define PID_Header_IdentificationList                             0x3B06
#define PID_Header_ObjectModelVersion                             0x3B07
#define PID_Header_OperationalPattern                             0x3B09
#define PID_Header_EssenceContainers                              0x3B0A
#define PID_Header_DescriptiveSchemes                             0x3B0B
#define PID_Identification_CompanyName                            0x3C01
#define PID_Identification_ProductName                            0x3C02
#define PID_Identification_ProductVersion                         0x3C03
#define PID_Identification_ProductVersionString                   0x3C04
#define PID_Identification_ProductID                              0x3C05
#define PID_Identification_Date                                   0x3C06
#define PID_Identification_ToolkitVersion                         0x3C07
#define PID_Identification_Platform                               0x3C08
#define PID_Identification_GenerationAUID                         0x3C09
#define PID_NetworkLocator_URLString                              0x4001
#define PID_TextLocator_Name                                      0x4101
#define PID_Mob_MobID                                             0x4401
#define PID_Mob_Name                                              0x4402
#define PID_Mob_Slots                                             0x4403
#define PID_Mob_LastModified                                      0x4404
#define PID_Mob_CreationTime                                      0x4405
#define PID_Mob_UserComments                                      0x4406
#define PID_Mob_KLVData                                           0x4407
#define PID_Mob_Attributes                                        0x4409
#define PID_Mob_UsageCode                                         0x4408
#define PID_CompositionMob_DefaultFadeLength                      0x4501
#define PID_CompositionMob_DefFadeType                            0x4502
#define PID_CompositionMob_DefFadeEditUnit                        0x4503
#define PID_CompositionMob_Rendering                              0x4504
#define PID_SourceMob_EssenceDescription                          0x4701
#define PID_MobSlot_SlotID                                        0x4801
#define PID_MobSlot_SlotName                                      0x4802
#define PID_MobSlot_Segment                                       0x4803
#define PID_MobSlot_PhysicalTrackNumber                           0x4804
#define PID_EventMobSlot_EditRate                                 0x4901
#define PID_EventMobSlot_EventSlotOrigin                          0x4902
#define PID_TimelineMobSlot_EditRate                              0x4B01
#define PID_TimelineMobSlot_Origin                                0x4B02
#define PID_TimelineMobSlot_MarkIn                                0x4B03
#define PID_TimelineMobSlot_MarkOut                               0x4B04
#define PID_TimelineMobSlot_UserPos                               0x4B05
#define PID_Parameter_Definition                                  0x4C01
#define PID_ConstantValue_Value                                   0x4D01
#define PID_VaryingValue_Interpolation                            0x4E01
#define PID_VaryingValue_PointList                                0x4E02
#define PID_TaggedValue_Name                                      0x5001
#define PID_TaggedValue_Value                                     0x5003
#define PID_KLVData_Value                                         0x5101
#define PID_DescriptiveMarker_DescribedSlots                      0x6102
#define PID_DescriptiveMarker_Description                         0x6101
#define PID_SoundDescriptor_AudioSamplingRate                     0x3D03
#define PID_SoundDescriptor_Locked                                0x3D02
#define PID_SoundDescriptor_AudioRefLevel                         0x3D04
#define PID_SoundDescriptor_ElectroSpatial                        0x3D05
#define PID_SoundDescriptor_Channels                              0x3D07
#define PID_SoundDescriptor_QuantizationBits                      0x3D01
#define PID_SoundDescriptor_DialNorm                              0x3D0C
#define PID_SoundDescriptor_Compression                           0x3D06
#define PID_DataEssenceDescriptor_DataEssenceCoding               0x3E01
#define PID_MultipleDescriptor_FileDescriptors                    0x3F01
#define PID_DescriptiveClip_DescribedSlotIDs                      0x6103
#define PID_AES3PCMDescriptor_Emphasis                            0x3D0D
#define PID_AES3PCMDescriptor_BlockStartOffset                    0x3D0F
#define PID_AES3PCMDescriptor_AuxBitsMode                         0x3D08
#define PID_AES3PCMDescriptor_ChannelStatusMode                   0x3D10
#define PID_AES3PCMDescriptor_FixedChannelStatusData              0x3D11
#define PID_AES3PCMDescriptor_UserDataMode                        0x3D12
#define PID_AES3PCMDescriptor_FixedUserData                       0x3D13
#define PID_PCMDescriptor_BlockAlign                              0x3D0A
#define PID_PCMDescriptor_SequenceOffset                          0x3D0B
#define PID_PCMDescriptor_AverageBPS                              0x3D09
#define PID_PCMDescriptor_ChannelAssignment                       0x3D32
#define PID_PCMDescriptor_PeakEnvelopeVersion                     0x3D29
#define PID_PCMDescriptor_PeakEnvelopeFormat                      0x3D2A
#define PID_PCMDescriptor_PointsPerPeakValue                      0x3D2B
#define PID_PCMDescriptor_PeakEnvelopeBlockSize                   0x3D2C
#define PID_PCMDescriptor_PeakChannels                            0x3D2D
#define PID_PCMDescriptor_PeakFrames                              0x3D2E
#define PID_PCMDescriptor_PeakOfPeaksPosition                     0x3D2F
#define PID_PCMDescriptor_PeakEnvelopeTimestamp                   0x3D30
#define PID_PCMDescriptor_PeakEnvelopeData                        0x3D31
#define PID_KLVDataDefinition_KLVDataType                         0x4D12
#define PID_AuxiliaryDescriptor_MimeType                          0x4E11
#define PID_AuxiliaryDescriptor_CharSet                           0x4E12
#define PID_RIFFChunk_ChunkID                                     0x4F01
#define PID_RIFFChunk_ChunkLength                                 0x4F02
#define PID_RIFFChunk_ChunkData                                   0x4F03
#define PID_BWFImportDescriptor_QltyFileSecurityReport            0x3D15
#define PID_BWFImportDescriptor_QltyFileSecurityWave              0x3D16
#define PID_BWFImportDescriptor_BextCodingHistory                 0x3D21
#define PID_BWFImportDescriptor_QltyBasicData                     0x3D22
#define PID_BWFImportDescriptor_QltyStartOfModulation             0x3D23
#define PID_BWFImportDescriptor_QltyQualityEvent                  0x3D24
#define PID_BWFImportDescriptor_QltyEndOfModulation               0x3D25
#define PID_BWFImportDescriptor_QltyQualityParameter              0x3D26
#define PID_BWFImportDescriptor_QltyOperatorComment               0x3D27
#define PID_BWFImportDescriptor_QltyCueSheet                      0x3D28
#define PID_BWFImportDescriptor_UnknownBWFChunks                  0x3D33

#define PID_MPEGVideoDescriptor_SingleSequence                    0x0000
#define PID_MPEGVideoDescriptor_ConstantBPictureCount             0x0000
#define PID_MPEGVideoDescriptor_CodedContentScanning              0x0000
#define PID_MPEGVideoDescriptor_LowDelay                          0x0000
#define PID_MPEGVideoDescriptor_ClosedGOP                         0x0000
#define PID_MPEGVideoDescriptor_IdenticalGOP                      0x0000
#define PID_MPEGVideoDescriptor_MaxGOP                            0x0000
#define PID_MPEGVideoDescriptor_MaxBPictureCount                  0x0000
#define PID_MPEGVideoDescriptor_BitRate                           0x0000
#define PID_MPEGVideoDescriptor_ProfileAndLevel                   0x0000

/*  MULTIPLE_VALUE_MATCHES
	https://sourceforge.net/p/aaf/mailman/aaf-commits/?viewmonth=200704&page=4
	"Log message : Set MPEGVideoDescriptor PIDs to 0x0000 (dynamic)"

! const int PID_MPEGVideoDescriptor_SingleSequence                 = 0xFF01;
! const int PID_MPEGVideoDescriptor_ConstantBPictureCount          = 0xFF02;
! const int PID_MPEGVideoDescriptor_CodedContentScanning           = 0xFF03;
! const int PID_MPEGVideoDescriptor_LowDelay                       = 0xFF04;
! const int PID_MPEGVideoDescriptor_ClosedGOP                      = 0xFF05;
! const int PID_MPEGVideoDescriptor_IdenticalGOP                   = 0xFF06;
! const int PID_MPEGVideoDescriptor_MaxGOP                         = 0xFF07;
! const int PID_MPEGVideoDescriptor_MaxBPictureCount               = 0xFF08;
! const int PID_MPEGVideoDescriptor_BitRate                        = 0xFF09;
! const int PID_MPEGVideoDescriptor_ProfileAndLevel                = 0xFF10;
*/


#define PID_ClassDefinition_ParentClass                           0x0008
#define PID_ClassDefinition_Properties                            0x0009
#define PID_ClassDefinition_IsConcrete                            0x000A
#define PID_PropertyDefinition_Type                               0x000B
#define PID_PropertyDefinition_IsOptional                         0x000C
#define PID_PropertyDefinition_LocalIdentification                0x000D
#define PID_PropertyDefinition_IsUniqueIdentifier                 0x000E
#define PID_TypeDefinitionInteger_Size                            0x000F
#define PID_TypeDefinitionInteger_IsSigned                        0x0010
#define PID_TypeDefinitionStrongObjectReference_ReferencedType    0x0011
#define PID_TypeDefinitionWeakObjectReference_ReferencedType      0x0012
#define PID_TypeDefinitionWeakObjectReference_TargetSet           0x0013
#define PID_TypeDefinitionEnumeration_ElementType                 0x0014
#define PID_TypeDefinitionEnumeration_ElementNames                0x0015
#define PID_TypeDefinitionEnumeration_ElementValues               0x0016
#define PID_TypeDefinitionFixedArray_ElementType                  0x0017
#define PID_TypeDefinitionFixedArray_ElementCount                 0x0018
#define PID_TypeDefinitionVariableArray_ElementType               0x0019
#define PID_TypeDefinitionSet_ElementType                         0x001A
#define PID_TypeDefinitionString_ElementType                      0x001B
#define PID_TypeDefinitionRecord_MemberTypes                      0x001C
#define PID_TypeDefinitionRecord_MemberNames                      0x001D
#define PID_TypeDefinitionRename_RenamedType                      0x001E
#define PID_TypeDefinitionExtendibleEnumeration_ElementNames      0x001F
#define PID_TypeDefinitionExtendibleEnumeration_ElementValues     0x0020
#define PID_MetaDefinition_Identification                         0x0005
#define PID_MetaDefinition_Name                                   0x0006
#define PID_MetaDefinition_Description                            0x0007
#define PID_MetaDictionary_ClassDefinitions                       0x0003
#define PID_MetaDictionary_TypeDefinitions                        0x0004

#endif // ! __AAFPropertyIDs_h__
