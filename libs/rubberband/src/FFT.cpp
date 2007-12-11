/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band
    An audio time-stretching and pitch-shifting library.
    Copyright 2007 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "FFT.h"
#include "Thread.h"


#include <fftw3.h>

#include <cmath>
#include <iostream>
#include <map>
#include <cstdio>
#include <vector>

namespace RubberBand {

class FFTImpl
{
public:
    virtual ~FFTImpl() { }

    virtual void initFloat() = 0;
    virtual void initDouble() = 0;

    virtual void forward(double *realIn, double *realOut, double *imagOut) = 0;
    virtual void forwardPolar(double *realIn, double *magOut, double *phaseOut) = 0;
    virtual void forwardMagnitude(double *realIn, double *magOut) = 0;

    virtual void forward(float *realIn, float *realOut, float *imagOut) = 0;
    virtual void forwardPolar(float *realIn, float *magOut, float *phaseOut) = 0;
    virtual void forwardMagnitude(float *realIn, float *magOut) = 0;

    virtual void inverse(double *realIn, double *imagIn, double *realOut) = 0;
    virtual void inversePolar(double *magIn, double *phaseIn, double *realOut) = 0;

    virtual void inverse(float *realIn, float *imagIn, float *realOut) = 0;
    virtual void inversePolar(float *magIn, float *phaseIn, float *realOut) = 0;

    virtual float *getFloatTimeBuffer() = 0;
    virtual double *getDoubleTimeBuffer() = 0;
};    




// Define FFTW_DOUBLE_ONLY to make all uses of FFTW functions be
// double-precision (so "float" FFTs are calculated by casting to
// doubles and using the double-precision FFTW function).
//
// Define FFTW_FLOAT_ONLY to make all uses of FFTW functions be
// single-precision (so "double" FFTs are calculated by casting to
// floats and using the single-precision FFTW function).
//
// Neither of these flags is terribly desirable -- FFTW_FLOAT_ONLY
// obviously loses you precision, and neither is handled in the most
// efficient way so any performance improvement will be small at best.
// The only real reason to define either flag would be to avoid
// linking against both fftw3 and fftw3f libraries.

//#define FFTW_DOUBLE_ONLY 1
//#define FFTW_FLOAT_ONLY 1

#ifdef FFTW_DOUBLE_ONLY
#ifdef FFTW_FLOAT_ONLY
#error Building for FFTW-DOUBLE BOTH
// Can't meaningfully define both
#undef FFTW_DOUBLE_ONLY
#undef FFTW_FLOAT_ONLY
#else /* !FFTW_FLOAT_ONLY */
#define fftwf_complex fftw_complex
#define fftwf_plan fftw_plan
#define fftwf_plan_dft_r2c_1d fftw_plan_dft_r2c_1d
#define fftwf_plan_dft_c2r_1d fftw_plan_dft_c2r_1d
#define fftwf_destroy_plan fftw_destroy_plan
#define fftwf_malloc fftw_malloc
#define fftwf_free fftw_free
#define fftwf_execute fftw_execute
#define atan2f atan2
#define sqrtf sqrt
#define cosf cos
#define sinf sin
#endif /* !FFTW_FLOAT_ONLY */
#endif

#ifdef FFTW_FLOAT_ONLY
#define fftw_complex fftwf_complex
#define fftw_plan fftwf_plan
#define fftw_plan_dft_r2c_1d fftwf_plan_dft_r2c_1d
#define fftw_plan_dft_c2r_1d fftwf_plan_dft_c2r_1d
#define fftw_destroy_plan fftwf_destroy_plan
#define fftw_malloc fftwf_malloc
#define fftw_free fftwf_free
#define fftw_execute fftwf_execute
#define atan2 atan2f
#define sqrt sqrtf
#define cos cosf
#define sif sinf
#endif /* FFTW_FLOAT_ONLY */

class D_FFTW : public FFTImpl
{
public:
    D_FFTW(unsigned int size) : m_fplanf(0)
#ifdef FFTW_DOUBLE_ONLY
                              , m_frb(0)
#endif
                              , m_dplanf(0)
#ifdef FFTW_FLOAT_ONLY
                              , m_drb(0)
#endif
                              , m_size(size)
    {
    }

