/*
 *  MICO --- an Open Source CORBA implementation
 *  Copyright (c) 2003 Harald Böhme
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  For more information, visit the MICO Home Page at
 *  http://www.mico.org/
 */

/* Modified by Cedric Gustin <cedric.gustin@gmail.com> on 2006/01/13 :
 * Redirect the output of dumpbin to dumpbin.out instead of reading the
 * output stream of popen, as it fails with Visual Studio 2005 in 
 * pre-link build events.
 */

#include <iostream>
#include <fstream>
#include <stdio.h>

using namespace std;

int main(int argc,char** argv)
{
  if (argc < 4) {
	  cerr << "Usage: " << argv[0] << " <def-file-name> <dll-base-name> <obj-file> ...." << endl;
	  return 2;
  }

  // CG : Explicitly redirect stdout to dumpbin.out.
  string dumpbin = "dumpbin /SYMBOLS /OUT:dumpbin.out";
  int i = 3;

  for(;i<argc;) {
	  dumpbin += " ";
	  dumpbin += argv[i++];
  }

  FILE * dump; 
  
  if( (dump = _popen(dumpbin.c_str(),"r")) == NULL ) {
	  cerr << "could not popen dumpbin" << endl;
	  return 3;
  }

  // CG : Wait for the dumpbin process to finish and open dumpbin.out.
  _pclose(dump);
  dump=fopen("dumpbin.out","r");

  ofstream def_file(argv[1]);

  def_file << "LIBRARY " << argv[2] << endl;
  def_file << "EXPORTS" << endl;

  i=0;
  while( !feof(dump)) {
	  char buf [65000]; 
	  
	  if( fgets( buf, 64999, dump ) != NULL ) {
		  if(!strstr(buf," UNDEF ") && strstr(buf," External ")) {
			  char *s = strchr(buf,'|') + 1;
			  while(*s == ' ' || *s == '\t') s++;
			  char *e=s;
			  while(*e != ' ' && *e != '\t' && *e != '\0' && *e!= '\n') e++;
			  *e = '\0';
			
			if(strchr(s,'?')==0 && s[0]=='_' && strchr(s,'@') == 0 )//this is a C export type: _fct -> fct
				  def_file << "    " << (s+1) << endl;			
			else
			if(strchr(s,'?')!=0 && strncmp(s,"??_G",4)!=0 && strncmp(s,"??_E",4)!=0) {
				  def_file << "    " << s << endl;
			  }
		  }
	  }
  }

  // CG : Close dumpbin.out and delete it.
  fclose(dump);
  remove("dumpbin.out");

  cout << dumpbin.c_str() << endl;
}
