#include "fst.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define MAX_STRING_LEN 256

#define FALSE 0
#define TRUE !FALSE

extern char * strdup (const char *);

static char *read_string( FILE *fp ) {
    char buf[MAX_STRING_LEN];

    fgets( buf, MAX_STRING_LEN, fp );
    if( strlen( buf ) < MAX_STRING_LEN ) {
	
	if( strlen(buf) )
	    buf[strlen(buf)-1] = 0;

	return strdup( buf );
    } else {
	return NULL;
    }
}

static VSTInfo *
load_fst_info_file (char* filename)
{
	VSTInfo *info = (VSTInfo *) malloc (sizeof (VSTInfo));
	FILE *fp;
	int i;
	
	if (info == NULL) {
		return NULL;
	}

	fp = fopen( filename, "r" );
	
	if (fp == NULL) {
		free (info);
		return NULL;
	}

    if( (info->name = read_string( fp )) == NULL ) goto error;
    if( (info->creator = read_string( fp )) == NULL ) goto error;
    if( 1 != fscanf( fp, "%d\n", &info->UniqueID ) ) goto error;
    if( (info->Category = read_string( fp )) == NULL ) goto error;
    if( 1 != fscanf( fp, "%d\n", &info->numInputs ) ) goto error;
    if( 1 != fscanf( fp, "%d\n", &info->numOutputs ) ) goto error;
    if( 1 != fscanf( fp, "%d\n", &info->numParams ) ) goto error;
    if( 1 != fscanf( fp, "%d\n", &info->wantMidi ) ) goto error;
    if( 1 != fscanf( fp, "%d\n", &info->hasEditor ) ) goto error;
    if( 1 != fscanf( fp, "%d\n", &info->canProcessReplacing ) ) goto error;

    if( (info->ParamNames = (char **) malloc( sizeof( char * ) * info->numParams )) == NULL ) goto error;
    for( i=0; i<info->numParams; i++ ) {
	if( (info->ParamNames[i] = read_string( fp )) == NULL ) goto error;
    }
    if( (info->ParamLabels = (char **) malloc( sizeof( char * ) * info->numParams )) == NULL ) goto error;
    for( i=0; i<info->numParams; i++ ) {
	if( (info->ParamLabels[i] = read_string( fp )) == NULL ) goto error;
    }
	

    fclose( fp );
    return info;

error:
    fclose( fp );
    free( info );
    return NULL;
}

static int
save_fst_info_file (VSTInfo* info, char* filename)
{
    FILE *fp;
    int i;


    if( info == NULL ) {
	fst_error( "info is NULL\n" );
	return TRUE;
    }

    fp = fopen( filename, "w" );
    
    if( fp == NULL ) {
	fst_error( "Cant write info file %s\n", filename );
	return TRUE;
    }

    fprintf( fp, "%s\n", info->name );
    fprintf( fp, "%s\n", info->creator );
    fprintf( fp, "%d\n", info->UniqueID );
    fprintf( fp, "%s\n", info->Category );
    fprintf( fp, "%d\n", info->numInputs );
    fprintf( fp, "%d\n", info->numOutputs );
    fprintf( fp, "%d\n", info->numParams );
    fprintf( fp, "%d\n", info->wantMidi );
    fprintf( fp, "%d\n", info->hasEditor );
    fprintf( fp, "%d\n", info->canProcessReplacing );

    for( i=0; i<info->numParams; i++ ) {
	fprintf( fp, "%s\n", info->ParamNames[i] );
    }
    for( i=0; i<info->numParams; i++ ) {
	fprintf( fp, "%s\n", info->ParamLabels[i] );
    }
	

    fclose( fp );

    return FALSE;
}

static char *fst_dllpath_to_infopath( char *dllpath ) {
    char *retval;
    if( strstr( dllpath, ".dll" ) == NULL ) return NULL;
    
    retval = strdup( dllpath );
    sprintf( retval + strlen(retval) - 4, ".fsi" );
    return retval;
}

static int fst_info_file_is_valid( char *dllpath ) {
    struct stat dllstat, fststat;
    char *fstpath = fst_dllpath_to_infopath( dllpath );

    if( !fstpath ) return FALSE;
    
    if( stat( dllpath, &dllstat ) ){ fst_error( "dll path %s invalid\n", dllpath );  return TRUE; }
    if( stat( fstpath, &fststat ) ) return FALSE;

    free( fstpath );
    if( dllstat.st_mtime > fststat.st_mtime )
	return FALSE;
    else 
	return TRUE;
}