    ~D_FFTW() {
        if (m_fplanf) {
            bool save = false;
            m_extantMutex.lock();
            if (m_extantf > 0 && --m_extantf == 0) save = true;
            m_extantMutex.unlock();
            if (save) saveWisdom('f');
            fftwf_destroy_plan(m_fplanf);
            fftwf_destroy_plan(m_fplani);
            fftwf_free(m_fbuf);
            fftwf_free(m_fpacked);
#ifdef FFTW_DOUBLE_ONLY
            if (m_frb) fftw_free(m_frb);
#endif
        }
        if (m_dplanf) {
            bool save = false;
            m_extantMutex.lock();
            if (m_extantd > 0 && --m_extantd == 0) save = true;
            m_extantMutex.unlock();
            if (save) saveWisdom('d');
            fftw_destroy_plan(m_dplanf);
            fftw_destroy_plan(m_dplani);
            fftw_free(m_dbuf);
            fftw_free(m_dpacked);
#ifdef FFTW_FLOAT_ONLY
            if (m_drb) fftwf_free(m_drb);
#endif
        }
    }

    void initFloat() {
        if (m_fplanf) return;
        bool load = false;
        m_extantMutex.lock();
        if (m_extantf++ == 0) load = true;
        m_extantMutex.unlock();
#ifdef FFTW_DOUBLE_ONLY
        if (load) loadWisdom('d');
        m_fbuf = (double *)fftw_malloc(m_size * sizeof(double));
#else
        if (load) loadWisdom('f');
        m_fbuf = (float *)fftwf_malloc(m_size * sizeof(float));
#endif
        m_fpacked = (fftwf_complex *)fftw_malloc
            ((m_size/2 + 1) * sizeof(fftwf_complex));
        m_fplanf = fftwf_plan_dft_r2c_1d
            (m_size, m_fbuf, m_fpacked, FFTW_MEASURE);
        m_fplani = fftwf_plan_dft_c2r_1d
            (m_size, m_fpacked, m_fbuf, FFTW_MEASURE);
    }

    void initDouble() {
        if (m_dplanf) return;
        bool load = false;
        m_extantMutex.lock();
        if (m_extantd++ == 0) load = true;
        m_extantMutex.unlock();
#ifdef FFTW_FLOAT_ONLY
        if (load) loadWisdom('f');
        m_dbuf = (float *)fftwf_malloc(m_size * sizeof(float));
#else
        if (load) loadWisdom('d');
        m_dbuf = (double *)fftw_malloc(m_size * sizeof(double));
#endif
        m_dpacked = (fftw_complex *)fftw_malloc
            ((m_size/2 + 1) * sizeof(fftw_complex));
        m_dplanf = fftw_plan_dft_r2c_1d
            (m_size, m_dbuf, m_dpacked, FFTW_MEASURE);
        m_dplani = fftw_plan_dft_c2r_1d
            (m_size, m_dpacked, m_dbuf, FFTW_MEASURE);
    }

    void loadWisdom(char type) { wisdom(false, type); }
    void saveWisdom(char type) { wisdom(true, type); }

    void wisdom(bool save, char type) {

#ifdef FFTW_DOUBLE_ONLY
        if (type == 'f') return;
#endif
#ifdef FFTW_FLOAT_ONLY
        if (type == 'd') return;
#endif

        const char *home = getenv("HOME");
        if (!home) return;

        char fn[256];
        snprintf(fn, 256, "%s/%s.%c", home, ".rubberband.wisdom", type);

        FILE *f = fopen(fn, save ? "wb" : "rb");
        if (!f) return;

        if (save) {
            switch (type) {
#ifdef FFTW_DOUBLE_ONLY
            case 'f': break;
#else
            case 'f': fftwf_export_wisdom_to_file(f); break;
#endif
#ifdef FFTW_FLOAT_ONLY
            case 'd': break;
#else
            case 'd': fftw_export_wisdom_to_file(f); break;
#endif
            default: break;
            }
        } else {
            switch (type) {
#ifdef FFTW_DOUBLE_ONLY
            case 'f': break;
#else
            case 'f': fftwf_import_wisdom_from_file(f); break;
#endif
#ifdef FFTW_FLOAT_ONLY
            case 'd': break;
#else
            case 'd': fftw_import_wisdom_from_file(f); break;
#endif
            default: break;
            }
        }

        fclose(f);
    }

