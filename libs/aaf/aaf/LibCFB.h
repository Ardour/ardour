/*
 * Copyright (C) 2017-2024 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __LibCFB_h__
#define __LibCFB_h__

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "aaf/log.h"

#if defined(__linux__)
#include <limits.h>
#include <linux/limits.h>
#elif defined(__APPLE__)
#include <sys/syslimits.h>
#elif defined(_WIN32)
#include <windows.h> // MAX_PATH
#include <limits.h>
#endif

/**
 * @file LibCFB/LibCFB.h
 * @brief Compound File Binary Library
 * @author Adrien Gesta-Fline
 * @version 0.1
 * @date 04 october 2017
 *
 * @ingroup LibCFB
 * @addtogroup LibCFB
 * @{
 */

/**
 * Class Identifier structure.
 *
 * Used by cfbHeader._clsid and cfbNode._clsId.
 *
 * 16-byte long, binary compatible with GUID and AAF's AUID.
 */

struct cfbCLSID_t {
	uint32_t Data1;
	uint16_t Data2;
	uint16_t Data3;
	uint8_t  Data4[8];
}; // __attribute__((packed));

typedef struct cfbCLSID_t cfbCLSID_t;

/**
 * 64-bit value representing number of 100 nanoseconds since January 1, 1601.
 *
 * Used in cfbNode.
 */

struct cfbFiletime_t {
	uint32_t dwLowDateTime;
	uint32_t dwHighDateTime;
}; // __attribute__((packed));

typedef struct cfbFiletime_t cfbFiletime_t;

/**
 * A sector ID, that is an index into the FAT or the MiniFAT.
 */

typedef uint32_t cfbSectorID_t;

/**
 * A stream identifier, that is an index into the array of nodes (directory entries).
 */

typedef uint32_t cfbSID_t;

/**
 * This enum defines sector IDs and storage IDs (SID) with special meanings.
 *  NOTE: enum was turned to MACRO, because of -Wpedantic (and even though they are valid 32bits...)
 */

#define CFB_MAX_REG_SECT 0xfffffffa

/**
 * Denotes a DiFAT sector ID in the FAT or MiniFAT.
 */

#define CFB_DIFAT_SECT 0xfffffffc

/**
 * Denotes a FAT sector ID in the FAT or MiniFAT.
 */

#define CFB_FAT_SECT 0xfffffffd

/**
 * End of a virtual stream chain.
 */

#define CFB_END_OF_CHAIN 0xfffffffe

/**
 * Unallocated FAT or MiniFAT sector.
 */

#define CFB_FREE_SECT 0xffffffff

/**
 * Maximum directory entry ID.
 */

#define CFB_MAX_REG_SID 0xfffffffa

/**
 * Unallocated directory entry.
 */

#define CFB_NO_STREAM 0xffffffff

// typedef enum cfbSpecialSectorID_e
// {
// 	/**
// 	 * Maximum sector ID.
// 	 */
//
// 	CFB_MAX_REG_SECT = 0xfffffffa,
//
//
// 	/**
// 	 * Denotes a DiFAT sector ID in the FAT or MiniFAT.
// 	 */
//
// 	CFB_DIFAT_SECT   = 0xfffffffc,
//
//
// 	/**
// 	 * Denotes a FAT sector ID in the FAT or MiniFAT.
// 	 */
//
// 	CFB_FAT_SECT     = 0xfffffffd,
//
//
// 	/**
// 	 * End of a virtual stream chain.
// 	 */
//
// 	CFB_END_OF_CHAIN = 0xfffffffe,
//
//
// 	/**
// 	 * Unallocated FAT or MiniFAT sector.
// 	 */
//
// 	CFB_FREE_SECT    = 0xffffffff,
//
//
// 	/**
// 	 * Maximum directory entry ID.
// 	 */
//
// 	CFB_MAX_REG_SID  = 0xfffffffa,
//
//
// 	/**
// 	 * Unallocated directory entry.
// 	 */
//
// 	CFB_NO_STREAM    = 0xffffffff
//
// } cfbSpecialSectorID_e;