static int
fst_can_midi (FST *fst)
{
	AEffect* plugin = fst->plugin;
	int vst_version = plugin->dispatcher (plugin, effGetVstVersion, 0, 0, NULL, 0.0f);

	if (vst_version >= 2) {
		
                /* should we send it VST events (i.e. MIDI) */
		
		if ((plugin->flags & effFlagsIsSynth) ||
		    (plugin->dispatcher (plugin, effCanDo, 0, 0,(void*) "receiveVstEvents", 0.0f) > 0))
		    return TRUE;
	}
	return FALSE;

}
static VSTInfo *
fst_info_from_plugin (FST* fst)
{
	VSTInfo* info = (VSTInfo *) malloc (sizeof (VSTInfo));
	AEffect* plugin;
	int i;
	char creator[65];

    if( ! fst ) {
	fst_error( "fst is NULL\n" );
	return NULL;
    }

    if( ! info ) return NULL;
    
    plugin = fst->plugin;
    

    info->name = strdup(fst->handle->name ); 
    plugin->dispatcher (plugin, 47 /* effGetVendorString */, 0, 0, creator, 0);
    if (strlen (creator) == 0) {
      info->creator = strdup ("Unknown");
    } else {
      info->creator = strdup (creator);
    }

    info->UniqueID = *((int32_t *) &plugin->uniqueID);

    info->Category = strdup( "None" );          // FIXME:  
    info->numInputs = plugin->numInputs;
    info->numOutputs = plugin->numOutputs;
    info->numParams = plugin->numParams;
    info->wantMidi = fst_can_midi( fst ); 
    info->hasEditor = plugin->flags & effFlagsHasEditor ? TRUE : FALSE;
    info->canProcessReplacing = plugin->flags & effFlagsCanReplacing ? TRUE : FALSE;

    info->ParamNames = (char **) malloc( sizeof( char * ) * info->numParams );
    info->ParamLabels = (char **) malloc( sizeof( char * ) * info->numParams );
    for( i=0; i<info->numParams; i++ ) {
	char name[20];
	char label[9];
	plugin->dispatcher (plugin,
			    effGetParamName,
			    i, 0, name, 0);
	info->ParamNames[i] = strdup( name );
	plugin->dispatcher (plugin,
			    6 /* effGetParamLabel */,
			    i, 0, label, 0);
	info->ParamLabels[i] = strdup( label );
    }
    return info;
}

// most simple one :) could be sufficient.... 
static intptr_t
simple_master_callback (AEffect *fx, int32_t opcode, int32_t index, intptr_t value, void *ptr, float opt)
{
	if (opcode == audioMasterVersion) {
		return 2;
	} else {
		return 0;
	}
}

VSTInfo *
fst_get_info (char* dllpath)
{
	if( fst_info_file_is_valid( dllpath ) ) {
		VSTInfo *info;
		char *fstpath = fst_dllpath_to_infopath( dllpath );
		
		info = load_fst_info_file( fstpath );
		free( fstpath );
		return info;

    } else {

	VSTHandle *h;
	FST *fst;
	VSTInfo *info;
	char *fstpath;

	if( !(h = fst_load( dllpath )) ) return NULL;
	if( !(fst = fst_instantiate( h, simple_master_callback, NULL )) ) {
	    fst_unload( h );
	    fst_error( "instantiate failed\n" );
	    return NULL;
	}
	fstpath = fst_dllpath_to_infopath( dllpath );
	if( !fstpath ) {
	    fst_close( fst );
	    fst_unload( h );
	    fst_error( "get fst filename failed\n" );
	    return NULL;
	}
	info = fst_info_from_plugin( fst );
	save_fst_info_file( info, fstpath );

	free( fstpath );
	fst_close( fst );
	fst_unload( h );
	return info;
    }
}

void
fst_free_info (VSTInfo *info)
{
    int i;

    for( i=0; i<info->numParams; i++ ) {
	free( info->ParamNames[i] );
	free( info->ParamLabels[i] );
    }
    free( info->name );
    free( info->creator );
    free( info->Category );
    free( info );
}