    void packFloat(float *re, float *im) {
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            m_fpacked[i][0] = re[i];
            m_fpacked[i][1] = im[i];
        }
    }

    void packDouble(double *re, double *im) {
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            m_dpacked[i][0] = re[i];
            m_dpacked[i][1] = im[i];
        }
    }

    void unpackFloat(float *re, float *im) {
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            re[i] = m_fpacked[i][0];
            im[i] = m_fpacked[i][1];
        }
    }        

    void unpackDouble(double *re, double *im) {
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            re[i] = m_dpacked[i][0];
            im[i] = m_dpacked[i][1];
        }
    }        

    void forward(double *realIn, double *realOut, double *imagOut) {
        if (!m_dplanf) initDouble();
#ifndef FFTW_FLOAT_ONLY
        if (realIn != m_dbuf) 
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                m_dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        unpackDouble(realOut, imagOut);
    }

    void forwardPolar(double *realIn, double *magOut, double *phaseOut) {
        if (!m_dplanf) initDouble();
#ifndef FFTW_FLOAT_ONLY
        if (realIn != m_dbuf)
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                m_dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            magOut[i] = sqrt(m_dpacked[i][0] * m_dpacked[i][0] +
                             m_dpacked[i][1] * m_dpacked[i][1]);
        }
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            phaseOut[i] = atan2(m_dpacked[i][1], m_dpacked[i][0]);
        }
    }

    void forwardMagnitude(double *realIn, double *magOut) {
        if (!m_dplanf) initDouble();
#ifndef FFTW_FLOAT_ONLY
        if (realIn != m_dbuf)
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                m_dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            magOut[i] = sqrt(m_dpacked[i][0] * m_dpacked[i][0] +
                             m_dpacked[i][1] * m_dpacked[i][1]);
        }
    }

    void forward(float *realIn, float *realOut, float *imagOut) {
        if (!m_fplanf) initFloat();
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != m_fbuf)
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                m_fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        unpackFloat(realOut, imagOut);
    }

    void forwardPolar(float *realIn, float *magOut, float *phaseOut) {
        if (!m_fplanf) initFloat();
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != m_fbuf) 
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                m_fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            magOut[i] = sqrtf(m_fpacked[i][0] * m_fpacked[i][0] +
                              m_fpacked[i][1] * m_fpacked[i][1]);
        }
        for (unsigned int i = 0; i <= m_size/2; ++i) {
          phaseOut[i] = atan2f(m_fpacked[i][1], m_fpacked[i][0]) ;
        }
    }

    void forwardMagnitude(float *realIn, float *magOut) {
        if (!m_fplanf) initFloat();
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != m_fbuf)
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                m_fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            magOut[i] = sqrtf(m_fpacked[i][0] * m_fpacked[i][0] +
                              m_fpacked[i][1] * m_fpacked[i][1]);
        }
    }

    void inverse(double *realIn, double *imagIn, double *realOut) {
        if (!m_dplanf) initDouble();
        packDouble(realIn, imagIn);
        fftw_execute(m_dplani);
#ifndef FFTW_FLOAT_ONLY
        if (realOut != m_dbuf) 
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                realOut[i] = m_dbuf[i];
            }
    }

    void inversePolar(double *magIn, double *phaseIn, double *realOut) {
        if (!m_dplanf) initDouble();
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            m_dpacked[i][0] = magIn[i] * cos(phaseIn[i]);
            m_dpacked[i][1] = magIn[i] * sin(phaseIn[i]);
        }
        fftw_execute(m_dplani);
