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

#ifdef COMPILER_MSVC
#pragma warning ( disable : 4244 )
#endif

#include <vector>
#include <cmath>
#include <algorithm>
#include <stdlib.h>

#include "pbd/cartesian.h"

#include "vbap_speakers.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

const double VBAPSpeakers::MIN_VOL_P_SIDE_LGTH = 0.01;

typedef std::vector<double>        DoubleVector;
typedef std::vector<float>         FloatVector;
typedef std::vector<bool>          BoolVector;
typedef std::vector<int>           IntVector;
typedef std::vector<IntVector>     IntVector2D;
typedef std::vector<DoubleVector>  DoubleVector2D;

VBAPSpeakers::VBAPSpeakers (boost::shared_ptr<Speakers> s)
	: _dimension (2)
        , _parent (s)
{
	_parent->Changed.connect_same_thread (speaker_connection, boost::bind (&VBAPSpeakers::update, this));
        update ();
}

VBAPSpeakers::~VBAPSpeakers ()
{
}

void
VBAPSpeakers::update ()
{
	int dim = 2;

        _speakers = _parent->speakers();

	for (vector<Speaker>::const_iterator i = _speakers.begin(); i != _speakers.end(); ++i) {
		if ((*i).angles().ele != 0.0) {
			dim = 3;
			break;
		}
	}

	_dimension = dim;

	if (_speakers.size() < 2) {
		/* nothing to be done with less than two speakers */
		return;
	}

	if (_dimension == 3)  {
		ls_triplet_chain *ls_triplets = 0;
		choose_speaker_triplets (&ls_triplets);
		if (ls_triplets) {
			calculate_3x3_matrixes (ls_triplets);
			free (ls_triplets);
		}
	} else {
		choose_speaker_pairs ();
	}
}

