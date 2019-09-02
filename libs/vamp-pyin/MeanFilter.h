/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    pYIN - A fundamental frequency estimator for monophonic audio
    Centre for Digital Music, Queen Mary, University of London.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#ifndef _MEAN_FILTER_H_
#define _MEAN_FILTER_H_

class MeanFilter
{
public:
    /**
     * Construct a non-causal mean filter with filter length flen,
     * that replaces each sample N with the mean of samples
     * [N-floor(F/2) .. N+floor(F/2)] where F is the filter length.
     * Only odd F are supported.
     */
    MeanFilter(int flen) : m_flen(flen) { }
    ~MeanFilter() { }

    /**
     * Filter the n samples in "in" and place the results in "out"
     */
    void filter(const double *in, double *out, const int n) {
	filterSubsequence(in, out, n, n, 0);
    }

    /**
     * Filter the n samples starting at the given offset in the
     * m-element array "in" and place the results in the n-element
     * array "out"
     */
    void filterSubsequence(const double *in, double *out,
			   const int m, const int n,
			   const int offset) {
	int half = m_flen/2;
	for (int i = 0; i < n; ++i) {
	    double v = 0;
	    int n = 0;
	    for (int j = -half; j <= half; ++j) {
		int ix = i + j + offset;
		if (ix >= 0 && ix < m) {
		    v += in[ix];
		    ++n;
		}
	    }
	    out[i] = v / n;
	}
    }

private:
    int m_flen;
};

#endif
