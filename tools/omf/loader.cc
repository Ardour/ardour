/*  Rewritten for Ardour by Paul Davis <paul@linuxaudiosystems.com>, Feb 2010
    but based on ...
 */

/*  REAPER OMF plug-in
    Copyright (C) 2009 Hannes Breul

    Provides OMF import.

    Based on the m3u example included in the Reaper SDK,
    Copyright (C) 2005-2008 Cockos Incorporated

    Original source available at:
    http://www.reaper.fm/sdk/plugin/plugin.php#ext_dl

    This software is provided 'as-is', without any express or implied
    warranty.  In no event will the authors be held liable for any damages
    arising from the use of this software.

    Permission is granted to anyone to use this software for any purpose,
    including commercial applications, and to alter it and redistribute it
    freely, subject to the following restrictions:

    1. The origin of this software must not be misrepresented; you must not
    claim that you wrote the original software. If you use this software
    in a product, an acknowledgment in the product documentation would be
    appreciated but is not required.
    2. Altered source versions must be plainly marked as such, and must not be
    misrepresented as being the original software.
    3. This notice may not be removed or altered from any source distribution.
*/

#ifndef __STDC_FORMAT_MACROS
#define __STDC_FORMAT_MACROS /* PRI<foo>; C++ requires explicit requesting of these */
#endif

#include <iostream>

#include <getopt.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <inttypes.h>
#include <sys/errno.h>
#include <sndfile.h>
#include <glibmm.h>

#include "pbd/xml++.h"
#include "pbd/basename.h"
#include "omftool.h"

using namespace std;
using namespace PBD;

//#define DEBUG(fmt,...) fprintf (stderr, fmt, ## __VA_ARGS__)
#define DEBUG(fmt,...)
#define INFO(fmt,...) fprintf (stdout, fmt, ## __VA_ARGS__)

#define MB_OK 0
void
MessageBox (FILE* /*ignored*/, const char* msg, const char* title, int status)
{
	fprintf (stderr, msg);
}

void
OMF::name_types ()
{
	/* Add built-in types */
	sqlite3_exec(db, "INSERT INTO lookup VALUES (1, 'TOC property 1')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (2, 'TOC property 2')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (3, 'TOC property 3')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (4, 'TOC property 4')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (5, 'TOC property 5')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (6, 'TOC property 6')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (7, '(Type 7)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (8, '(Type 8)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (9, '(Type 9)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (10, '(Type 10)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (11, '(Type 11)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (12, '(Type 12)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (13, '(Type 13)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (14, '(Type 14)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (15, '(Type 15)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (16, '(Type 16)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (17, '(Type 17)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (18, '(Type 18)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (19, 'TOC Value')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (20, '(Type 20)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (21, 'String')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (22, '(Type 22)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (23, 'Type Name')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (24, 'Property Name')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (25, '(Type 25)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (26, '(Type 26)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (27, '(Type 27)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (28, '(Type 28)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (29, '(Type 29)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (30, '(Type 30)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (31, 'Referenced Object')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (32, 'Object')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (33, '(Type 33)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (34, '(Type 34)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (35, '(Type 35)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (36, '(Type 36)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (37, '(Type 37)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (38, '(Type 38)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (39, '(Type 39)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (40, '(Type 40)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (41, '(Type 41)')", 0, 0, 0);
	sqlite3_exec(db, "INSERT INTO lookup VALUES (42, '(Type 42)')", 0, 0, 0);

	/* Assign type and property values to names */
	sqlite3_exec(db, "UPDATE data SET property = (SELECT name FROM lookup WHERE property = key), type = (SELECT name FROM lookup WHERE type = key)", 0, 0, 0);
	sqlite3_exec(db, "DROP TABLE lookup", 0, 0, 0);
}