#ifndef FFTW_FLOAT_ONLY
        if (realOut != m_dbuf)
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                realOut[i] = m_dbuf[i];
            }
    }

    void inverse(float *realIn, float *imagIn, float *realOut) {
        if (!m_fplanf) initFloat();
        packFloat(realIn, imagIn);
        fftwf_execute(m_fplani);
#ifndef FFTW_DOUBLE_ONLY
        if (realOut != m_fbuf)
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                realOut[i] = m_fbuf[i];
            }
    }

    void inversePolar(float *magIn, float *phaseIn, float *realOut) {
        if (!m_fplanf) initFloat();
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            m_fpacked[i][0] = magIn[i] * cosf(phaseIn[i]);
            m_fpacked[i][1] = magIn[i] * sinf(phaseIn[i]);
        }
        fftwf_execute(m_fplani);
#ifndef FFTW_DOUBLE_ONLY
        if (realOut != m_fbuf)
#endif
            for (unsigned int i = 0; i < m_size; ++i) {
                realOut[i] = m_fbuf[i];
            }
    }

    float *getFloatTimeBuffer() {
        initFloat();
#ifdef FFTW_DOUBLE_ONLY
        if (!m_frb) m_frb = (float *)fftw_malloc(m_size * sizeof(float));
        return m_frb;
#else
        return m_fbuf;
#endif
    }

    double *getDoubleTimeBuffer() {
        initDouble();
#ifdef FFTW_FLOAT_ONLY
        if (!m_drb) m_drb = (double *)fftwf_malloc(m_size * sizeof(double));
        return m_drb;
#else
        return m_dbuf;
#endif
    }

private:
    fftwf_plan m_fplanf;
    fftwf_plan m_fplani;
#ifdef FFTW_DOUBLE_ONLY
    float *m_frb;
    double *m_fbuf;
#else
    float *m_fbuf;
#endif
    fftwf_complex *m_fpacked;
    fftw_plan m_dplanf;
    fftw_plan m_dplani;
#ifdef FFTW_FLOAT_ONLY
    float *m_dbuf;
    double *m_drb;
#else
    double *m_dbuf;
#endif
    fftw_complex *m_dpacked;
    unsigned int m_size;
    static unsigned int m_extantf;
    static unsigned int m_extantd;
    static Mutex m_extantMutex;
};

unsigned int
D_FFTW::m_extantf = 0;

unsigned int
D_FFTW::m_extantd = 0;

Mutex
D_FFTW::m_extantMutex;


class D_Cross : public FFTImpl
{
public:
    D_Cross(unsigned int size) : m_size(size), m_table(0), m_frb(0), m_drb(0) {
        
        m_a = new double[size];
        m_b = new double[size];
        m_c = new double[size];
        m_d = new double[size];

        m_table = new int[m_size];
    
        unsigned int bits;
        unsigned int i, j, k, m;

        for (i = 0; ; ++i) {
            if (m_size & (1 << i)) {
                bits = i;
                break;
            }
        }
        
        for (i = 0; i < m_size; ++i) {
            
            m = i;
            
            for (j = k = 0; j < bits; ++j) {
                k = (k << 1) | (m & 1);
                m >>= 1;
            }
            
            m_table[i] = k;
        }
    }

    ~D_Cross() {
        delete[] m_table;
        delete[] m_a;
        delete[] m_b;
        delete[] m_c;
        delete[] m_d;
        delete[] m_frb;
        delete[] m_drb;
    }

    void initFloat() { }
    void initDouble() { }

    void forward(double *realIn, double *realOut, double *imagOut) {
        basefft(false, realIn, 0, m_c, m_d);
        for (size_t i = 0; i <= m_size/2; ++i) realOut[i] = m_c[i];
        for (size_t i = 0; i <= m_size/2; ++i) imagOut[i] = m_d[i];
    }