/**
 * Storage Type. These are the values used by cfbNode._mse
 * to specify the type of the node.
 *
 *  NOTE: microsoft already define enum tagSTGTY, but it lacks of STGTY_INVALID and STGTY_ROOT.
 */

typedef enum customTagSTGTY {
	/**
	 * Unknown storage type.
	 */

	STGTY_INVALID = 0,

#ifndef _WIN32
	/**
	 * The node is a storage object, that is a "directory" node.
	 *
	 * For AAF, this node represents an aafObject.
	 */

	STGTY_STORAGE = 1,

	/**
	 * The node is a stream object, that is a "file" node.
	 *
	 * For AAF, this node can be a "Properties" node, a StrongRefSet,
	 * a StrongRefVector or a data stream containing some essence.
	 */

	STGTY_STREAM = 2,

	/**
	 * The node is an ILockBytes object.
	 *
	 * TODO What is an ILockBytes object ?
	 */

	STGTY_LOCKBYTES = 3,

	/**
	 * The node is an IPropertyStorage object.
	 *
	 * TODO What is an IPropertyStorage object ?
	 */

	STGTY_PROPERTY = 4,

#endif
	/**
	 * The node is the Root node (SID 0).
	 */

	STGTY_ROOT = 5

} cfbStorageType_e;

/**
 * This enum defines the colors for the red/black Tree.
 *
 * Used by cfbNode._bflags.
 */

typedef enum tagDECOLOR {
	CFB_RED   = 0,
	CFB_BLACK = 1
} cfbColor_e;

/**
 * This enum defines the values for cfbHeader._uByteOrder.
 */

typedef enum cfbByteOrder_e {
	CFB_BYTE_ORDER_LE = 0xfffe,
	CFB_BYTE_ORDER_BE = 0xfeff
} cfbByteOrder_e;

/**
 * The length of the cfbNode._ab uint16_t array holding the node's UTF-16 name, including
 * the NULL terminating Unicode.
 */

#define CFB_NODE_NAME_SZ 32

/**
 * This is an arbitrary length chosen for defining a char array to store a path in the
 * directory Tree.
 */

#define CFB_PATH_NAME_SZ CFB_NODE_NAME_SZ * 64

/**
 * This is the header of the Compound File. It corresponds to the first 512 bytes of the
 * file starting at offset zero. If the sector size is greater than 512 bytes, then the
 * header is padded to the sector size with zeroes.
 */