int
OMF::load (const string& path)
{
	if ((file = fopen(path.c_str(), "rb")) == 0) {
		MessageBox(NULL, "Cannot open file","OMF Error", MB_OK);
		return -1;
	}

	/* --------------- */
	char *fname = (char*) malloc (path.size()+5);

	strcpy(fname, path.c_str());
	strcat(fname, ".db3");
	//remove(fname);
	if(sqlite3_open(":memory:", &db)) {
		char error[512];
		sprintf(error, "Can't open database: %s", sqlite3_errmsg(db));
		MessageBox(NULL, error,"OMF Error", MB_OK);
		sqlite3_close(db);
		return -3;
	}
	sqlite3_exec(db, "BEGIN", 0, 0, 0);
	sqlite3_exec(db, "CREATE TABLE data (object, property, type, value, offset, length)", 0, 0, 0);
	sqlite3_exec(db, "CREATE TABLE lookup (key, name)", 0, 0, 0);

	uint8_t magic[8];
	fseek(file, -24, SEEK_END);
	fread(magic, 8, 1, file);
	if ((magic[0] != 0xa4) | (magic[1] != 0x43) | (magic[2] != 0x4d) | (magic[3] != 0xa5) | (magic[4] != 0x48) | (magic[5] != 0x64) | (magic[6] != 0x72) | (magic[7] != 0xd7)) {
		MessageBox(NULL, "No valid OMF file.","OMF Error", MB_OK);
		return -4;
	}

	uint16_t bSize, version;
	fseek(file, -12, SEEK_END);
	fread(&version, 2, 1, file);
	bigEndian = false;
	if ((version == 1) | (version == 256)) {
		MessageBox(NULL, "You tried to open an OMF1 file.\nOMF1 is not supported.","OMF Error", MB_OK);
		return -2;
	} else if (version == 512) {
		bigEndian = true;
	} else if (version != 2) {
		MessageBox(NULL, "You tried to open a corrupted file.","OMF Error", MB_OK);
		return -2;
	}

	uint32_t tocStart, tocSize;
	fseek(file, -14, SEEK_END);
	fread(&bSize, 2, 1, file);
	bSize = e16(bSize);

	fseek(file, -8, SEEK_END);
	fread(&tocStart, 4, 1, file);
	tocStart = e32(tocStart);

	fseek(file, -4, SEEK_END);
	fread(&tocSize, 4, 1, file);
	tocSize = e32(tocSize);
	DEBUG ("block size: %d\n toc start: %d\n  toc size: %d\n", bSize, tocStart, tocSize);


	/* Calculate number of TOC blocks */
	uint32_t tocBlocks = tocSize / (bSize * 1024) + 1;
	DEBUG ("toc blocks: %d\n", tocBlocks);
	/* ------------------------------ */

	time_t globalstart, starttime, endtime;
	time(&globalstart);
	starttime = globalstart;
	INFO ("Parsing TOC... ");

	/* Go through TOC blocks */
	uint32_t j;
	uint32_t currentObj = 0;
	uint32_t currentProp = 0;
	uint32_t currentType = 0;
	char skip = 0;
	//uint64_t len = 0;
	for (j = 0; j < tocBlocks; j++) {
		uint32_t currentBlock = tocStart + j * 1024 * bSize; // Start at beginning of current block
		uint32_t currentPos;
		for (currentPos = currentBlock; currentPos < currentBlock + 1024 * bSize; currentPos++) {
			if (currentPos > tocStart + tocSize) break; // Exit at end of TOC
			char cByte; // TOC control byte
			fseek(file, currentPos, SEEK_SET);
			fread(&cByte, 1, 1, file);

			/* New object */
			if (cByte == 1) {
				fseek(file, currentPos + 1, SEEK_SET);
				fread(&currentObj, 4, 1, file);
				currentObj = e32(currentObj);
				fseek(file, currentPos + 5, SEEK_SET);
				fread(&currentProp, 4, 1, file);
				currentProp = e32(currentProp);
				fseek(file, currentPos + 9, SEEK_SET);
				fread(&currentType, 4, 1, file);
				currentType = e32(currentType);
				DEBUG("---------------------\n");
				DEBUG("   object: 0x%x\n", currentObj);
				DEBUG(" property: 0x%x\n", currentProp);
				DEBUG("     type: 0x%x\n", currentType);
				skip = 0;
				currentPos += 12; // Skip the bytes that were just read
			}
			/* ---------- */


			/* New property */
			else if (cByte == 2) {
				fseek(file, currentPos + 1, SEEK_SET);
				fread(&currentProp, 4, 1, file);
				currentProp = e32(currentProp);
				fseek(file, currentPos + 5, SEEK_SET);
				fread(&currentType, 4, 1, file);
				currentType = e32(currentType);
				DEBUG(" property: 0x%x\n", currentProp);
				DEBUG("     type: 0x%x\n", currentType);
				skip = 0;
				currentPos += 8;
			}
			/* ------------ */


			/* New type */
			else if (cByte == 3) {
				fseek(file, currentPos + 1, SEEK_SET);
				fread(&currentType, 4, 1, file);
				currentType = e32(currentType);
				DEBUG("     type: 0x%x\n", currentType);
				skip = 0;
				currentPos += 4;
			}
			/* -------- */


			/* (unused) */
			else if (cByte == 4) {
				currentPos += 4;
			}
			/* -------- */


			/* Reference to a value - 4/8 byte offset, 4/8 byte size */
			else if ((cByte == 5) | (cByte == 6) | (cByte == 7) | (cByte == 8)) {
				if (!skip) {
					uint32_t offset32 = 0;
					uint32_t length32 = 0;
					uint64_t dataOffset = 0;
					uint64_t dataLength = 0;
					if ((cByte == 5) | (cByte == 6)) {
						fseek(file, currentPos + 1, SEEK_SET);
						fread(&offset32, 4, 1, file);
						fseek(file, currentPos + 5, SEEK_SET);
						fread(&length32, 4, 1, file);
						dataOffset = e32(offset32);
						dataLength = e32(length32);
					} else {
						fseek(file, currentPos + 1, SEEK_SET);
						fread(&dataOffset, 8, 1, file);
						dataOffset = e64(dataOffset);
						fseek(file, currentPos + 9, SEEK_SET);
						fread(&dataLength, 8, 1, file);
						dataLength = e64(dataLength);
					}
					DEBUG("   offset: %d\n", dataOffset);
					DEBUG("   length: %d\n", dataLength);

					if (currentType == 21) {
						char* string = (char*) malloc((uint32_t) dataLength);
						fseek(file, dataOffset, SEEK_SET);
						fread(string, dataLength, 1, file);
						char* query = sqlite3_mprintf("INSERT INTO lookup VALUES(%d, '%s')",currentObj, string);
						sqlite3_exec(db, query, 0, 0, 0);
						sqlite3_free(query);
					} else if (currentType == 32){
						uint32_t object = 0;
						fseek(file, dataOffset, SEEK_SET);
						fread(&object, 4, 1, file);
						object = e32(object);
						char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, %d, -1, -1)",currentObj, currentProp, currentType, object);
						sqlite3_exec(db, query, 0, 0, 0);
						sqlite3_free(query);
						if (dataLength == 16) {
							DEBUG("   offset: %lld\n", dataOffset + 8);
							DEBUG("   length: %lld\n", dataLength);
							fseek(file, dataOffset + 8, SEEK_SET);
							fread(&object, 4, 1, file);
							object = e32(object);
							char* query2 = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, %d, -1, -1)",currentObj, currentProp, currentType, object);
							sqlite3_exec(db, query2, 0, 0, 0);
							sqlite3_free(query2);
						}
					} else {
						char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, '', %lld, %lld)",currentObj, currentProp, currentType, dataOffset, dataLength);
						sqlite3_exec(db, query, 0, 0, 0);
						sqlite3_free(query);
					}
				}
				if ((cByte == 5) | (cByte == 6)) {
					currentPos += 8;
				} else {
					currentPos += 16;
				}
			}
			/* ----------------------------------------------------- */


			/* Zero byte value */
			else if (cByte == 9) {
				if (!skip) {
					char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, 'true', -1, -1)",currentObj, currentProp, currentType);
					sqlite3_exec(db, query, 0, 0, 0);
					sqlite3_free(query);
					DEBUG("    value: true\n");
				}
			}
			/* --------------- */


			/* Immediate value */
			else if ((cByte == 10) | (cByte == 11) | (cByte == 12) | (cByte == 13) | (cByte == 14)) {
				if (!skip) {
					uint32_t data = 0;
					fseek(file, currentPos + 1, SEEK_SET);
					fread(&data, 4, 1, file);
					data = e32(data);
					char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, %d, -1, -1)",currentObj, currentProp, currentType, data);
					sqlite3_exec(db, query, 0, 0, 0);
					sqlite3_free(query);
					DEBUG("    value: %d\n", data);

				}
				currentPos += 4;
			}
			/* --------------- */


			/* Reference list */
			else if (cByte == 15) {
				uint32_t data = 0;
				fseek(file, currentPos + 1, SEEK_SET);
				fread(&data, 4, 1, file);
				data = e32(data);
				char* query = sqlite3_mprintf("INSERT INTO data VALUES(%d, %d, %d, %d, -1, -1)",currentObj, currentProp, currentType, data);
				sqlite3_exec(db, query, 0, 0, 0);
				sqlite3_free(query);
				DEBUG("reference: 0x%x\n", data);
				skip = 1;
				currentPos += 4;
			}
			/* -------------- */
			else {
				break;
			}

		}
	}
	/* --------------------- */
	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	INFO("Assigning type and property names... ");
	name_types ();
	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	bool isAvid = false;

	/* resolve ObjRefArrays */
	char **arrays;
	int arrayCount;
	int l;
	INFO("Resolving ObjRefArrays ");
	sqlite3_get_table(db, "SELECT * FROM data WHERE type LIKE 'omfi:ObjRefArray' AND value = ''", &arrays, &arrayCount, 0, 0);
	INFO("(%d to be processed)... ", arrayCount);
	sqlite3_exec(db,"DELETE FROM data WHERE type LIKE 'omfi:ObjRefArray' AND value = ''",0,0,0);
	for (l = 6; l <= arrayCount * 6; l+=6) {
		uint16_t counter;
		uint32_t arrOffs = atoi(arrays[l+4]);
		uint32_t arrLen = atoi(arrays[l+5]);
		fseek(file, arrOffs, SEEK_SET);
		fread(&counter, 2, 1, file);
		counter = e16(counter);
		if (arrLen = 4 * counter + 2) {
			isAvid = true;
			currentObj++;
			DEBUG("currentObj: %d - references:", currentObj);
			for (counter = 2; counter < arrLen; counter += 4) {
				uint32_t temp;
				fseek(file, arrOffs + counter, SEEK_SET);
				fread(&temp, 4, 1, file);
				temp = e32(temp);
				DEBUG(" %d", temp);
				sqlite3_exec(db, sqlite3_mprintf("INSERT INTO data VALUES (%d, 'Referenced Object', 'Object', %d, -1, -1)", currentObj, temp), 0, 0, 0);
			}
			DEBUG("\nData: %s | %s | %s | %d | -1 | -1\n", arrays[l], arrays[l+1], arrays[l+2], currentObj);
			sqlite3_exec(db, sqlite3_mprintf("INSERT INTO data VALUES (%s, '%s', '%s', %d, -1, -1)", arrays[l], arrays[l+1], arrays[l+2], currentObj), 0, 0, 0);
		}
	}
	sqlite3_free_table(arrays);
	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;
	/* -------------------- */


	//return -1;
	/*char **refs;
	  int refCount;
	  int currentRef;
	  printf("Resolving ObjRefs...\n");
	  sqlite3_get_table(db,"SELECT object, property, value FROM data WHERE type = 'omfi:ObjRef'", &refs, &refCount, 0, 0);
	  printf("temporary table created\n");
	  for (currentRef = 3; currentRef <= refCount * 3; currentRef += 3) {
	  DEBUG("%d / %d\n", currentRef/3, refCount);
	  char **target;
	  int targetCount;
	  sqlite3_get_table(db, sqlite3_mprintf("SELECT value FROM data WHERE object = %s AND type = 'Object' LIMIT 1", refs[currentRef+2]), &target, &targetCount, 0, 0);
	  DEBUG("temporary table filled\n");
	  if (targetCount > 0) {
	  //sqlite3_exec(db,sqlite3_mprintf("DELETE FROM data WHERE object = %s", refs[currentRef+2]),0,0,0);
	  DEBUG("unused reference deleted\n");
	  sqlite3_exec(db,sqlite3_mprintf("UPDATE data SET value = %s WHERE object LIKE '%s' AND property LIKE '%s' LIMIT 1", target[1], refs[currentRef], refs[currentRef+1]),0,0,0);
	  printf("temporary data inserted\n");
	  }
	  sqlite3_free_table(target);
	  }
	  sqlite3_free_table(refs);
	  printf("temporary table deleted\n"); */

	if (!isAvid) {
		INFO("Resolving ObjRefs ");
		sqlite3_exec(db,"CREATE TABLE reference (object1, property1, value1)",0,0,0);
		sqlite3_exec(db,"INSERT INTO reference SELECT object, property, value FROM data WHERE type LIKE 'omfi:ObjRef'",0,0,0);
		sqlite3_exec(db,"CREATE TABLE objects (object2, value2)",0,0,0);
		sqlite3_exec(db,"INSERT INTO objects SELECT object, value FROM data WHERE type LIKE 'Object'",0,0,0);
		sqlite3_exec(db,"UPDATE reference SET value1 = (SELECT value2 FROM objects WHERE object2 = value1)",0,0,0);
		//sqlite3_exec(db,"UPDATE data SET value = (SELECT value1 FROM references WHERE object1 = object) WHERE ",0,0,0);
		char **refs;
		int refCount;
		int currentRef;

		sqlite3_get_table(db,"SELECT * FROM reference", &refs, &refCount, 0, 0);
		INFO ("(%d to be processed)... ", refCount);
		for (currentRef = 3; currentRef <= refCount * 3; currentRef += 3) {
			DEBUG("%d / %d: object %s, property %s, value %s\n", currentRef/3, refCount, refs[currentRef], refs[currentRef+1], refs[currentRef+2]);
			sqlite3_exec(db,sqlite3_mprintf("DELETE FROM data WHERE object = %s AND property = '%s'", refs[currentRef], refs[currentRef+1]),0,0,0);
			sqlite3_exec(db,sqlite3_mprintf("INSERT INTO data VALUES (%s, '%s', 'omfi:ObjRef', %s, -1, -1)", refs[currentRef], refs[currentRef+1], refs[currentRef+2]),0,0,0);
		}
		sqlite3_free_table(refs);
	}
	DEBUG("temporary table deleted\n");

	/*sqlite3_get_table(db,"SELECT object, property, value FROM data WHERE type LIKE 'omfi:ObjRef'", &refs, &refCount, 0, 0);
	  printf("%d\n", refCount);
	  for (currentRef = 3; currentRef <= refCount * 3; currentRef += 3) {
	  printf("%d / %d: object %s, property %s, value %s\n", currentRef/3, refCount, refs[currentRef], refs[currentRef+1], refs[currentRef+2]);
	  }
	  sqlite3_free_table(refs);
	  }*/


	/* resolve ObjRefs *
	   printf("Resolving ObjRefs...\n");
	   sqlite3_exec(db,"CREATE TABLE temp (object, property, type, value, offset, length)",0,0,0);
	   printf("temporary table created\n");
	   sqlite3_exec(db,"INSERT INTO temp SELECT d1.object, d1.property, d1.type, d2.value, d1.offset, d1.length FROM data d1, data d2 WHERE d1.type = 'omfi:ObjRef' AND d1.value = d2.object AND d2.type = 'Object'",0,0,0);
	   printf("temporary table filled\n");
	   //sqlite3_exec(db,"DELETE FROM data WHERE object IN (SELECT value FROM data WHERE type LIKE 'omfi:ObjRef')",0,0,0);
	   sqlite3_exec(db,"DELETE FROM data WHERE object IN (SELECT object FROM temp) AND type = 'omfi:ObjRef'",0,0,0);
	   printf("unused referenced deleted\n");
	   sqlite3_exec(db,"INSERT INTO data SELECT * FROM temp",0,0,0);
	   printf("temporary data inserted\n");
	   sqlite3_exec(db,"DROP TABLE temp",0,0,0);
	   printf("temporary table deleted\n");
	   * --------------- */

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	//return -1;
	/* resolve UIDs */
	INFO("Resolving UIDs... ");
	char **mobID;
	int mobIDCount;
	int currentID;
	sqlite3_get_table(db,"SELECT object, property, offset FROM data WHERE type LIKE 'omfi:UID'", &mobID, &mobIDCount, 0, 0);
	sqlite3_exec(db,"DELETE FROM data WHERE type LIKE 'omfi:UID'",0,0,0);
	for (currentID = 3; currentID <= mobIDCount * 3; currentID += 3) {
		uint32_t mobIDoffs = atoi(mobID[currentID+2]);
		//sscanf(mobID[currentID+2], "%d", &mobIDoffs);
		int mobBuffer[3];
		fseek(file, mobIDoffs, SEEK_SET);
		fread(mobBuffer, 12, 1, file);
		sqlite3_exec(db,sqlite3_mprintf("INSERT INTO data VALUES (%s, '%s', 'omfi:UID', '%d %d %d', -1, -1)", mobID[currentID], mobID[currentID + 1], mobBuffer[0], mobBuffer[1], mobBuffer[2]),0,0,0);
	}
	sqlite3_free_table(mobID);
	/* ------------ */

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	//return -1;

	/* extract media data */
	printf("Extracting media data...\n");
	char **objects;
	int objectsCount, k;
	//sqlite3_exec(db,"CREATE TABLE names (UID, value)",0,0,0);
	sqlite3_get_table(db, "SELECT object, offset, length FROM data WHERE object IN (SELECT value FROM data WHERE object IN (SELECT value FROM data WHERE property = 'OMFI:HEAD:MediaData' LIMIT 1)) AND type = 'omfi:DataValue'", &objects, &objectsCount, 0, 0);
	for (k = 3; k <= objectsCount * 3; k += 3) {
		char **fileName;
		int fileNameCount;
		FILE *fd;
		std::string full_path;
		sqlite3_get_table(db, sqlite3_mprintf("SELECT offset, length FROM data WHERE object IN (SELECT object FROM data WHERE value IN (SELECT value FROM data WHERE object = %s AND property = 'OMFI:MDAT:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:MobID' LIMIT 1) AND property = 'OMFI:MOBJ:Name' LIMIT 1", objects[k]), &fileName, &fileNameCount, 0, 0);
		if (fileNameCount > 0) {
			uint32_t fnOffs = atoi(fileName[2]);
			uint32_t fnLen = atoi(fileName[3]);
			if (get_offset_and_length (fileName[2], fileName[3], fnOffs, fnLen)) {
				char *fnBuf = (char*) malloc(fnLen+1);
				fseek(file, fnOffs, SEEK_SET);
				fread(fnBuf, fnLen, 1, file);
				fnBuf[fnLen] = '\0';

				audiofile_path_vector.push_back (fnBuf);
				full_path = Glib::build_filename (audiofile_path_vector);
				audiofile_path_vector.pop_back ();

				fd = fopen(full_path.c_str(), "wb");

			} else {
				INFO ("Skip unnamed media file\n");
				continue;
			}
		} else {
			audiofile_path_vector.push_back (objects[k]);
			full_path = Glib::build_filename (audiofile_path_vector);
			audiofile_path_vector.pop_back ();
			fd = fopen(full_path.c_str(), "wb");
			INFO ("Direct file name used (%s)\n", full_path.c_str());
		}

		if(fd == NULL){
			char error[255];
			sprintf(error, "Can't create file [%s] (object %s)", full_path.c_str(), objects[k]);
			MessageBox(NULL,error,"OMF Error", MB_OK);
			sqlite3_exec(db, "COMMIT", 0, 0, 0);
			sqlite3_close(db);
			return 1;
		} else {
			INFO("Writing file %s (object %s): ", full_path.c_str(), objects[k]);
		}
		uint32_t foffset;
		uint32_t flength;

		if (get_offset_and_length (objects[k+1], objects[k+2], foffset, flength)) {
			int blockSize = 1024;
			uint32_t currentBlock;
			uint32_t written = 0;
			char* buffer = (char*) malloc(blockSize);
			fseek(file, foffset, SEEK_SET);
			for (currentBlock = 0; currentBlock < flength / blockSize; currentBlock++) {
				fread(buffer, blockSize, 1, file);
				written += fwrite(buffer, 1, blockSize, fd);
			}
			fread(buffer, flength % blockSize, 1, file);
			written += fwrite(buffer, 1, flength % blockSize, fd);
			INFO("%d of %d bytes\n", written, flength);
			fclose(fd);

			get_audio_info (full_path);
		}
		sqlite3_free_table(fileName);
	}
	sqlite3_free_table(objects);
	/* ------------------ */

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	/* resolve ClassIDs */
	printf("Resolving ClassIDs ");
	char **classID;
	int classIDCount;
	int currentClsID;
	sqlite3_get_table(db,"SELECT object, property, value FROM data WHERE type = 'omfi:ClassID'", &classID, &classIDCount, 0, 0);
	sqlite3_exec(db,"DELETE FROM data WHERE type = 'omfi:ClassID'",0,0,0);
	INFO("(%d to be processed)... ", classIDCount);
	for (currentClsID = 3; currentClsID <= classIDCount * 3; currentClsID += 3) {
		//int clsID = (int) malloc(5);
		int clsID = atoi(classID[currentClsID+2]);
		clsID = e32(clsID);
		//sscanf(classID[currentClsID+2], "%d", &clsID);
		char clsString[5];
		strncpy(clsString, (char *) &clsID, 4);
		clsString[4] = 0;
		DEBUG("%d -> %s\n", clsID, clsString);

		sqlite3_exec(db,sqlite3_mprintf("INSERT INTO data VALUES (%s, '%s', 'omfi:ClassID', '%s', -1, -1)", classID[currentClsID], classID[currentClsID + 1], clsString),0,0,0);
	}
	sqlite3_free_table(classID);
	/* ---------------- */

	sqlite3_exec(db, "COMMIT", 0, 0, 0);

	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	starttime = endtime;

	/*time(&endtime);
	  printf("Took %ld seconds\n", endtime - starttime);
	  starttime = endtime;*/


	time(&endtime);
	INFO("done. (%ld seconds)\n", endtime - starttime);
	INFO("Overall time: %ld seconds\n", endtime - globalstart);

	return 0;
	/* -------- */
}