void 
VBAPSpeakers::choose_speaker_triplets(struct ls_triplet_chain **ls_triplets) 
{
	/* Selects the loudspeaker triplets, and
	   calculates the inversion matrices for each selected triplet.
	   A line (connection) is drawn between each loudspeaker. The lines
	   denote the sides of the triangles. The triangles should not be 
	   intersecting. All crossing connections are searched and the 
	   longer connection is erased. This yields non-intesecting triangles,
	   which can be used in panning.
	*/

	int i,j,k,l,table_size;
	int n_speakers = _speakers.size ();

	if (n_speakers < 1) {
		return;
	}

	FloatVector  distance_table(((n_speakers * (n_speakers - 1)) / 2));
	IntVector    distance_table_i(((n_speakers * (n_speakers - 1)) / 2));
	IntVector    distance_table_j(((n_speakers * (n_speakers - 1)) / 2));
	IntVector2D  connections(n_speakers, IntVector(n_speakers));
	float        distance;
	struct ls_triplet_chain *trip_ptr, *prev, *tmp_ptr;

	for (i = 0; i < n_speakers; i++) {
		for (j = i+1; j < n_speakers; j++) {
			for(k=j+1;k<n_speakers;k++) {
				if (vol_p_side_lgth(i,j, k, _speakers) > MIN_VOL_P_SIDE_LGTH){
					connections[i][j]=1;
					connections[j][i]=1;
					connections[i][k]=1;
					connections[k][i]=1;
					connections[j][k]=1;
					connections[k][j]=1;
					add_ldsp_triplet(i,j,k,ls_triplets);
				}
			}
		}
	}

	/*calculate distancies between all speakers and sorting them*/
	table_size =(((n_speakers - 1) * (n_speakers)) / 2); 
	for (i = 0; i < table_size; i++) {
		distance_table[i] = 100000.0;
	}

	for (i = 0;i < n_speakers; i++) { 
		for (j = i+1; j < n_speakers; j++) { 
			if (connections[i][j] == 1) {
				distance = fabs(vec_angle(_speakers[i].coords(),_speakers[j].coords()));
				k=0;
				while(distance_table[k] < distance) {
					k++;
				}
				for (l = table_size - 1; l > k ; l--) {
					distance_table[l] = distance_table[l-1];
					distance_table_i[l] = distance_table_i[l-1];
					distance_table_j[l] = distance_table_j[l-1];
				}
				distance_table[k] = distance;
				distance_table_i[k] = i;
				distance_table_j[k] = j;
			} else
				table_size--;
		}
	}

	/* disconnecting connections which are crossing shorter ones,
	   starting from shortest one and removing all that cross it,
	   and proceeding to next shortest */
	for (i = 0; i < table_size; i++) {
		int fst_ls = distance_table_i[i];
		int sec_ls = distance_table_j[i];
		if (connections[fst_ls][sec_ls] == 1) {
			for (j = 0; j < n_speakers; j++) {
				for (k = j+1; k < n_speakers; k++) {
					if ((j!=fst_ls) && (k != sec_ls) && (k!=fst_ls) && (j != sec_ls)){
						if (lines_intersect(fst_ls, sec_ls, j,k) == 1){
							connections[j][k] = 0;
							connections[k][j] = 0;
						}
					}
				}
			}
		}
	}

	/* remove triangles which had crossing sides
	   with smaller triangles or include loudspeakers*/
	trip_ptr = *ls_triplets;
	prev = 0;
	while (trip_ptr != 0){
		i = trip_ptr->ls_nos[0];
		j = trip_ptr->ls_nos[1];
		k = trip_ptr->ls_nos[2];
		if (connections[i][j] == 0 || 
		    connections[i][k] == 0 || 
		    connections[j][k] == 0 ||
		    any_ls_inside_triplet(i,j,k) == 1 ){
			if (prev != 0) {
				prev->next = trip_ptr->next;
				tmp_ptr = trip_ptr;
				trip_ptr = trip_ptr->next;
				free(tmp_ptr);
			} else {
				*ls_triplets = trip_ptr->next;
				tmp_ptr = trip_ptr;
				trip_ptr = trip_ptr->next;
				free(tmp_ptr);
			}
		} else {
			prev = trip_ptr;
			trip_ptr = trip_ptr->next;

		}
	}
}

int 
VBAPSpeakers::any_ls_inside_triplet(int a, int b, int c)
{
	/* returns 1 if there is loudspeaker(s) inside given ls triplet */
	float invdet;
	const CartesianVector* lp1;
	const CartesianVector* lp2;
	const CartesianVector* lp3;
	float invmx[9];
	int i,j;
	float tmp;
	bool any_ls_inside;
	bool this_inside;
	int n_speakers = _speakers.size();

	lp1 =  &(_speakers[a].coords());
	lp2 =  &(_speakers[b].coords());
	lp3 =  &(_speakers[c].coords());
        
	/* matrix inversion */
	invdet = 1.0 / (  lp1->x * ((lp2->y * lp3->z) - (lp2->z * lp3->y))
	                  - lp1->y * ((lp2->x * lp3->z) - (lp2->z * lp3->x))
	                  + lp1->z * ((lp2->x * lp3->y) - (lp2->y * lp3->x)));
        
	invmx[0] = ((lp2->y * lp3->z) - (lp2->z * lp3->y)) * invdet;
	invmx[3] = ((lp1->y * lp3->z) - (lp1->z * lp3->y)) * -invdet;
	invmx[6] = ((lp1->y * lp2->z) - (lp1->z * lp2->y)) * invdet;
	invmx[1] = ((lp2->x * lp3->z) - (lp2->z * lp3->x)) * -invdet;
	invmx[4] = ((lp1->x * lp3->z) - (lp1->z * lp3->x)) * invdet;
	invmx[7] = ((lp1->x * lp2->z) - (lp1->z * lp2->x)) * -invdet;
	invmx[2] = ((lp2->x * lp3->y) - (lp2->y * lp3->x)) * invdet;
	invmx[5] = ((lp1->x * lp3->y) - (lp1->y * lp3->x)) * -invdet;
	invmx[8] = ((lp1->x * lp2->y) - (lp1->y * lp2->x)) * invdet;
        
	any_ls_inside = false;
	for (i = 0; i < n_speakers; i++) {
		if (i != a && i!=b && i != c) {
			this_inside = true;
			for (j = 0; j < 3; j++) {
				tmp = _speakers[i].coords().x * invmx[0 + j*3];
				tmp += _speakers[i].coords().y * invmx[1 + j*3];
				tmp += _speakers[i].coords().z * invmx[2 + j*3];
				if (tmp < -0.001) {
					this_inside = false;
				}
			}
			if (this_inside) {
				any_ls_inside = true;
			}
		}
	}

	return any_ls_inside;
}