typedef struct StructuredStorageHeader {
	/**
	 * File Signature. Shall be {0xd0, 0xcf, 0x11, 0xe0, 0xa1, 0xb1, 0x1a, 0xe1}.
	 *
	 * The file signature should be `uint8_t _abSig[8]`, but we define
	 * it as uint64_t for quick comparison.
	 */

	uint64_t _abSig;

	/**
	 * CFB spec says this should be set to zero, however AAF gets the file kind from
	 * this CLSID. Thus, this CLSID should match one of the AAFFileKind_* from
	 * AAFDefs/AAFFileKinds.h
	 */

	cfbCLSID_t _clsid;

	/**
	 * Minor version of the format. 33 is written by reference implementation.
	 *
	 * This field is unused by this library.
	 */

	uint16_t _uMinorVersion;

	/**
	 * Major version of the dll/format. 3 for 512-byte sectors, 4 for 4kB sectors.
	 *
	 * This field is unused by this library.
	 */

	uint16_t _uDllVersion;

	/**
	 * File's byte ordering. Should be either CFB_BYTE_ORDER_LE (0xfffe) or
	 * CFB_BYTE_ORDER_BE (0xfeff).
	 *
	 * The spec says this field should always be little-endian byte ordering for
	 * maximum file portability. This implementation does not support big-endian
	 * byte ordering.
	 */

	uint16_t _uByteOrder;

	/**
	 * Size of sectors in power-of-two. Typical values are 9 (512-byte sectors)
	 * and 12 (4096-byte sectors).
	 */

	uint16_t _uSectorShift;

	/**
	 * Size of mini-sectors in power-of-two. Typical value is 6 (64-byte mini-sectors)
	 */

	uint16_t _uMiniSectorShift;

	/**
	 * Reserved, must be zero.
	 */

	uint16_t _usReserved;

	/**
	 * Reserved, must be zero.
	 */

	uint32_t _ulReserved1;

	/**
	 * Number of sector IDs in directory chain for 4 KB sectors. Must be zero for
	 * 512-byte sectors.
	 */

	cfbSectorID_t _csectDir;

	/**
	 * Number of sector IDs in the FAT chain.
	 */

	cfbSectorID_t _csectFat;

	/**
	 * First sector ID in the directory chain.
	 */

	cfbSectorID_t _sectDirStart;

	/**
	 * Signature used for transactions, must be zero. The reference implementation
	 * does not support transactions.
	 *
	 * This field is unused by this library.
	 *
	 * TODO What is a transaction ???
	 */

	uint32_t _signature;

	/**
	 * Maximum size for a mini-stream, typically 4096 bytes. If a streamNode
	 * size is below this treshold, then it is stored in the mini-stream and
	 * will be retrieved from the MiniFAT. Otherwise the stream is a regular
	 * stream, that will be retrieved from the FAT.
	 */

	uint32_t _ulMiniSectorCutoff;

	/**
	 * First sector ID in the MiniFAT chain.
	 */

	cfbSectorID_t _sectMiniFatStart;

	/**
	 * Number of sector IDs in the MiniFAT chain.
	 */

	cfbSectorID_t _csectMiniFat;

	/**
	 * First sector ID in the DiFAT chain.
	 */

	cfbSectorID_t _sectDifStart;

	/**
	 * Number of sector IDs in the DiFAT chain.
	 */

	cfbSectorID_t _csectDif;

	/**
	 * Array of the first 109 FAT sector IDs.
	 *
	 * These are the first entries in the DiFAT. If the _csectDif is zero, then this
	 * is the entire DiFAT.
	 */

	cfbSectorID_t _sectFat[109];

} cfbHeader; // __attribute__((packed)) cfbHeader;

/**
 * This structure represents a Node in the Directory stream and thus a node in the
 * Directory Tree. The Compound File Directory spec calls a "Node" a "Directory".
 *
 * The Directory stream starts at sector ID cfbHeader._sectDirStart in the FAT and
 * ends at sector ID CFB_NO_STREAM. Each directory sector is an array of directory
 * entries (cfbNode), so the entire directory stream forms the array of nodes.
 * An index into that array is called a stream identifier (SID).
 *
 * A cfbNode structure always being 128-byte long, there are `sectorSize / 128`
 * cfbNode per directory sector, that is 4 cfbNode per 512-byte sector, and 32
 * cfbNode per 4-kB sector.
 *
 * The Directory Tree forms a red/black Tree composed of each cfbNode, in which the
 * root node is the first cfbNode entry (SID 0).
 */

/* An index used in that sector chain is called a stream identifier (SID). */

