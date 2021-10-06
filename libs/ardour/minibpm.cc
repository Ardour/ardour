/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    MiniBPM
    A fixed-tempo BPM estimator for music audio
    Copyright 2012 Particular Programs Ltd.

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.

    Alternatively, if you have a valid commercial licence for MiniBPM
    obtained by agreement with the copyright holders, you may
    redistribute and/or modify it under the terms described in that
    licence.

    If you wish to distribute code using MiniBPM under terms other
    than those of the GNU General Public License, you must obtain a
    valid commercial licence before doing so.
*/

/*
 * Method:
 *
 * - Take the audio as a sequence of overlapping time-domain
 *   frames. The frame size is chosen so that, following a Fourier
 *   transform, the frequency range up to about an octave above
 *   middle-C would take about half a dozen bins. This is a relatively
 *   short frame giving quite good time resolution.
 *
 * - For each frame, extract the low-frequency range into the
 *   frequency domain (up to a cutoff around 400-500 Hz) using a small
 *   filterbank. Also extract a single bin from a high frequency range
 *   (around 9K) for broadband noise, and calculate the overall RMS of
 *   the frame.  (The low-frequency feature is the main contributor to
 *   tempo estimation, the other two are used as fallbacks if there is
 *   not enough low-frequency information.)  Accumulate sequences of
 *   framewise spectral difference sums for the frequency domain
 *   information, and a sequence of the RMS values, across the
 *   duration of the audio.
 *
 * - When all audio has been processed, calculate an autocorrelation
 *   of each of the three features normalised to unity maximum, and
 *   calculate a weighted sum of the autocorrelations (discarding any
 *   phase difference between the three signals) with the
 *   low-frequency feature given the most weight.
 *
 * - Drag a comb filter across the subset of the summed
 *   autocorrelation sequence that corresponds to the plausible tempo
 *   range. Allocate to each lag a weighted sum of its value and those
 *   of elements around beats-per-bar multiples of its lag.
 *
 * - Apply a simplistic perceptual weighting filter to prefer tempi
 *   around 120-130bpm.
 *
 * - Find the peak of the resulting filtered autocorrelation and
 *   return its corresponding tempo.
 */

#include "ardour/minibpm.h"

#include <vector>
#include <map>
#include <utility>
#include <cmath>

#ifdef __MSVC__
#define R__ __restrict
#else
#ifdef __GNUC__
#define R__ __restrict__
#else
#define R__
#endif
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using std::vector;

#include <iostream>

namespace breakfastquay {

class Autocorrelation
{
public:
    Autocorrelation(int n, int m) : m_n(n), m_m(m) { }

    template <typename T>
    void acf(const T *R__ in, T *R__ out) const {
	for (int i = 0; i < m_m; ++i) {
	    out[i] = 0.0;
	    for (int j = i; j < m_n; ++j) {
		out[i] += in[j] * in[j - i];
	    }
	}
    }

    template <typename T>
    void acfUnityNormalised(const T *R__ in, T *R__ out) const {

	acf(in, out);

        double max = 0.0;
	for (int i = 0; i < m_m; ++i) {
	    out[i] /= m_n - i;
            if (out[i] > max) max = out[i];
	}
        if (max > 0.0) {
            for (int i = 0; i < m_m; ++i) {
                out[i] /= max;
            }
        }
    }

    static int bpmToLag(double bpm, double hopsPerSec) {
	return int((60.0 / bpm) * hopsPerSec + 0.5);
    }
    static double lagToBpm(double lag, double hopsPerSec) {
	return (60.0 * hopsPerSec) / lag;
    }

private:
    int m_n;
    int m_m;
};

class FourierFilterbank
{
public:
    FourierFilterbank(int n, double fs, double minFreq, double maxFreq,
		      bool windowed) :
	m_n(n), m_fs(fs), m_fmin(minFreq), m_fmax(maxFreq),
	m_windowed(windowed)
    {
	m_binmin = int(floor(n * m_fmin) / fs);
	m_binmax = int(ceil(n * m_fmax) / fs);
	m_bins = m_binmax - m_binmin + 1;
	initFilters();
    }

    ~FourierFilterbank() {
        for (int i = 0; i < m_bins; ++i) {
            delete[] m_sin[i];
            delete[] m_cos[i];
        }
        delete[] m_sin;
        delete[] m_cos;
    }

    int getOutputSize() const {
	return m_bins;
    }

