/* $Id: generate_extra_defs.cc 793 2009-03-09 17:49:00Z daniel $ */

/* generate_extra_defs.cc
 *
 * Copyright (C) 2001 The Free Software Foundation
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include "generate_extra_defs.h"
#include <algorithm>

std::string get_properties(GType gtype)
{
  std::string strResult;
  std::string strObjectName = g_type_name(gtype);

  //Get the list of properties:
  GParamSpec** ppParamSpec = 0;
  guint iCount = 0;
  if(G_TYPE_IS_OBJECT(gtype))
  {
    GObjectClass* pGClass = G_OBJECT_CLASS(g_type_class_ref(gtype));
    ppParamSpec = g_object_class_list_properties (pGClass, &iCount);
    g_type_class_unref(pGClass);

    if(!ppParamSpec)
    {
      strResult += ";; Warning: g_object_class_list_properties() returned NULL for " + std::string(g_type_name(gtype)) + "\n";
    }
  }
  else if (G_TYPE_IS_INTERFACE(gtype))
  {
    gpointer pGInterface = g_type_default_interface_ref(gtype);
    if(pGInterface) //We check because this fails for G_TYPE_VOLUME, for some reason.
    {
      ppParamSpec = g_object_interface_list_properties(pGInterface, &iCount);
      g_type_default_interface_unref(pGInterface);

      if(!ppParamSpec)
      {
        strResult +=  ";; Warning: g_object_interface_list_properties() returned NULL for " + std::string(g_type_name(gtype)) + "\n";
      }
    }
  }

  //This extra check avoids an occasional crash, for instance for GVolume
  if(!ppParamSpec)
    iCount = 0;

  for(guint i = 0; i < iCount; i++)
  {
    GParamSpec* pParamSpec = ppParamSpec[i];
    if(pParamSpec)
    {
      //Name and type:
      const std::string strName = g_param_spec_get_name(pParamSpec);
      const std::string strTypeName = G_PARAM_SPEC_TYPE_NAME(pParamSpec);

      const gchar* pchBlurb = g_param_spec_get_blurb(pParamSpec);
      std::string strDocs = (pchBlurb) ? pchBlurb : "";
      // Quick hack to get rid of nested double quotes:
      std::replace(strDocs.begin(), strDocs.end(), '"', '\'');

      strResult += "(define-property " + strName + "\n";
      strResult += "  (of-object \"" + strObjectName + "\")\n";
      strResult += "  (prop-type \"" + strTypeName + "\")\n";
      strResult += "  (docs \"" + strDocs + "\")\n";

      //Flags:
      GParamFlags flags = pParamSpec->flags;
      bool bReadable = (flags & G_PARAM_READABLE) == G_PARAM_READABLE;
      bool bWritable = (flags & G_PARAM_WRITABLE) == G_PARAM_WRITABLE;
      bool bConstructOnly = (flags & G_PARAM_CONSTRUCT_ONLY) == G_PARAM_CONSTRUCT_ONLY;

      //#t and #f aren't documented, but I guess that it's correct based on the example in the .defs spec.
      const std::string strTrue = "#t";
      const std::string strFalse = "#f";

      strResult += "  (readable " + (bReadable ? strTrue : strFalse) + ")\n";
      strResult += "  (writable " + (bWritable ? strTrue : strFalse) + ")\n";
      strResult += "  (construct-only " + (bConstructOnly ? strTrue : strFalse) + ")\n";

      strResult += ")\n\n"; //close (define-property		
    }
  }

  g_free(ppParamSpec);

  return strResult;
}

std::string get_type_name(GType gtype) //Adds a * if necessary.
{
  std::string strTypeName = g_type_name(gtype);

  if( g_type_is_a(gtype, G_TYPE_OBJECT) || g_type_is_a(gtype, G_TYPE_BOXED) )
    strTypeName += "*";  //Add * to show that it's a pointer.
  else if( g_type_is_a(gtype, G_TYPE_STRING) )
    strTypeName = "gchar*"; //g_type_name() returns "gchararray".

  return strTypeName;
}

std::string get_type_name_parameter(GType gtype)
{
  std::string strTypeName = get_type_name(gtype);

  //All signal parameters that are registered as GTK_TYPE_STRING are actually const gchar*.
  if(strTypeName == "gchar*")
    strTypeName = "const-gchar*";

  return strTypeName;
}

std::string get_type_name_signal(GType gtype)
{
  return get_type_name_parameter(gtype); //At the moment, it needs the same stuff.
}


std::string get_signals(GType gtype)
{
  std::string strResult;
  std::string strObjectName = g_type_name(gtype);

  gpointer gclass_ref = 0;
  gpointer ginterface_ref = 0;

  if(G_TYPE_IS_OBJECT(gtype))
    gclass_ref = g_type_class_ref(gtype); //Ensures that class_init() is called.
  else if(G_TYPE_IS_INTERFACE(gtype))
    ginterface_ref = g_type_default_interface_ref(gtype); //install signals.

  //Get the list of signals:
  guint iCount = 0;
  guint* pIDs = g_signal_list_ids (gtype, &iCount);

  //Loop through the list of signals:
  if(pIDs)
  {
    for(guint i = 0; i < iCount; i++)
    {
      guint signal_id = pIDs[i];

      //Name:
      std::string strName = g_signal_name(signal_id);
      strResult += "(define-signal " + strName + "\n";
      strResult += "  (of-object \"" + strObjectName + "\")\n";



      //Other information about the signal:
      GSignalQuery signalQuery = { 0, 0, 0, GSignalFlags(0), 0, 0, 0, };
      g_signal_query(signal_id, &signalQuery);

      //Return type:
      std::string strReturnTypeName = get_type_name_signal( signalQuery.return_type & ~G_SIGNAL_TYPE_STATIC_SCOPE ); //The type is mangled with a flag. Hacky.
      //bool bReturnTypeHasStaticScope = (signalQuery.return_type & G_SIGNAL_TYPE_STATIC_SCOPE) == G_SIGNAL_TYPE_STATIC_SCOPE;
      strResult += "  (return-type \"" + strReturnTypeName + "\")\n";


      //When:
      {
        bool bWhenFirst = (signalQuery.signal_flags & G_SIGNAL_RUN_FIRST) == G_SIGNAL_RUN_FIRST;
        bool bWhenLast = (signalQuery.signal_flags & G_SIGNAL_RUN_LAST) == G_SIGNAL_RUN_LAST;

        std::string strWhen = "unknown";

        if(bWhenFirst && bWhenLast)
          strWhen = "both";
        else if(bWhenFirst)
          strWhen = "first";
        else if(bWhenLast)
          strWhen = "last";

        strResult += "  (when \"" + strWhen + "\")\n";
      }


      //Loop through the list of parameters:
      const GType* pParameters = signalQuery.param_types;
      if(pParameters)
      {
        strResult += "  (parameters\n";

        for(unsigned i = 0; i < signalQuery.n_params; i++)
        {
          GType typeParamMangled = pParameters[i];

          //Parameter name:
          //TODO: How can we get the real parameter name?
          gchar* pchNum = g_strdup_printf("%d", i);
          std::string strParamName = "p" + std::string(pchNum);
          g_free(pchNum);
          pchNum = 0;

          //Just like above, for the return type:
          std::string strTypeName = get_type_name_signal( typeParamMangled & ~G_SIGNAL_TYPE_STATIC_SCOPE ); //The type is mangled with a flag. Hacky.
          //bool bReturnTypeHasStaticScope = (typeParamMangled & G_SIGNAL_TYPE_STATIC_SCOPE) == G_SIGNAL_TYPE_STATIC_SCOPE;

          strResult += "    '(\"" + strTypeName + "\" \"" + strParamName + "\")\n";
        }

        strResult += "  )\n"; //close (properties
      }

      strResult += ")\n\n"; //close (define=signal
    }
  }

  g_free(pIDs);

  if(gclass_ref)
    g_type_class_unref(gclass_ref); //to match the g_type_class_ref() above.
  else if(ginterface_ref)
    g_type_default_interface_unref(ginterface_ref); // for interface ref above.

  return strResult;
}



std::string get_defs(GType gtype)
{
  std::string strObjectName = g_type_name(gtype);
  std::string strDefs = ";; From " + strObjectName + "\n\n";

  if(G_TYPE_IS_OBJECT(gtype) || G_TYPE_IS_INTERFACE(gtype))
  {
    strDefs += get_signals(gtype);
    strDefs += get_properties(gtype);
  }

  return strDefs;
}