void 
VBAPSpeakers::add_ldsp_triplet(int i, int j, int k, struct ls_triplet_chain **ls_triplets)
{
	/* adds i,j,k triplet to triplet chain*/

	struct ls_triplet_chain *trip_ptr, *prev;
	trip_ptr = *ls_triplets;
	prev = 0;
        
	while (trip_ptr != 0){
		prev = trip_ptr;
		trip_ptr = trip_ptr->next;
	}

	trip_ptr = (struct ls_triplet_chain*) malloc (sizeof (struct ls_triplet_chain));

	if (prev == 0) {
		*ls_triplets = trip_ptr;
	} else {
		prev->next = trip_ptr;
	}

	trip_ptr->next = 0;
	trip_ptr->ls_nos[0] = i;
	trip_ptr->ls_nos[1] = j;
	trip_ptr->ls_nos[2] = k;
}

float 
VBAPSpeakers::vec_angle(CartesianVector v1, CartesianVector v2)
{
	float inner= ((v1.x*v2.x + v1.y*v2.y + v1.z*v2.z)/
	              (vec_length(v1) * vec_length(v2)));

	if (inner > 1.0) {
		inner= 1.0;
	}

	if (inner < -1.0) {
		inner = -1.0;
	}

	return fabsf((float) acos((double) inner));
}

float 
VBAPSpeakers::vec_length(CartesianVector v1)
{
	return (sqrt(v1.x*v1.x + v1.y*v1.y + v1.z*v1.z));
}

float 
VBAPSpeakers::vec_prod(CartesianVector v1, CartesianVector v2)
{
	return (v1.x*v2.x + v1.y*v2.y + v1.z*v2.z);
}

float 
VBAPSpeakers::vol_p_side_lgth(int i, int j,int k, const vector<Speaker>& speakers)
{
	/* calculate volume of the parallelepiped defined by the loudspeaker
	   direction vectors and divide it with total length of the triangle sides. 
	   This is used when removing too narrow triangles. */
        
	float volper, lgth;
	CartesianVector xprod;

	cross_prod (speakers[i].coords(), speakers[j].coords(), &xprod);
	volper = fabsf (vec_prod(xprod, speakers[k].coords()));
	lgth = (fabsf (vec_angle(speakers[i].coords(), speakers[j].coords())) 
	        + fabsf (vec_angle(speakers[i].coords(), speakers[k].coords())) 
	        + fabsf (vec_angle(speakers[j].coords(), speakers[k].coords())));

	if (lgth > 0.00001) {
		return volper / lgth;
	} else {
		return 0.0;
	}
}

void 
VBAPSpeakers::cross_prod(CartesianVector v1,CartesianVector v2, CartesianVector *res) 
{
	float length;

	res->x = (v1.y * v2.z ) - (v1.z * v2.y);
	res->y = (v1.z * v2.x ) - (v1.x * v2.z);
	res->z = (v1.x * v2.y ) - (v1.y * v2.x);
        
	length = vec_length(*res);
	res->x /= length;
	res->y /= length;
	res->z /= length;
}

