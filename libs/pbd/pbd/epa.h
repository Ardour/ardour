/*
    Copyright (C) 2010 Paul Davis 

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

#ifndef __libpbd_epa_h__
#define __libpbd_epa_h__

#include <map>
#include <string>

namespace PBD {

class EnvironmentalProtectionAgency {
  public:
        EnvironmentalProtectionAgency (bool arm = true, const std::string& envname = std::string());
        ~EnvironmentalProtectionAgency ();
        
        void arm ();
        void save ();
        void restore () const;
        
        static EnvironmentalProtectionAgency* get_global_epa () { return _global_epa; }
        static void set_global_epa (EnvironmentalProtectionAgency* epa) { _global_epa = epa; }
        
  private:
        void clear () const;
		
        bool _armed;
        std::string _envname;
        std::map<std::string,std::string> e;
        static EnvironmentalProtectionAgency* _global_epa;
};

}

#endif /* __libpbd_epa_h__ */