    void forwardMagnitude(const double *R__ realIn, double *R__ magOut) const {
	for (int i = 0; i < m_bins; ++i) {
	    const double *R__ sin = m_sin[i];
	    const double *R__ cos = m_cos[i];
	    double real = 0.0, imag = 0.0;
	    for (int j = 0; j < m_n; ++j) real += realIn[j] * cos[j];
	    for (int j = 0; j < m_n; ++j) imag += realIn[j] * sin[j];
	    magOut[i] = sqrt(real*real + imag*imag);
	}
    }

private:
    int m_n;
    double m_fs;
    double m_fmin;
    double m_fmax;
    bool m_windowed;
    int m_binmin;
    int m_binmax;
    int m_bins;
    
    double **m_sin;
    double **m_cos;

    void initFilters() {
        m_sin = new double*[m_bins];
        m_cos = new double*[m_bins];
	double twopi = M_PI * 2.0;
        double win = 1.0;
	for (int i = 0; i < m_bins; ++i) {
            m_sin[i] = new double[m_n];
            m_cos[i] = new double[m_n];
	    int bin = i + m_binmin;
	    double delta = (twopi * bin) / m_n;
	    for (int j = 0; j < m_n; ++j) {
		double angle = j * delta;
                if (m_windowed) win = 0.5 - 0.5 * cos(twopi * j / m_n);
		m_sin[i][j] = sin(angle) * win;
		m_cos[i][j] = cos(angle) * win;
	    }
	}
    }
};

class ACFCombFilter
{
public:
    ACFCombFilter(int beatsPerBar, int minlag, int maxlag, double hopsPerSec) :
        m_beatsPerBar(beatsPerBar),
        m_min(minlag), m_max(maxlag),
        m_hopsPerSec(hopsPerSec) { }
    ~ACFCombFilter() { }

    int getFilteredLength() const {
        return m_max - m_min + 1;
    }

    static void getContributingRange(int lag, int multiple,
                                     int &base, int &count) {
        if (multiple == 1) {
            base = lag;
            count = 1;
        } else {
            // 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15 16 17 18 ...
            // 0  1  2  4  4  4  8  8  8  8  8  8 16 16 16 16 16 16 16 ...
            base = (lag * multiple) - (multiple / 4);
            count = (multiple / 4) + (multiple / 2);
        }
    }

    void filter(const double *acf, int acfLength, double *filtered) {
        
	int flen = getFilteredLength();
    
        for (int i = 0; i < flen; ++i) {
            
            filtered[i] = 0.0;

            int lag = m_min + i;
            int multiple = 1;
            int n = 0;

            while (1) {

                int base, count;
                getContributingRange(lag, multiple, base, count);
                if (base + count > acfLength) break;

                double peak = 0.0;
                for (int j = base; j < base + count; ++j) {
                    if (j == base || acf[j] > peak) {
                        peak = acf[j];
                    }
                }
                filtered[i] += peak;
                ++n;

                if (multiple == 1) multiple = m_beatsPerBar;
                else multiple = multiple * 2;
            }

            filtered[i] /= n;
        }
    }

    double refine(int lag, const double *acf, int acfLength) {
        
        int multiple = 1;
        double interpolated = lag;

        double total = 0.0;
        int n = 0;

        while (1) {
            
            int base, count;
            getContributingRange(lag, multiple, base, count);

            if (base + count > acfLength) break;

            double peak = 0.0;
            int peakidx = 0;
            for (int j = base; j < base + count; ++j) {
                if (acf[j] > peak) {
                    peak = acf[j];
                    peakidx = j;
                }
            }
            
            if (peak > 0.0) {
                double scaled = double(peakidx) / multiple;
                total += scaled;
                ++n;
            }
            
            if (multiple == 1) multiple = m_beatsPerBar;
            else multiple = multiple * 2;
        }

        if (n > 0) {
            interpolated = total / n;
        }
    
        double bpm = Autocorrelation::lagToBpm(interpolated, m_hopsPerSec);
        return bpm;
    }

private:
    int m_beatsPerBar;
    int m_min;
    int m_max;
    double m_hopsPerSec;
};

class MiniBPM::D
{
public:
    double m_minbpm;
    double m_maxbpm;
    int m_beatsPerBar;

    template <typename S, typename T>
    void copy(T *R__ t, const S *R__ s, const int n) {
	for (int i = 0; i < n; ++i) t[i] = s[i];
    }
    template <typename T>
    void zero(T *R__ t, const int n) {
	for (int i = 0; i < n; ++i) t[i] = T(0);
    }
    template <typename T>
    void unityNormalise(T *R__ t, const int n) {
        double max = 0.0, min = 0.0;
        for (int i = 0; i < n; ++i) {
            if (i == 0 || t[i] > max) max = t[i];
            if (i == 0 || t[i] < min) min = t[i];
        }
        if (max > min) {
            for (int i = 0; i < n; ++i) {
                t[i] = (t[i] - min) / (max - min);
            }
        }
    }

