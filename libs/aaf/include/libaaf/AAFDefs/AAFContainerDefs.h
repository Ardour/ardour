#ifndef __ContainerDefinition_h__
#define __ContainerDefinition_h__

#include <libaaf/AAFTypes.h>

// AAF well-known ContainerDefinition instances
//

//{4313b572-d8ba-11d2-809b-006008143e6f}
static const aafUID_t AAFContainerDef_External = {
    0x4313b572,
    0xd8ba,
    0x11d2,
    {0x80, 0x9b, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};

//{4b1c1a46-03f2-11d4-80fb-006008143e6f}
static const aafUID_t AAFContainerDef_OMF = {
    0x4b1c1a46,
    0x03f2,
    0x11d4,
    {0x80, 0xfb, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};

//{4313b571-d8ba-11d2-809b-006008143e6f}
static const aafUID_t AAFContainerDef_AAF = {
    0x4313b571,
    0xd8ba,
    0x11d2,
    {0x80, 0x9b, 0x00, 0x60, 0x08, 0x14, 0x3e, 0x6f}};

//{42464141-000d-4d4f-060e-2b34010101ff}
static const aafUID_t AAFContainerDef_AAFMSS = {
    0x42464141,
    0x000d,
    0x4d4f,
    {0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0xff}};

//{4b464141-000d-4d4f-060e-2b34010101ff}
static const aafUID_t AAFContainerDef_AAFKLV = {
    0x4b464141,
    0x000d,
    0x4d4f,
    {0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0xff}};

//{58464141-000d-4d4f-060e-2b34010101ff}
static const aafUID_t AAFContainerDef_AAFXML = {
    0x58464141,
    0x000d,
    0x4d4f,
    {0x06, 0x0e, 0x2b, 0x34, 0x01, 0x01, 0x01, 0xff}};

//{0d010301-0201-0101-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_DefinedTemplate =
        {0x0d010301,
         0x0201,
         0x0101,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-0102-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_ExtendedTemplate =
        {0x0d010301,
         0x0201,
         0x0102,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-017f-060e-2b3404010102}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_50Mbps_PictureOnly = {
        0x0d010301,
        0x0201,
        0x017f,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02}};

//{0d010301-0201-0201-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_DefinedTemplate =
        {0x0d010301,
         0x0201,
         0x0201,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-0202-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_ExtendedTemplate =
        {0x0d010301,
         0x0201,
         0x0202,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-027f-060e-2b3404010102}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_50Mbps_PictureOnly =
        {0x0d010301,
         0x0201,
         0x027f,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02}};

//{0d010301-0201-0301-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_DefinedTemplate =
        {0x0d010301,
         0x0201,
         0x0301,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-0302-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_ExtendedTemplate =
        {0x0d010301,
         0x0201,
         0x0302,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-037f-060e-2b3404010102}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_40Mbps_PictureOnly = {
        0x0d010301,
        0x0201,
        0x037f,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02}};

//{0d010301-0201-0401-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_DefinedTemplate =
        {0x0d010301,
         0x0201,
         0x0401,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-0402-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_ExtendedTemplate =
        {0x0d010301,
         0x0201,
         0x0402,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-047f-060e-2b3404010102}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_40Mbps_PictureOnly =
        {0x0d010301,
         0x0201,
         0x047f,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02}};

//{0d010301-0201-0501-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_DefinedTemplate =
        {0x0d010301,
         0x0201,
         0x0501,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-0502-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_ExtendedTemplate =
        {0x0d010301,
         0x0201,
         0x0502,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-057f-060e-2b3404010102}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_625x50I_30Mbps_PictureOnly = {
        0x0d010301,
        0x0201,
        0x057f,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02}};

//{0d010301-0201-0601-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_DefinedTemplate =
        {0x0d010301,
         0x0201,
         0x0601,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-0602-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_ExtendedTemplate =
        {0x0d010301,
         0x0201,
         0x0602,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0201-067f-060e-2b3404010102}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_SMPTE_D10_525x5994I_30Mbps_PictureOnly =
        {0x0d010301,
         0x0201,
         0x067f,
         {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02}};

//{0d010301-0202-0101-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_IECDV_525x5994I_25Mbps = {
        0x0d010301,
        0x0202,
        0x0101,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-0102-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Clipwrapped_IECDV_525x5994I_25Mbps =
    {0x0d010301,
     0x0202,
     0x0102,
     {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-0201-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Framewrapped_IECDV_625x50I_25Mbps =
    {0x0d010301,
     0x0202,
     0x0201,
     {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-0202-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Clipwrapped_IECDV_625x50I_25Mbps = {
    0x0d010301,
    0x0202,
    0x0202,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-0301-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_IECDV_525x5994I_25Mbps_SMPTE322M = {
        0x0d010301,
        0x0202,
        0x0301,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-0302-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_IECDV_525x5994I_25Mbps_SMPTE322M = {
        0x0d010301,
        0x0202,
        0x0302,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-0401-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_IECDV_625x50I_25Mbps_SMPTE322M = {
        0x0d010301,
        0x0202,
        0x0401,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-0402-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_IECDV_625x50I_25Mbps_SMPTE322M = {
        0x0d010301,
        0x0202,
        0x0402,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-3f01-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_IECDV_UndefinedSource_25Mbps = {
        0x0d010301,
        0x0202,
        0x3f01,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-3f02-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_IECDV_UndefinedSource_25Mbps = {
        0x0d010301,
        0x0202,
        0x3f02,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-4001-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_525x5994I_25Mbps = {
        0x0d010301,
        0x0202,
        0x4001,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-4002-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_DVbased_525x5994I_25Mbps = {
        0x0d010301,
        0x0202,
        0x4002,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-4101-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_625x50I_25Mbps = {
        0x0d010301,
        0x0202,
        0x4101,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-4102-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Clipwrapped_DVbased_625x50I_25Mbps =
    {0x0d010301,
     0x0202,
     0x4102,
     {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-5001-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_525x5994I_50Mbps = {
        0x0d010301,
        0x0202,
        0x5001,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-5002-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_DVbased_525x5994I_50Mbps = {
        0x0d010301,
        0x0202,
        0x5002,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-5101-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_625x50I_50Mbps = {
        0x0d010301,
        0x0202,
        0x5101,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-5102-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Clipwrapped_DVbased_625x50I_50Mbps =
    {0x0d010301,
     0x0202,
     0x5102,
     {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-6001-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_1080x5994I_100Mbps = {
        0x0d010301,
        0x0202,
        0x6001,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-6002-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_DVbased_1080x5994I_100Mbps = {
        0x0d010301,
        0x0202,
        0x6002,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-6101-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_1080x50I_100Mbps = {
        0x0d010301,
        0x0202,
        0x6101,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-6102-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_DVbased_1080x50I_100Mbps = {
        0x0d010301,
        0x0202,
        0x6102,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-6201-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_720x5994P_100Mbps = {
        0x0d010301,
        0x0202,
        0x6201,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-6202-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_DVbased_720x5994P_100Mbps = {
        0x0d010301,
        0x0202,
        0x6202,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-6301-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_720x50P_100Mbps = {
        0x0d010301,
        0x0202,
        0x6301,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-6302-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_DVbased_720x50P_100Mbps = {
        0x0d010301,
        0x0202,
        0x6302,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-7f01-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_DVbased_UndefinedSource = {
        0x0d010301,
        0x0202,
        0x7f01,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0202-7f02-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_DVbased_UndefinedSource = {
        0x0d010301,
        0x0202,
        0x7f02,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0204-6001-060e-2b3404010102}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_MPEGES_VideoStream0_SID = {
        0x0d010301,
        0x0204,
        0x6001,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02}};

//{0d010301-0204-6107-060e-2b3404010102}
static const aafUID_t
    AAFContainerDef_MXFGC_CustomClosedGOPwrapped_MPEGES_VideoStream1_SID = {
        0x0d010301,
        0x0204,
        0x6107,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x02}};

//{0d010301-0205-0101-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_Uncompressed_525x5994I_720_422 = {
        0x0d010301,
        0x0205,
        0x0101,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-0102-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_525x5994I_720_422 = {
        0x0d010301,
        0x0205,
        0x0102,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-0103-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Linewrapped_Uncompressed_525x5994I_720_422 = {
        0x0d010301,
        0x0205,
        0x0103,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-0105-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_Uncompressed_625x50I_720_422 = {
        0x0d010301,
        0x0205,
        0x0105,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-0106-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_625x50I_720_422 = {
        0x0d010301,
        0x0205,
        0x0106,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-0107-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Linewrapped_Uncompressed_625x50I_720_422 = {
        0x0d010301,
        0x0205,
        0x0107,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-0119-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_Uncompressed_525x5994P_960_422 = {
        0x0d010301,
        0x0205,
        0x0119,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-011a-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_525x5994P_960_422 = {
        0x0d010301,
        0x0205,
        0x011a,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-011b-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Linewrapped_Uncompressed_525x5994P_960_422 = {
        0x0d010301,
        0x0205,
        0x011b,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-011d-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_Uncompressed_625x50P_960_422 = {
        0x0d010301,
        0x0205,
        0x011d,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-011e-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_Uncompressed_625x50P_960_422 = {
        0x0d010301,
        0x0205,
        0x011e,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0205-011f-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Linewrapped_Uncompressed_625x50P_960_422 = {
        0x0d010301,
        0x0205,
        0x011f,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0206-0100-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Framewrapped_Broadcast_Wave_audio_data = {
        0x0d010301,
        0x0206,
        0x0100,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0206-0200-060e-2b3404010101}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_Broadcast_Wave_audio_data = {
        0x0d010301,
        0x0206,
        0x0200,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0206-0300-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Framewrapped_AES3_audio_data = {
    0x0d010301,
    0x0206,
    0x0300,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-0206-0400-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Clipwrapped_AES3_audio_data = {
    0x0d010301,
    0x0206,
    0x0400,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0d010301-020a-0100-060e-2b3404010103}
static const aafUID_t AAFContainerDef_MXFGC_Framewrapped_Alaw_Audio = {
    0x0d010301,
    0x020a,
    0x0100,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x03}};

//{0d010301-020a-0200-060e-2b3404010103}
static const aafUID_t AAFContainerDef_MXFGC_Clipwrapped_Alaw_Audio = {
    0x0d010301,
    0x020a,
    0x0200,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x03}};

//{0d010301-020a-0300-060e-2b3404010103}
static const aafUID_t AAFContainerDef_MXFGC_Customwrapped_Alaw_Audio = {
    0x0d010301,
    0x020a,
    0x0300,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x03}};

//{0d010301-0210-6002-060e-2b340401010a}
static const aafUID_t
    AAFContainerDef_MXFGC_Clipwrapped_AVCbytestream_VideoStream0_SID = {
        0x0d010301,
        0x0210,
        0x6002,
        {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x0a}};

//{0d010301-0211-0100-060e-2b340401010a}
static const aafUID_t AAFContainerDef_MXFGC_Framewrapped_VC3 = {
    0x0d010301,
    0x0211,
    0x0100,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x0a}};

//{0d010301-0211-0200-060e-2b340401010a}
static const aafUID_t AAFContainerDef_MXFGC_Clipwrapped_VC3 = {
    0x0d010301,
    0x0211,
    0x0200,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x0a}};

//{0d010301-0212-0100-060e-2b340401010a}
static const aafUID_t AAFContainerDef_MXFGC_Framewrapped_VC1 = {
    0x0d010301,
    0x0212,
    0x0100,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x0a}};

//{0d010301-0212-0200-060e-2b340401010a}
static const aafUID_t AAFContainerDef_MXFGC_Clipwrapped_VC1 = {
    0x0d010301,
    0x0212,
    0x0200,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x0a}};

//{0d010301-027f-0100-060e-2b3404010103}
static const aafUID_t AAFContainerDef_MXFGC_Generic_Essence_Multiple_Mappings =
    {0x0d010301,
     0x027f,
     0x0100,
     {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x03}};

//{0d011301-0101-0100-060e-2b3404010106}
static const aafUID_t AAFContainerDef_RIFFWAVE = {
    0x0d011301,
    0x0101,
    0x0100,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x06}};

//{0d011301-0102-0200-060e-2b3404010107}
static const aafUID_t AAFContainerDef_JFIF = {
    0x0d011301,
    0x0102,
    0x0200,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x07}};

//{0d011301-0104-0100-060e-2b3404010106}
static const aafUID_t AAFContainerDef_AIFFAIFC = {
    0x0d011301,
    0x0104,
    0x0100,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x06}};

//{0e040301-0206-0101-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_220X_1080p = {
    0x0e040301,
    0x0206,
    0x0101,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0102-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_145_1080p = {
    0x0e040301,
    0x0206,
    0x0102,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0103-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_220_1080p = {
    0x0e040301,
    0x0206,
    0x0103,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0104-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_36_1080p = {
    0x0e040301,
    0x0206,
    0x0104,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0201-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_220X_1080i = {
    0x0e040301,
    0x0206,
    0x0201,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0202-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_145_1080i = {
    0x0e040301,
    0x0206,
    0x0202,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0203-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_220_1080i = {
    0x0e040301,
    0x0206,
    0x0203,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0204-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_145_1440_1080i = {
    0x0e040301,
    0x0206,
    0x0204,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0301-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_220X_720p = {
    0x0e040301,
    0x0206,
    0x0301,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0302-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_220_720p = {
    0x0e040301,
    0x0206,
    0x0302,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

//{0e040301-0206-0303-060e-2b3404010101}
static const aafUID_t AAFContainerDef_MXFGC_Avid_DNX_145_720p = {
    0x0e040301,
    0x0206,
    0x0303,
    {0x06, 0x0e, 0x2b, 0x34, 0x04, 0x01, 0x01, 0x01}};

// AAF ContainerDefinition legacy aliases
//

// const aafUID_t ContainerFile = kAAFContainerDef_External;
// const aafUID_t ContainerOMF = kAAFContainerDef_OMF;
// const aafUID_t ContainerAAF = kAAFContainerDef_AAF;
// const aafUID_t ContainerAAFMSS = kAAFContainerDef_AAFMSS;
// const aafUID_t ContainerAAFKLV = kAAFContainerDef_AAFKLV;
// const aafUID_t ContainerAAFXML = kAAFContainerDef_AAFXML;

#endif // ! __ContainerDefinition_h__