int 
VBAPSpeakers::lines_intersect (int i, int j, int k, int l)
{
	/* checks if two lines intersect on 3D sphere 
	   see theory in paper Pulkki, V. Lokki, T. "Creating Auditory Displays
	   with Multiple Loudspeakers Using VBAP: A Case Study with
	   DIVA Project" in International Conference on 
	   Auditory Displays -98. E-mail Ville.Pulkki@hut.fi
	   if you want to have that paper.
	*/

	CartesianVector v1;
	CartesianVector v2;
	CartesianVector v3, neg_v3;
	float dist_ij,dist_kl,dist_iv3,dist_jv3,dist_inv3,dist_jnv3;
	float dist_kv3,dist_lv3,dist_knv3,dist_lnv3;
        
	cross_prod(_speakers[i].coords(),_speakers[j].coords(),&v1);
	cross_prod(_speakers[k].coords(),_speakers[l].coords(),&v2);
	cross_prod(v1,v2,&v3);
        
	neg_v3.x= 0.0 - v3.x;
	neg_v3.y= 0.0 - v3.y;
	neg_v3.z= 0.0 - v3.z;

	dist_ij = (vec_angle(_speakers[i].coords(),_speakers[j].coords()));
	dist_kl = (vec_angle(_speakers[k].coords(),_speakers[l].coords()));
	dist_iv3 = (vec_angle(_speakers[i].coords(),v3));
	dist_jv3 = (vec_angle(v3,_speakers[j].coords()));
	dist_inv3 = (vec_angle(_speakers[i].coords(),neg_v3));
	dist_jnv3 = (vec_angle(neg_v3,_speakers[j].coords()));
	dist_kv3 = (vec_angle(_speakers[k].coords(),v3));
	dist_lv3 = (vec_angle(v3,_speakers[l].coords()));
	dist_knv3 = (vec_angle(_speakers[k].coords(),neg_v3));
	dist_lnv3 = (vec_angle(neg_v3,_speakers[l].coords()));

	/* if one of loudspeakers is close to crossing point, don't do anything*/


	if(fabsf(dist_iv3) <= 0.01 || fabsf(dist_jv3) <= 0.01 || 
	   fabsf(dist_kv3) <= 0.01 || fabsf(dist_lv3) <= 0.01 ||
	   fabsf(dist_inv3) <= 0.01 || fabsf(dist_jnv3) <= 0.01 || 
	   fabsf(dist_knv3) <= 0.01 || fabsf(dist_lnv3) <= 0.01 ) {
		return(0);
	}

	if (((fabsf(dist_ij - (dist_iv3 + dist_jv3)) <= 0.01 ) &&
	     (fabsf(dist_kl - (dist_kv3 + dist_lv3))  <= 0.01)) ||
	    ((fabsf(dist_ij - (dist_inv3 + dist_jnv3)) <= 0.01)  &&
	     (fabsf(dist_kl - (dist_knv3 + dist_lnv3)) <= 0.01 ))) {
		return (1);
	} else {
		return (0);
	}
}