    D(float sampleRate) :
	m_minbpm(55),
	m_maxbpm(190),
	m_beatsPerBar(4),
	m_inputSampleRate(sampleRate),
	m_lfmin(0),
	m_lfmax(550),
	m_hfmin(9000),
	m_hfmax(9001),
	m_input(0),
	m_partial(0),
	m_partialFill(0),
	m_frame(0),
	m_lfprev(0),
	m_hfprev(0)
    {
	int lfbinmax = 6;
	m_blockSize = (m_inputSampleRate * lfbinmax) / m_lfmax;
	m_stepSize = m_blockSize / 2;

	m_lf = new FourierFilterbank(m_blockSize, m_inputSampleRate, 
				     m_lfmin, m_lfmax, true);

	m_hf = new FourierFilterbank(m_blockSize, m_inputSampleRate, 
				     m_hfmin, m_hfmax, true);

	int lfsize = m_lf->getOutputSize();
	int hfsize = m_hf->getOutputSize();

	m_lfprev = new double[lfsize];
	for (int i = 0; i < lfsize; ++i) m_lfprev[i] = 0.0;

	m_hfprev = new double[hfsize];
	for (int i = 0; i < hfsize; ++i) m_hfprev[i] = 0.0;

	m_input = new double[m_blockSize];
	m_partial = new double[m_stepSize];

        int frameSize = std::max(lfsize, hfsize);
	m_frame = new double[frameSize];

        zero(m_input, m_blockSize);
        zero(m_partial, m_stepSize);
        zero(m_frame, frameSize);
    }
	
    ~D()
    {
	delete m_lf;
	delete m_hf;
	delete[] m_lfprev;
	delete[] m_hfprev;
	delete[] m_input;
	delete[] m_partial;
	delete[] m_frame;
    }

    double
    specdiff(const double *a, const double *b, int n)
    {
	double tot = 0.0;
	for (int i = 0; i < n; ++i) {
	    tot += sqrt(fabs(a[i]*a[i] - b[i]*b[i]));
	}
	return tot;
    }

    double estimateTempoOfSamples(const float *samples, int nsamples)
    {
	int i = 0;
	while (i + m_blockSize < nsamples) {
	    copy(m_input, samples + i, m_blockSize);
	    processInputBlock();
	    i += m_stepSize;
	}
	return finish();
    }

    void process(const float *samples, int nsamples)
    {
	int n = 0;
	while (n < nsamples) {
	    int hole = m_blockSize - m_stepSize;
	    int remaining = nsamples - n;
	    if (m_partialFill + remaining < m_stepSize) {
		copy(m_partial + m_partialFill, samples + n, remaining);
		m_partialFill += remaining;
		break;
	    }
	    copy(m_input + hole, m_partial, m_partialFill);
	    int toConsume = m_stepSize - m_partialFill;
	    copy(m_input + hole + m_partialFill, samples + n, toConsume);
	    n += toConsume;
	    m_partialFill = 0;
	    processInputBlock();
	    copy(m_input, m_input + m_stepSize, hole);
	}
    }

    double estimateTempo()
    {
	if (m_partialFill > 0) {
	    int hole = m_blockSize - m_stepSize;
	    copy(m_input + hole, m_partial, m_partialFill);
	    zero(m_input + hole + m_partialFill, m_stepSize - m_partialFill);
	    m_partialFill = 0;
	    processInputBlock();
	}
	return finish();
    }

    std::vector<double> getTempoCandidates() const
    {
        return m_candidates;
    }

    void reset()
    {
	m_lfdf.clear();
	m_hfdf.clear();
	m_rms.clear();
	m_partialFill = 0;
    }

    void processInputBlock()
    {
	double rms = 0.0;

	for (int i = 0; i < m_blockSize; ++i) {
	    rms += m_input[i] * m_input[i];
	}

	rms = sqrt(rms / m_blockSize);
	m_rms.push_back(rms);

	int lfsize = m_lf->getOutputSize();
	int hfsize = m_hf->getOutputSize();

	m_lf->forwardMagnitude(m_input, m_frame);
	m_lfdf.push_back(specdiff(m_frame, m_lfprev, lfsize));
	copy(m_lfprev, m_frame, lfsize);
	
	m_hf->forwardMagnitude(m_input, m_frame);
	m_hfdf.push_back(specdiff(m_frame, m_hfprev, hfsize));
	copy(m_hfprev, m_frame, hfsize);
    }