typedef struct StructuredStorageDirectoryEntry {
	/**
	 * The node's name, as a Unicode string.
	 *
	 * A 64-byte array, for a maximum of 32 Unicode characters including a terminating
	 * Unicode NULL character. The string shall be padded with zeros to fill the array.
	 */

	uint16_t _ab[CFB_NODE_NAME_SZ];

	/**
	 * Length of the node's name in bytes, including the Unicode NULL terminating byte.
	 */

	uint16_t _cb;

	/**
	 * Type of the node. TODO
	 *
	 * Value taken from the cfbStorageType_e enumeration.
	 */

	uint8_t _mse;

	/**
	 * "Color" of the node. Shall be either CFB_RED or CFB_BLACK.
	 */

	uint8_t _bflags;

	/**
	 * SID of the left-sibling of this node in the directory tree.
	 */

	cfbSID_t _sidLeftSib;

	/**
	 * SID of the right-sibling of this node in the directory tree.
	 */

	cfbSID_t _sidRightSib;

	/**
	 * SID of the child acting as the root node of all the children of this node.
	 *
	 * Only if _mse is STGTY_STORAGE or STGTY_ROOT.
	 */

	cfbSID_t _sidChild;

	/**
	 * CLSID of this node.
	 *
	 * Only if _mse is STGTY_STORAGE or STGTY_ROOT.
	 */

	cfbCLSID_t _clsId;

	/**
	 * User flags of this node.
	 *
	 * Don't know what that is, maybe some custom user flags. Looks like it
	 * is left unused by the AAF anyway.
	 *
	 * Only if _mse is STGTY_STORAGE or STGTY_ROOT.\n
	 * This field is unused by this library.
	 */

	uint32_t _dwUserFlags;

	/**
	 * Array of two cfbFiletime_t struct. The first one holds the creation date/time,
	 * the second the modification date/time.
	 *
	 * Only if _mse is STGTY_STORAGE.
	 */

	cfbFiletime_t _time[2];

	/**
	 * First sector ID of the stream.
	 *
	 * Only if _mse is STGTY_STREAM.
	 */

	cfbSectorID_t _sectStart;

	/**
	 * Low part of the 64-bit stream size in bytes.
	 *
	 * Only if _mse is STGTY_STREAM.
	 */

	uint32_t _ulSizeLow;

	/**
	 * High part of 64-bit stream size.
	 *
	 * Only if _mse is STGTY_STREAM and when sector size is 4kB. Shall be zero for
	 * 512-byte sectors.
	 */

	uint32_t _ulSizeHigh;

} cfbNode; // __attribute__((packed)) cfbNode;

/* Node size matching CFB file */
#define CFB_NODE_SIZE 128

/**
 * This structure is the main structure when using LibCFB.
 */

typedef struct CFB_Data {
	/**
	 * CFB file path.
	 */

	char* file;

	/**
	 * CFB file size.
	 */

	size_t file_sz;

	/**
	 * CFB file pointer.
	 */

	FILE* fp;

	/**
	 * Pointer to the cfbHeader structure.
	 */

	cfbHeader* hdr;

	/**
	 * Number of (FAT) sector entries in the DiFAT.
	 */

	uint32_t DiFAT_sz;

	/**
	 * Array of FAT sector IDs, that is sectors holding the FAT.
	 */

	cfbSectorID_t* DiFAT;

	/**
	 * Number of sector entries in the FAT.
	 */

	uint32_t fat_sz;

	/**
	 * Array of sector IDs.
	 */

	cfbSectorID_t* fat;

	/**
	 * Number of mini-sector entries in the MiniFAT.
	 */

	uint32_t miniFat_sz;

	/**
	 * Array of mini-sector IDs.
	 */

	cfbSectorID_t* miniFat;

	/**
	 * Number of cfbNode pointers in the CFB_Data.nodes array.
	 */

	uint32_t nodes_cnt;

	/**
	 * Array of pointers to cfbNodes.
	 */

	cfbNode* nodes;

	struct aafLog* log;

} CFB_Data;

/**
 * @name Function macros
 * @{
 */

/**
 * Loops through each sector from a given Chain in the FAT,
 * starting at sector index id.
 *
 * @param cfbd Pointer to the CFB_Data structure.
 * @param buf  Pointer to the buffer that will hold each sector data bytes.
 * @param id   Index of the first sector in the Chain.
 */

#define CFB_foreachSectorInChain(cfbd, buf, id) \
	for (buf = cfb_getSector (cfbd, id);    \
	     id < CFB_MAX_REG_SECT &&           \
	     buf != NULL;                       \
	     id = cfbd->fat[id],                \
	    buf = cfb_getSector (cfbd, id))

/**
 * Loops through each sector from a given Chain in the MiniFAT,
 * starting at mini-sector index id.
 *
 * @param cfbd Pointer to the CFB_Data structure.
 * @param buf  Pointer to the buffer that will hold each mini-sector data bytes.
 * @param id   Index of the first mini-sector in the Chain.
 */

#define CFB_foreachMiniSectorInChain(cfbd, buf, id) \
	for (buf = cfb_getMiniSector (cfbd, id);    \
	     id < CFB_MAX_REG_SECT;                 \
	     id = cfbd->miniFat[id],                \
	    buf = cfb_getMiniSector (cfbd, id))

