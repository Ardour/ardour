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
#include <vector>

#include <boost/utility.hpp>

#include <pbd/signals.h>

#include "ardour/panner.h"
#include "ardour/speakers.h"

namespace ARDOUR {

class Speakers;

class VBAPSpeakers : public boost::noncopyable {
public:
	VBAPSpeakers (boost::shared_ptr<Speakers>);

	typedef std::vector<double> dvector;
	const dvector matrix (int tuple) const  { return _matrices[tuple]; }
	int speaker_for_tuple (int tuple, int which) const { return _speaker_tuples[tuple][which]; }

	int           n_tuples () const  { return _matrices.size(); }
	int           dimension() const { return _dimension; }

        uint32_t n_speakers() const { return _speakers.size(); }
        boost::shared_ptr<Speakers> parent() const { return _parent; }

	~VBAPSpeakers ();

private:
	static const double MIN_VOL_P_SIDE_LGTH;
	int   _dimension;  
        boost::shared_ptr<Speakers> _parent;
	std::vector<Speaker> _speakers;
	PBD::ScopedConnection speaker_connection;

	struct azimuth_sorter {
		bool operator() (const Speaker& s1, const Speaker& s2) {
			return s1.angles().azi < s2.angles().azi;
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

	std::vector<dvector>  _matrices;       /* holds matrices for a given speaker combinations */
	std::vector<tmatrix>  _speaker_tuples; /* holds speakers IDs for a given combination */

	/* A struct for all loudspeakers */
	struct ls_triplet_chain {
		int ls_nos[3];
		float inv_mx[9];
		struct ls_triplet_chain *next;
	};

	static double vec_angle(PBD::CartesianVector v1, PBD::CartesianVector v2);
	static double vec_length(PBD::CartesianVector v1);
	static double vec_prod(PBD::CartesianVector v1, PBD::CartesianVector v2);
	static double vol_p_side_lgth(int i, int j,int k, const std::vector<Speaker>&);
	static void   cross_prod(PBD::CartesianVector v1,PBD::CartesianVector v2, PBD::CartesianVector *res);

	void update ();
	int  any_ls_inside_triplet (int a, int b, int c);
	void add_ldsp_triplet (int i, int j, int k, struct ls_triplet_chain **ls_triplets);
	int  lines_intersect (int i,int j,int k,int l);
	void calculate_3x3_matrixes (struct ls_triplet_chain *ls_triplets);
	void choose_speaker_triplets (struct ls_triplet_chain **ls_triplets);
	void choose_speaker_pairs ();
	void sort_2D_lss (int* sorted_lss);
	int  calc_2D_inv_tmatrix (double azi1,double azi2, double* inv_mat);
        
};

} /* namespace */

#endif /* __libardour_vbap_speakers_h__ */