    double finish()
    {
        m_candidates.clear();

	double hopsPerSec = m_inputSampleRate / m_stepSize;
	int dfLength = m_lfdf.size();

	// We have no use for any lag beyond 4 bars at minimum bpm
	double barPM = m_minbpm / (4 * m_beatsPerBar);
	int acfLength = Autocorrelation::bpmToLag(barPM, hopsPerSec);
        while (acfLength > dfLength) acfLength /= 2;

	Autocorrelation acfcalc(dfLength, acfLength);

	double *acf = new double[acfLength];
	double *temp = new double[acfLength];

	zero(acf, acfLength);

	acfcalc.acfUnityNormalised(m_lfdf.data(), temp);
	for (int i = 0; i < acfLength; ++i) acf[i] += temp[i];

	acfcalc.acfUnityNormalised(m_hfdf.data(), temp);
	for (int i = 0; i < acfLength; ++i) acf[i] += temp[i] * 0.5;

	acfcalc.acfUnityNormalised(m_rms.data(), temp);
	for (int i = 0; i < acfLength; ++i) acf[i] += temp[i] * 0.1;

	int minlag = Autocorrelation::bpmToLag(m_maxbpm, hopsPerSec);
	int maxlag = Autocorrelation::bpmToLag(m_minbpm, hopsPerSec);

	if (acfLength < maxlag) {
	    // Not enough data
	    return 0.0;
	}

        ACFCombFilter filter(m_beatsPerBar, minlag, maxlag, hopsPerSec);
        int cflen = filter.getFilteredLength();
	double *cf = new double[cflen];
        filter.filter(acf, acfLength, cf);
        unityNormalise(cf, cflen);

	for (int i = 0; i < cflen; ++i) {
	    // perceptual weighting: prefer middling values
	    double bpm = Autocorrelation::lagToBpm(minlag + i, hopsPerSec);
            double weight;
            double centre = 130.0;
            if (bpm < centre) {
                weight = 1.0 - pow(fabs(centre - bpm) / 100.0, 2.4);
            } else {
                weight = 1.0 - pow(fabs(centre - bpm) / 80.0, 2.4);
            }                
	    if (weight < 0.0) weight = 0.0;
	    cf[i] *= weight;
	}

        std::multimap<double, int> candidateMap;
	for (int i = 1; i + 1 < cflen; ++i) {
            if (cf[i] > cf[i-1] && cf[i] > cf[i+1]) {
                candidateMap.insert(std::pair<double, int>(cf[i], i));
            }
	}

        if (candidateMap.empty()) {
            return 0.0;
        }

        std::multimap<double, int>::const_iterator ci(candidateMap.end());
        while (ci != candidateMap.begin()) {
            --ci;
            int lag = ci->second + minlag;
            double bpm = filter.refine(lag, acf, acfLength);
            m_candidates.push_back(bpm);
        }

	delete[] cf;
	delete[] acf;
	delete[] temp;

	return m_candidates[0];
    }
	

private:
    float m_inputSampleRate;
    int m_blockSize;
    int m_stepSize;
    int m_lfmin;
    int m_lfmax;
    int m_hfmin;
    int m_hfmax;

    std::vector<double> m_lfdf;
    std::vector<double> m_hfdf;
    std::vector<double> m_rms;

    std::vector<double> m_candidates;

    FourierFilterbank *m_lf;
    FourierFilterbank *m_hf;
    
    double *m_input;
    double *m_partial;
    int m_partialFill;

    double *m_frame;
    double *m_lfprev;
    double *m_hfprev;
};

MiniBPM::MiniBPM(float sampleRate) :
    m_d(new D(sampleRate))
{
}

MiniBPM::~MiniBPM()
{
    delete m_d;
}

void
MiniBPM::setBPMRange(double min, double max)
{
    m_d->m_minbpm = min;
    m_d->m_maxbpm = max;
}

void
MiniBPM::getBPMRange(double &min, double &max) const
{
    min = m_d->m_minbpm;
    max = m_d->m_maxbpm;
}

void
MiniBPM::setBeatsPerBar(int bpb)
{
    m_d->m_beatsPerBar = bpb;
}

int
MiniBPM::getBeatsPerBar() const
{
    return m_d->m_beatsPerBar;
}

double
MiniBPM::estimateTempoOfSamples(const float *samples, int nsamples)
{
    return m_d->estimateTempoOfSamples(samples, nsamples);
}

void
MiniBPM::process(const float *samples, int nsamples)
{
    m_d->process(samples, nsamples);
}

double
MiniBPM::estimateTempo()
{
    return m_d->estimateTempo();
}

std::vector<double>
MiniBPM::getTempoCandidates() const
{
    return m_d->getTempoCandidates();
}

void
MiniBPM::reset()
{
    m_d->reset();
}

}


