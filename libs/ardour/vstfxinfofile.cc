/***********************************************************/
/*vstfx infofile - module to manage info files             */
/*containing cached information about a plugin. e.g. its   */
/*name, creator etc etc                                    */
/***********************************************************/

/*This is largely unmodified from the original (C code) FST vstinfofile module*/

#include "ardour/vstfx.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <libgen.h>

#define MAX_STRING_LEN 256

#define FALSE 0
#define TRUE !FALSE

static char* read_string(FILE *fp)
{
    char buf[MAX_STRING_LEN];

    fgets( buf, MAX_STRING_LEN, fp );
	
    if(strlen(buf) < MAX_STRING_LEN)
	{
		if(strlen(buf))
	    	buf[strlen(buf)-1] = 0;

		return strdup(buf);
    }
	else
	{
		return NULL;
    }
}

static VSTFXInfo* load_vstfx_info_file(char *filename)
{
	VSTFXInfo *info = (VSTFXInfo*) malloc(sizeof(VSTFXInfo));
    FILE *fp;
    int i;
	
    if(info == NULL)
		return NULL;

    fp = fopen(filename, "r");
    
    if(fp == NULL)
	{
		free( info );
		return NULL;
    }

    if((info->name = read_string(fp)) == NULL) goto error;
    if((info->creator = read_string(fp)) == NULL) goto error;
    if(1 != fscanf(fp, "%d\n", &info->UniqueID)) goto error;
    if((info->Category = read_string(fp)) == NULL) goto error;
    if(1 != fscanf(fp, "%d\n", &info->numInputs)) goto error;
    if(1 != fscanf(fp, "%d\n", &info->numOutputs)) goto error;
    if(1 != fscanf(fp, "%d\n", &info->numParams)) goto error;
    if(1 != fscanf(fp, "%d\n", &info->wantMidi)) goto error;
    if(1 != fscanf(fp, "%d\n", &info->hasEditor)) goto error;
    if(1 != fscanf(fp, "%d\n", &info->canProcessReplacing)) goto error;

    if((info->ParamNames = (char **) malloc(sizeof(char*)*info->numParams)) == NULL) goto error;
    for(i=0; i<info->numParams; i++)
	{
		if((info->ParamNames[i] = read_string(fp)) == NULL) goto error;
    }
    if((info->ParamLabels = (char **) malloc(sizeof(char*)*info->numParams)) == NULL) goto error;
    
	for(i=0; i < info->numParams; i++)
	{
		if((info->ParamLabels[i] = read_string(fp)) == NULL) goto error;
    }
	
    fclose( fp );
    return info;

error:
    fclose( fp );
    free( info );
    return NULL;
}

