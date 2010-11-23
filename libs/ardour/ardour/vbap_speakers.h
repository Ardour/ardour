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

#ifndef __libardour_vbap_speakers_h__
#define __libardour_vbap_speakers_h__

#include <string>
#include <map>

#include <pbd/signals.h>

#include "ardour/panner.h"

namespace ARDOUR {

class VBAPSpeakers {
  public:
        struct cart_vec {
            double x;
            double y;
            double z;
        };
        
        struct ang_vec {
            double azi;
            double ele;
            double length;
        };

        static const int MAX_TRIPLET_AMOUNT = 60;
        typedef std::vector<double> dvector;

        VBAPSpeakers ();
        ~VBAPSpeakers ();
        
        int  add_speaker (double direction, double elevation = 0.0);
        void remove_speaker (int id);
        void move_speaker (int id, double direction, double elevation = 0.0);
        void clear_speakers ();

        const dvector matrix (int tuple) const  { return _matrices[tuple]; }
        int speaker_for_tuple (int tuple, int which) const { return _speaker_tuples[tuple][which]; }

        int           n_tuples () const  { return _matrices.size(); }
        int           dimension() const { return _dimension; }

        static void angle_to_cart(ang_vec *from, cart_vec *to);

        PBD::Signal0<void> Changed;

  private:
        static const double MIN_VOL_P_SIDE_LGTH = 0.01;
        int   _dimension;  

        /* A struct for a loudspeaker instance */
        struct Speaker { 
            int id;
            cart_vec coords;
            ang_vec angles;
            
            Speaker (int, double azimuth, double elevation);

            void move (double azimuth, double elevation);
        };

        struct azimuth_sorter {
            bool operator() (const Speaker& s1, const Speaker& s2) {
                    return s1.angles.azi < s2.angles.azi;
            }
        };

        struct twoDmatrix : public dvector {
          twoDmatrix() : dvector (4, 0.0) {}
        };

        struct threeDmatrix : public dvector {
          threeDmatrix() : dvector (9, 0.0) {}
        };
        
        struct tmatrix : public dvector {
          tmatrix() : dvector (3, 0.0) {}
        };

        std::vector<Speaker>  _speakers;
        std::vector<dvector>  _matrices;       /* holds matrices for a given speaker combinations */
        std::vector<tmatrix>  _speaker_tuples; /* holds speakers IDs for a given combination */

        /* A struct for all loudspeakers */
        struct ls_triplet_chain {
            int ls_nos[3];
            float inv_mx[9];
            struct ls_triplet_chain *next;
        };

        static float vec_angle(cart_vec v1, cart_vec v2);
        static float vec_length(cart_vec v1);
        static float vec_prod(cart_vec v1, cart_vec v2);
        static float vol_p_side_lgth(int i, int j,int k, const std::vector<Speaker>&);
        static void  cross_prod(cart_vec v1,cart_vec v2, cart_vec *res);

        void update ();
        int  any_ls_inside_triplet (int a, int b, int c);
        void add_ldsp_triplet (int i, int j, int k, struct ls_triplet_chain **ls_triplets);
        int  lines_intersect (int i,int j,int k,int l);
        void calculate_3x3_matrixes (struct ls_triplet_chain *ls_triplets);
        void choose_speaker_triplets (struct ls_triplet_chain **ls_triplets);
        void choose_speaker_pairs ();
        void sort_2D_lss (int* sorted_lss);
        int  calc_2D_inv_tmatrix (double azi1,double azi2, double* inv_mat);
        
        void dump_speakers (std::ostream&);
};

} /* namespace */

#endif /* __libardour_vbap_speakers_h__ */