void  
VBAPSpeakers::calculate_3x3_matrixes(struct ls_triplet_chain *ls_triplets)
{  
	/* Calculates the inverse matrices for 3D */
	float invdet;
	const CartesianVector* lp1;
	const CartesianVector* lp2;
	const CartesianVector* lp3;
	float *invmx;
	struct ls_triplet_chain *tr_ptr = ls_triplets;
	int triplet_count = 0;
	int triplet;

	assert (tr_ptr);
        
	/* counting triplet amount */

	while (tr_ptr != 0) {
		triplet_count++;
		tr_ptr = tr_ptr->next;
	}

	cerr << "@@@ triplets generate " << triplet_count << " of speaker tuples\n";

	triplet = 0;

	_matrices.clear ();
	_speaker_tuples.clear ();

	for (int n = 0; n < triplet_count; ++n) {
		_matrices.push_back (threeDmatrix());
		_speaker_tuples.push_back (tmatrix());
	}

	while (tr_ptr != 0) {
		lp1 =  &(_speakers[tr_ptr->ls_nos[0]].coords());
		lp2 =  &(_speakers[tr_ptr->ls_nos[1]].coords());
		lp3 =  &(_speakers[tr_ptr->ls_nos[2]].coords());
                
		/* matrix inversion */
		invmx = tr_ptr->inv_mx;
		invdet = 1.0 / (  lp1->x * ((lp2->y * lp3->z) - (lp2->z * lp3->y))
		                  - lp1->y * ((lp2->x * lp3->z) - (lp2->z * lp3->x))
		                  + lp1->z * ((lp2->x * lp3->y) - (lp2->y * lp3->x)));
                
		invmx[0] = ((lp2->y * lp3->z) - (lp2->z * lp3->y)) * invdet;
		invmx[3] = ((lp1->y * lp3->z) - (lp1->z * lp3->y)) * -invdet;
		invmx[6] = ((lp1->y * lp2->z) - (lp1->z * lp2->y)) * invdet;
		invmx[1] = ((lp2->x * lp3->z) - (lp2->z * lp3->x)) * -invdet;
		invmx[4] = ((lp1->x * lp3->z) - (lp1->z * lp3->x)) * invdet;
		invmx[7] = ((lp1->x * lp2->z) - (lp1->z * lp2->x)) * -invdet;
		invmx[2] = ((lp2->x * lp3->y) - (lp2->y * lp3->x)) * invdet;
		invmx[5] = ((lp1->x * lp3->y) - (lp1->y * lp3->x)) * -invdet;
		invmx[8] = ((lp1->x * lp2->y) - (lp1->y * lp2->x)) * invdet;
                
		/* copy the matrix */

		_matrices[triplet][0] = invmx[0];
		_matrices[triplet][1] = invmx[1];
		_matrices[triplet][2] = invmx[2];
		_matrices[triplet][3] = invmx[3];
		_matrices[triplet][4] = invmx[4];
		_matrices[triplet][5] = invmx[5];
		_matrices[triplet][6] = invmx[6];
		_matrices[triplet][7] = invmx[7];
		_matrices[triplet][8] = invmx[8];

		_speaker_tuples[triplet][0] = tr_ptr->ls_nos[0];
		_speaker_tuples[triplet][1] = tr_ptr->ls_nos[1];
		_speaker_tuples[triplet][2] = tr_ptr->ls_nos[2];

		cerr << "Triplet[" << triplet << "] = " 
		     << tr_ptr->ls_nos[0] << " + " 
		     << tr_ptr->ls_nos[1] << " + " 
		     << tr_ptr->ls_nos[2] << endl;

		triplet++;

		tr_ptr = tr_ptr->next;
	}
}

