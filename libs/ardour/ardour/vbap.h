/* 
   This software is being provided to you, the licensee, by Ville Pulkki,
   under the following license. By obtaining, using and/or copying this
   software, you agree that you have read, understood, and will comply
   with these terms and conditions: Permission to use, copy, modify and
   distribute, including the right to grant others rights to distribute
   at any tier, this software and its documentation for any purpose and
   without fee or royalty is hereby granted, provided that you agree to
   comply with the following copyright notice and statements, including
   the disclaimer, and that the same appear on ALL copies of the software
   and documentation, including modifications that you make for internal
   use or for distribution:
   
   Copyright 1998 by Ville Pulkki, Helsinki University of Technology.  All
   rights reserved.  
   
   The software may be used, distributed, and included to commercial
   products without any charges. When included to a commercial product,
   the method "Vector Base Amplitude Panning" and its developer Ville
   Pulkki must be referred to in documentation.
   
   This software is provided "as is", and Ville Pulkki or Helsinki
   University of Technology make no representations or warranties,
   expressed or implied. By way of example, but not limitation, Helsinki
   University of Technology or Ville Pulkki make no representations or
   warranties of merchantability or fitness for any particular purpose or
   that the use of the licensed software or documentation will not
   infringe any third party patents, copyrights, trademarks or other
   rights. The name of Ville Pulkki or Helsinki University of Technology
   may not be used in advertising or publicity pertaining to distribution
   of the software.
*/

#ifndef __libardour_vbap_h__
#define __libardour_vbap_h__

#include <string>
#include <map>

#include "ardour/panner.h"

namespace ARDOUR {

class VBAPSpeakers {
  public:
        struct cart_vec {
            float x;
            float y;
            float z;
        };
        
        struct ang_vec {
            float azi;
            float ele;
            float length;
        };

        static const int MAX_TRIPLET_AMOUNT = 60;

        VBAPSpeakers ();
        ~VBAPSpeakers ();
        
        int  add_speaker (double direction, double elevation = 0.0);
        void remove_speaker (int id);
        void move_speaker (int id, double direction, double elevation = 0.0);
        
        const double* matrix (int tuple) const  { return _matrices[tuple]; }
        int speaker_for_tuple (int tuple, int which) const { return _speaker_tuples[tuple][which]; }

        int           n_tuples () const  { return _matrices.size(); }
        int           dimension() const { return _dimension; }

        static void angle_to_cart(ang_vec *from, cart_vec *to);

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

        std::vector<Speaker> _speakers;
        std::vector<double[9]> _matrices;       /* holds matrices for a given speaker combinations */
        std::vector<int[3]>    _speaker_tuples; /* holds speakers IDs for a given combination */

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
        void choose_ls_triplets (struct ls_triplet_chain **ls_triplets);
        void choose_ls_pairs ();
        void sort_2D_lss (int* sorted_lss);
        int  calc_2D_inv_tmatrix (double azi1,double azi2, double* inv_mat);
};

class VBAPanner : public StreamPanner { 
  public:
        VBAPanner (Panner& parent, Evoral::Parameter param, VBAPSpeakers& s);
        ~VBAPanner ();

        void do_distribute (AudioBuffer&, BufferSet& obufs, gain_t gain_coeff, nframes_t nframes);

        void set_azimuth_elevation (double azimuth, double elevation);

        /* a utility function to convert azimuth+elevation into cartesian coordinates 
           as used by the StreamPanner API
        */
        
        void azi_ele_to_cart (int azi, int ele, double* c);

  private:
        double        _azimuth;   /* direction for the signal source */
        double        _elevation; /* elevation of the signal source */
        VBAPSpeakers& _speakers;

        void compute_gains (double g[3], int ls[3], int azi, int ele);

        void update ();
};

} /* namespace */

#endif /* __libardour_vbap_h__ */