static int save_vstfx_info_file(VSTFXInfo *info, char *filename)
{
    FILE *fp;
    int i;

    if(info == NULL)
	{
		vstfx_error("** ERROR ** VSTFXinfofile : info ptr is NULL\n");
		return TRUE;
    }

    fp = fopen(filename, "w");
    
    if(fp == NULL)
	{
		vstfx_error("** WARNING ** VSTFX : Can't write info file %s\n", filename);
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

    for(i=0; i < info->numParams; i++)
	{
		fprintf(fp, "%s\n", info->ParamNames[i]);
    }
	
    for(i=0; i < info->numParams; i++)
	{
		fprintf(fp, "%s\n", info->ParamLabels[i]);
    }
	
    fclose( fp );

    return FALSE;
}

static char* vstfx_dllpath_to_infopath(char *dllpath)
{
    char* retval;
	char* dir_path;
	char* base_name;
	
    if(strstr(dllpath, ".so" ) == NULL)
		return NULL;
    
	/*Allocate space for the filename - need strlen + 1 for the terminating'0', +1 because .so is three
	chars, and .fsi is four chars and +1 because we have a '.' at the beginning*/
	
	retval = (char*)malloc(strlen(dllpath) + 3);
	
	dir_path = strdup(dllpath);
	base_name = strdup(dllpath);
	
	sprintf(retval, "%s/.%s", dirname(dir_path), basename(base_name));
	sprintf(retval + strlen(retval) - 3, ".fsi");
	
	free(dir_path);
	free(base_name);
	
    return retval;
}

static int vstfx_info_file_is_valid(char *dllpath)
{
    struct stat dllstat;
	struct stat vstfxstat;
	
    char *vstfxpath = vstfx_dllpath_to_infopath(dllpath);
	
    if(!vstfxpath)
		return FALSE;
    
    if(stat(dllpath, &dllstat))
	{
		vstfx_error( "** ERROR ** VSTFXinfofile : .so path %s invalid\n", dllpath );
			return TRUE;
	}
	
    if(stat(vstfxpath, &vstfxstat))
		return FALSE;

    free(vstfxpath);
	
    if(dllstat.st_mtime > vstfxstat.st_mtime)
		return FALSE;
    else 
		return TRUE;
}

static int vstfx_can_midi(VSTFX *vstfx)
{
	struct AEffect *plugin = vstfx->plugin;
	
	int vst_version = plugin->dispatcher (plugin, effGetVstVersion, 0, 0, NULL, 0.0f);

	if (vst_version >= 2)
	{
		/* should we send it VST events (i.e. MIDI) */
		
		if ((plugin->flags & effFlagsIsSynth) || (plugin->dispatcher (plugin, effCanDo, 0, 0,(void*) "receiveVstEvents", 0.0f) > 0))
		    return TRUE;
	}
	return FALSE;
}

static VSTFXInfo* vstfx_info_from_plugin(VSTFX *vstfx)
{

	VSTFXInfo* info = (VSTFXInfo*) malloc(sizeof(VSTFXInfo));
	
    struct AEffect *plugin;
    int i;

	/*We need to init the creator because some plugins
	fail to implement getVendorString, and so won't stuff the
	string with any name*/
	
    char creator[65] = "Unknown\0";

    if(!vstfx)
	{
		vstfx_error( "** ERROR ** VSTFXinfofile : vstfx ptr is NULL\n" );
		return NULL;
    }

    if(!info)
		return NULL;
    
    plugin = vstfx->plugin;
	
    info->name = strdup(vstfx->handle->name ); 
	
	/*If the plugin doesn't bother to implement GetVendorString we will
	have pre-stuffed the string with 'Unkown' */
	
    plugin->dispatcher (plugin, effGetVendorString, 0, 0, creator, 0);
	
	/*Some plugins DO implement GetVendorString, but DON'T put a name in it
	so if its just a zero length string we replace it with 'Unknown' */
	
    if (strlen(creator) == 0)
	{
      info->creator = strdup("Unknown");
    }
	else
	{
      info->creator = strdup (creator);
    }

    info->UniqueID = plugin->uniqueID;

    info->Category = strdup("None");          // FIXME:  
    info->numInputs = plugin->numInputs;
    info->numOutputs = plugin->numOutputs;
    info->numParams = plugin->numParams;
    info->wantMidi = vstfx_can_midi(vstfx); 
	info->hasEditor = plugin->flags & effFlagsHasEditor ? TRUE : FALSE;
    info->canProcessReplacing = plugin->flags & effFlagsCanReplacing ? TRUE : FALSE;
    info->ParamNames = (char **) malloc(sizeof(char*)*info->numParams);
    info->ParamLabels = (char **) malloc(sizeof(char*)*info->numParams);
    for(i=0; i < info->numParams; i++)
	{
		char name[64];
		char label[64];
		
		/*Not all plugins give parameters labels as well as names*/
		
		strcpy(name, "No Name");
		strcpy(label, "No Label");
		
		plugin->dispatcher (plugin, effGetParamName, i, 0, name, 0);
		info->ParamNames[i] = strdup(name);
		
		//NOTE: 'effGetParamLabel' is no longer defined in vestige headers
		//plugin->dispatcher (plugin, effGetParamLabel, i, 0, label, 0);
		info->ParamLabels[i] = strdup(label);
    }
    return info;
}

/* A simple 'dummy' audiomaster callback which should be ok,
we will only be instantiating the plugin in order to get its info*/

static intptr_t simple_master_callback(struct AEffect *, int32_t opcode, int32_t, intptr_t, void *, float)
{
	if (opcode == audioMasterVersion)
		return 2;
	else
		return 0;
}

/*Try to get plugin info - first by looking for a .fsi cache of the
data, and if that doesn't exist, load the plugin, get its data and
then cache it for future ref*/

VSTFXInfo *vstfx_get_info(char *dllpath)
{
	if( vstfx_info_file_is_valid(dllpath))
	{
		VSTFXInfo *info;
		char *vstfxpath = vstfx_dllpath_to_infopath(dllpath);

		info = load_vstfx_info_file(vstfxpath);
		free(vstfxpath);
		
		return info;
    }
	else
	{
		VSTFXHandle *h;
		VSTFX *vstfx;
		VSTFXInfo *info;
		
		char *vstfxpath;
		
		if(!(h = vstfx_load(dllpath)))
			return NULL;
							
		if(!(vstfx = vstfx_instantiate(h, simple_master_callback, NULL)))
		{
	    	vstfx_unload(h);
	    	vstfx_error( "** ERROR ** VSTFXinfofile : Instantiate failed\n" );
	    	return NULL;
		}
		
		vstfxpath = vstfx_dllpath_to_infopath(dllpath);
		
		if(!vstfxpath)
		{
	    	vstfx_close(vstfx);
	    	vstfx_unload(h);
	    	vstfx_error( "** ERROR ** VSTFXinfofile : get vstfx filename failed\n" );
	    	return NULL;
		}
		
		info = vstfx_info_from_plugin(vstfx);
		
		save_vstfx_info_file(info, vstfxpath);

		free(vstfxpath);
		
		vstfx_close(vstfx);
		vstfx_unload(h);
		
		return info;
    }
}

void vstfx_free_info(VSTFXInfo *info )
{
    int i;

    for(i=0; i < info->numParams; i++)
	{
		free(info->ParamNames[i]);
		free(info->ParamLabels[i]);
    }
	
    free(info->name);
    free(info->creator);
    free(info->Category);
    free(info);
}