void 
VBAPSpeakers::choose_speaker_pairs (){

	/* selects the loudspeaker pairs, calculates the inversion
	   matrices and stores the data to a global array
	*/
	const int n_speakers = _speakers.size();

	if (n_speakers < 1) {
		return;
	}

	IntVector      sorted_speakers(n_speakers);
	BoolVector     exists(n_speakers);
	DoubleVector2D inverse_matrix(n_speakers, DoubleVector(4)); 
	const double   AZIMUTH_DELTA_THRESHOLD_DEGREES = (180.0/M_PI) * (M_PI - 0.175);
	int expected_pairs = 0;
	int pair;
	int speaker;

	for (speaker = 0; speaker < n_speakers; ++speaker) {
		exists[speaker] = false;
	}

	/* sort loudspeakers according their aximuth angle */
	sort_2D_lss (&sorted_speakers[0]);
        
	/* adjacent loudspeakers are the loudspeaker pairs to be used.*/
	for (speaker = 0; speaker < n_speakers-1; speaker++) {

		if ((_speakers[sorted_speakers[speaker+1]].angles().azi - 
		     _speakers[sorted_speakers[speaker]].angles().azi) <= AZIMUTH_DELTA_THRESHOLD_DEGREES) {
			if (calc_2D_inv_tmatrix( _speakers[sorted_speakers[speaker]].angles().azi, 
			                         _speakers[sorted_speakers[speaker+1]].angles().azi, 
			                         &inverse_matrix[speaker][0]) != 0){
				exists[speaker] = true;
				expected_pairs++;
			}
		}
	}
        
	if (((6.283 - _speakers[sorted_speakers[n_speakers-1]].angles().azi) 
	     +_speakers[sorted_speakers[0]].angles().azi) <= AZIMUTH_DELTA_THRESHOLD_DEGREES) {
		if (calc_2D_inv_tmatrix(_speakers[sorted_speakers[n_speakers-1]].angles().azi, 
		                        _speakers[sorted_speakers[0]].angles().azi, 
		                        &inverse_matrix[n_speakers-1][0]) != 0) { 
			exists[n_speakers-1] = true;
			expected_pairs++;
		} 
	}

	pair = 0;

	_matrices.clear ();
	_speaker_tuples.clear ();

	for (int n = 0; n < expected_pairs; ++n) {
		_matrices.push_back (twoDmatrix());
		_speaker_tuples.push_back (tmatrix());
	}

	for (speaker = 0; speaker < n_speakers - 1; speaker++) {
		if (exists[speaker]) {
			_matrices[pair][0] = inverse_matrix[speaker][0];
			_matrices[pair][1] = inverse_matrix[speaker][1];
			_matrices[pair][2] = inverse_matrix[speaker][2];
			_matrices[pair][3] = inverse_matrix[speaker][3];

			_speaker_tuples[pair][0] = sorted_speakers[speaker];
			_speaker_tuples[pair][1] = sorted_speakers[speaker+1];

			pair++;
		}
	}
        
	if (exists[n_speakers-1]) {
		_matrices[pair][0] = inverse_matrix[speaker][0];
		_matrices[pair][1] = inverse_matrix[speaker][1];
		_matrices[pair][2] = inverse_matrix[speaker][2];
		_matrices[pair][3] = inverse_matrix[speaker][3];

		_speaker_tuples[pair][0] = sorted_speakers[n_speakers-1];
		_speaker_tuples[pair][1] = sorted_speakers[0];
	}
}

void 
VBAPSpeakers::sort_2D_lss (int* sorted_speakers)
{
	vector<Speaker> tmp = _speakers;
	vector<Speaker>::iterator s;
	azimuth_sorter sorter;
	int n;

	sort (tmp.begin(), tmp.end(), sorter);

	for (n = 0, s = tmp.begin(); s != tmp.end(); ++s, ++n) {
		sorted_speakers[n] = (*s).id;
	}
}

int 
VBAPSpeakers::calc_2D_inv_tmatrix (double azi1, double azi2, double* inverse_matrix)
{
	double x1,x2,x3,x4;
	double det;

	x1 = cos (azi1 * (M_PI/180.0));
        x2 = sin (azi1 * (M_PI/180.0));
	x3 = cos (azi2 * (M_PI/180.0));
	x4 = sin (azi2 * (M_PI/180.0));
	det = (x1 * x4) - ( x3 * x2 );

	if (fabs(det) <= 0.001) {
                
		inverse_matrix[0] = 0.0;
		inverse_matrix[1] = 0.0;
		inverse_matrix[2] = 0.0;
		inverse_matrix[3] = 0.0;

		return 0;

	} else {

		inverse_matrix[0] = x4 / det;
		inverse_matrix[1] = -x3 / det;
		inverse_matrix[2] = -x2 / det;
		inverse_matrix[3] = x1 / det;

		return 1;
	}
}