/**
 * Loops through each DiFAT sector.
 *
 * @param cfbd Pointer to the CFB_Data structure.
 * @param buf  Pointer to the buffer that will hold each DiFAT sector data bytes.
 * @param id   Index of each DiFAT sector. The first index is retrieved from the Compound
 *             File Binary Header, so user can pass any value (usualy 0). Then, the ID of
 *             the next DiFAT sector is retrieved from the last 4 bytes of each DiFAT
 *             sector data.
 */

#define CFB_foreachSectorInDiFATChain(cfbd, buf, id)                                       \
	for (id = cfbd->hdr->_sectDifStart,                                                \
	    buf = cfb_getSector (cfbd, id);                                                \
	     id < CFB_MAX_REG_SECT;                                                        \
	     memcpy (&id, (buf + (1 << cfbd->hdr->_uSectorShift) - 4), sizeof (uint32_t)), \
	    free (buf),                                                                    \
	    buf = cfb_getSector (cfbd, id))

/**
 * Loops through each FAT sector ID in the DiFAT.
 *
 * @param cfbd Pointer to the CFB_Data structure.
 * @param id   Index of each FAT sector.
 */

#define CFB_foreachFATSectorIDInDiFAT(cfbd, id) \
	for (id = 0;                            \
	     id < cfbd->DiFAT_sz &&             \
	     id < cfbd->hdr->_csectFat;         \
	     id++)

/**
 * Retrieves the full stream length of a streamNode.
 * When 512 bytes sectors we don't care about _ulSizeHigh.
 */

#define CFB_getNodeStreamLen(cfbd, node) \
	((cfbd->hdr->_uSectorShift > 9) ? (uint64_t) (((uint64_t) (node->_ulSizeHigh) << 32) | (node->_ulSizeLow)) : node->_ulSizeLow)

#define CFB_getStreamSectorShift(cfbd, node) \
	((CFB_getNodeStreamLen (cfbd, node) < cfbd->hdr->_ulMiniSectorCutoff) ? cfbd->hdr->_uMiniSectorShift : cfbd->hdr->_uSectorShift)

/*
 * @}
 */

const char*
cfb_CLSIDToText (const cfbCLSID_t* clsid);

char*
cfb_w16toUTF8 (const uint16_t* w16buf, size_t w16blen);

/**
 * @name Constructor function
 * The first function to be called when using LibCFB.
 * @{
 */

CFB_Data*
cfb_alloc (struct aafLog* log);

/**
 * @}
 *
 * @name Destructor function
 * The last function to be called when using LibCFB.
 * @{
 */

void
cfb_release (CFB_Data** cfbd);

/**
 * @}
 *
 * @name Initialization function
 * One of these function shall be called after cfb_alloc().
 * @{
 */

int
cfb_load_file (CFB_Data** cfbd, const char* file);

int
cfb_new_file (CFB_Data* cfbd, const char* file, int sectSize);

/**
 * @name File parsing functions
 * @{
 */

unsigned char*
cfb_getSector (CFB_Data* cfbd, cfbSectorID_t id);

unsigned char*
cfb_getMiniSector (CFB_Data* cfbd, cfbSectorID_t id);

uint64_t
cfb_getStream (CFB_Data* cfbd, cfbNode* node, unsigned char** stream, uint64_t* stream_sz);

int
cfb__foreachSectorInStream (CFB_Data* cfbd, cfbNode* node, unsigned char** buf, size_t* bytesRead, cfbSectorID_t* sectID);

#define CFB_foreachSectorInStream(cfbd, node, buf, bytesRead, sectID) \
	while (cfb__foreachSectorInStream (cfbd, node, buf, bytesRead, sectID))

/**
 * @}
 *
 * @name Nodes access functions
 * @{
 */

cfbNode*
cfb_getNodeByPath (CFB_Data* cfbd, const char* path, cfbSID_t id);

cfbNode*
cfb_getChildNode (CFB_Data* cfbd, const char* name, cfbNode* startNode);

/**
 * @}
 *
 * @name Misc functions
 * @{
 */

/**
 * @}
 */

/**
 * @}
 */

#endif // ! __LibCFB_h__