    void forwardPolar(double *realIn, double *magOut, double *phaseOut) {
        basefft(false, realIn, 0, m_c, m_d);
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            magOut[i] = sqrt(m_c[i] * m_c[i] + m_d[i] * m_d[i]);
            phaseOut[i] = atan2(m_d[i], m_c[i]) ;
        }
    }

    void forwardMagnitude(double *realIn, double *magOut) {
        basefft(false, realIn, 0, m_c, m_d);
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            magOut[i] = sqrt(m_c[i] * m_c[i] + m_d[i] * m_d[i]);
        }
    }

    void forward(float *realIn, float *realOut, float *imagOut) {
        for (size_t i = 0; i < m_size; ++i) m_a[i] = realIn[i];
        basefft(false, m_a, 0, m_c, m_d);
        for (size_t i = 0; i <= m_size/2; ++i) realOut[i] = m_c[i];
        for (size_t i = 0; i <= m_size/2; ++i) imagOut[i] = m_d[i];
    }

    void forwardPolar(float *realIn, float *magOut, float *phaseOut) {
        for (size_t i = 0; i < m_size; ++i) m_a[i] = realIn[i];
        basefft(false, m_a, 0, m_c, m_d);
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            magOut[i] = sqrt(m_c[i] * m_c[i] + m_d[i] * m_d[i]);
            phaseOut[i] = atan2(m_d[i], m_c[i]) ;
        }
    }

    void forwardMagnitude(float *realIn, float *magOut) {
        for (size_t i = 0; i < m_size; ++i) m_a[i] = realIn[i];
        basefft(false, m_a, 0, m_c, m_d);
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            magOut[i] = sqrt(m_c[i] * m_c[i] + m_d[i] * m_d[i]);
        }
    }

    void inverse(double *realIn, double *imagIn, double *realOut) {
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            double real = realIn[i];
            double imag = imagIn[i];
            m_a[i] = real;
            m_b[i] = imag;
            if (i > 0) {
                m_a[m_size-i] = real;
                m_b[m_size-i] = -imag;
            }
        }
        basefft(true, m_a, m_b, realOut, m_d);
    }

    void inversePolar(double *magIn, double *phaseIn, double *realOut) {
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            double real = magIn[i] * cos(phaseIn[i]);
            double imag = magIn[i] * sin(phaseIn[i]);
            m_a[i] = real;
            m_b[i] = imag;
            if (i > 0) {
                m_a[m_size-i] = real;
                m_b[m_size-i] = -imag;
            }
        }
        basefft(true, m_a, m_b, realOut, m_d);
    }

    void inverse(float *realIn, float *imagIn, float *realOut) {
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            float real = realIn[i];
            float imag = imagIn[i];
            m_a[i] = real;
            m_b[i] = imag;
            if (i > 0) {
                m_a[m_size-i] = real;
                m_b[m_size-i] = -imag;
            }
        }
        basefft(true, m_a, m_b, m_c, m_d);
        for (unsigned int i = 0; i < m_size; ++i) realOut[i] = m_c[i];
    }

    void inversePolar(float *magIn, float *phaseIn, float *realOut) {
        for (unsigned int i = 0; i <= m_size/2; ++i) {
            float real = magIn[i] * cosf(phaseIn[i]);
            float imag = magIn[i] * sinf(phaseIn[i]);
            m_a[i] = real;
            m_b[i] = imag;
            if (i > 0) {
                m_a[m_size-i] = real;
                m_b[m_size-i] = -imag;
            }
        }
        basefft(true, m_a, m_b, m_c, m_d);
        for (unsigned int i = 0; i < m_size; ++i) realOut[i] = m_c[i];
    }

    float *getFloatTimeBuffer() {
        if (!m_frb) m_frb = new float[m_size];
        return m_frb;
    }

    double *getDoubleTimeBuffer() {
        if (!m_drb) m_drb = new double[m_size];
        return m_drb;
    }

private:
    unsigned int m_size;
    int *m_table;
    float *m_frb;
    double *m_drb;
    double *m_a;
    double *m_b;
    double *m_c;
    double *m_d;
    void basefft(bool inverse, double *ri, double *ii, double *ro, double *io);
};

void
D_Cross::basefft(bool inverse, double *ri, double *ii, double *ro, double *io)
{
    if (!ri || !ro || !io) return;

    unsigned int i, j, k, m;
    unsigned int blockSize, blockEnd;

    double tr, ti;

    double angle = 2.0 * M_PI;
    if (inverse) angle = -angle;

    const unsigned int n = m_size;

    if (ii) {
	for (i = 0; i < n; ++i) {
	    ro[m_table[i]] = ri[i];
	    io[m_table[i]] = ii[i];
	}
    } else {
	for (i = 0; i < n; ++i) {
	    ro[m_table[i]] = ri[i];
	    io[m_table[i]] = 0.0;
	}
    }

    blockEnd = 1;

    for (blockSize = 2; blockSize <= n; blockSize <<= 1) {

	double delta = angle / (double)blockSize;
	double sm2 = -sin(-2 * delta);
	double sm1 = -sin(-delta);
	double cm2 = cos(-2 * delta);
	double cm1 = cos(-delta);
	double w = 2 * cm1;
	double ar[3], ai[3];

	for (i = 0; i < n; i += blockSize) {

	    ar[2] = cm2;
	    ar[1] = cm1;

	    ai[2] = sm2;
	    ai[1] = sm1;

	    for (j = i, m = 0; m < blockEnd; j++, m++) {

		ar[0] = w * ar[1] - ar[2];
		ar[2] = ar[1];
		ar[1] = ar[0];

		ai[0] = w * ai[1] - ai[2];
		ai[2] = ai[1];
		ai[1] = ai[0];

		k = j + blockEnd;
		tr = ar[0] * ro[k] - ai[0] * io[k];
		ti = ar[0] * io[k] + ai[0] * ro[k];

		ro[k] = ro[j] - tr;
		io[k] = io[j] - ti;

		ro[j] += tr;
		io[j] += ti;
	    }
	}

	blockEnd = blockSize;
    }

/* fftw doesn't rescale, so nor will we

    if (inverse) {

	double denom = (double)n;

	for (i = 0; i < n; i++) {
	    ro[i] /= denom;
	    io[i] /= denom;
	}
    }
*/
}

int
FFT::m_method = -1;

FFT::FFT(unsigned int size)
{
    if (size < 2) throw InvalidSize;
    if (size & (size-1)) throw InvalidSize;

    if (m_method == -1) {
        m_method = 1;
    }

    switch (m_method) {

    case 0:
        d = new D_Cross(size);
        break;

    case 1:
//        std::cerr << "FFT::FFT(" << size << "): using FFTW3 implementation"
//                  << std::endl;
        d = new D_FFTW(size);
        break;

    default:
        std::cerr << "FFT::FFT(" << size << "): WARNING: using slow built-in implementation"
                  << std::endl;
        d = new D_Cross(size);
        break;
    }
}

FFT::~FFT()
{
    delete d;
}

void
FFT::forward(double *realIn, double *realOut, double *imagOut)
{
    d->forward(realIn, realOut, imagOut);
}

void
FFT::forwardPolar(double *realIn, double *magOut, double *phaseOut)
{
    d->forwardPolar(realIn, magOut, phaseOut);
}

void
FFT::forwardMagnitude(double *realIn, double *magOut)
{
    d->forwardMagnitude(realIn, magOut);
}

void
FFT::forward(float *realIn, float *realOut, float *imagOut)
{
    d->forward(realIn, realOut, imagOut);
}

void
FFT::forwardPolar(float *realIn, float *magOut, float *phaseOut)
{
    d->forwardPolar(realIn, magOut, phaseOut);
}

void
FFT::forwardMagnitude(float *realIn, float *magOut)
{
    d->forwardMagnitude(realIn, magOut);
}

void
FFT::inverse(double *realIn, double *imagIn, double *realOut)
{
    d->inverse(realIn, imagIn, realOut);
}

void
FFT::inversePolar(double *magIn, double *phaseIn, double *realOut)
{
    d->inversePolar(magIn, phaseIn, realOut);
}

void
FFT::inverse(float *realIn, float *imagIn, float *realOut)
{
    d->inverse(realIn, imagIn, realOut);
}

void
FFT::inversePolar(float *magIn, float *phaseIn, float *realOut)
{
    d->inversePolar(magIn, phaseIn, realOut);
}

void
FFT::initFloat() 
{
    d->initFloat();
}

void
FFT::initDouble() 
{
    d->initDouble();
}

float *
FFT::getFloatTimeBuffer()
{
    return d->getFloatTimeBuffer();
}

double *
FFT::getDoubleTimeBuffer()
{
    return d->getDoubleTimeBuffer();
}


void
FFT::tune()
{
}


}
