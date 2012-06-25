/* -*- c-basic-offset: 4 indent-tabs-mode: nil -*-  vi:set ts=8 sts=4 sw=4: */

/*
    Rubber Band
    An audio time-stretching and pitch-shifting library.
    Copyright 2007-2008 Chris Cannam.
    
    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License as
    published by the Free Software Foundation; either version 2 of the
    License, or (at your option) any later version.  See the file
    COPYING included with this distribution for more information.
*/

#include "FFT.h"
#include "Thread.h"
#include "Profiler.h"

//#define FFT_MEASUREMENT 1

#define HAVE_FFTW3 // for Ardour

#ifdef HAVE_FFTW3
#include <fftw3.h>
#endif

#include <cstdlib>

#ifdef USE_KISSFFT
#include "bsd-3rdparty/kissfft/kiss_fftr.h"
#endif

#ifndef HAVE_FFTW3
#ifndef USE_KISSFFT
#ifndef USE_BUILTIN_FFT
#error No FFT implementation selected!
#endif
#endif
#endif

#include <cmath>
#include <iostream>
#include <map>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace RubberBand {

class FFTImpl
{
public:
    virtual ~FFTImpl() { }

    virtual void initFloat() = 0;
    virtual void initDouble() = 0;

    virtual void forward(const double *R__ realIn, double *R__ realOut, double *R__ imagOut) = 0;
    virtual void forwardPolar(const double *R__ realIn, double *R__ magOut, double *R__ phaseOut) = 0;
    virtual void forwardMagnitude(const double *R__ realIn, double *R__ magOut) = 0;

    virtual void forward(const float *R__ realIn, float *R__ realOut, float *R__ imagOut) = 0;
    virtual void forwardPolar(const float *R__ realIn, float *R__ magOut, float *R__ phaseOut) = 0;
    virtual void forwardMagnitude(const float *R__ realIn, float *R__ magOut) = 0;

    virtual void inverse(const double *R__ realIn, const double *R__ imagIn, double *R__ realOut) = 0;
    virtual void inversePolar(const double *R__ magIn, const double *R__ phaseIn, double *R__ realOut) = 0;
    virtual void inverseCepstral(const double *R__ magIn, double *R__ cepOut) = 0;

    virtual void inverse(const float *R__ realIn, const float *R__ imagIn, float *R__ realOut) = 0;
    virtual void inversePolar(const float *R__ magIn, const float *R__ phaseIn, float *R__ realOut) = 0;
    virtual void inverseCepstral(const float *R__ magIn, float *R__ cepOut) = 0;

    virtual float *getFloatTimeBuffer() = 0;
    virtual double *getDoubleTimeBuffer() = 0;
};    

namespace FFTs {


#ifdef HAVE_FFTW3

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

#if defined(FFTW_DOUBLE_ONLY) && defined(FFTW_FLOAT_ONLY)
// Can't meaningfully define both
#undef FFTW_DOUBLE_ONLY
#undef FFTW_FLOAT_ONLY
#endif

#ifdef FFTW_DOUBLE_ONLY
#define fft_float_type double
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
#else
#define fft_float_type float
#endif /* FFTW_DOUBLE_ONLY */

#ifdef FFTW_FLOAT_ONLY
#define fft_double_type float
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
#define sin sinf
#else
#define fft_double_type double
#endif /* FFTW_FLOAT_ONLY */

class D_FFTW : public FFTImpl
{
public:
    D_FFTW(int size) : m_fplanf(0)
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
#ifndef FFTW_DOUBLE_ONLY
            if (save) saveWisdom('f');
#endif
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
#ifndef FFTW_FLOAT_ONLY
            if (save) saveWisdom('d');
#endif
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
#else
        if (load) loadWisdom('f');
#endif
        m_fbuf = (fft_float_type *)fftw_malloc(m_size * sizeof(fft_float_type));
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
#else
        if (load) loadWisdom('d');
#endif
        m_dbuf = (fft_double_type *)fftw_malloc(m_size * sizeof(fft_double_type));
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

    void packFloat(const float *R__ re, const float *R__ im) {
        const int hs = m_size/2;
        fftwf_complex *const R__ fpacked = m_fpacked; 
        for (int i = 0; i <= hs; ++i) {
            fpacked[i][0] = re[i];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                fpacked[i][1] = im[i];
            }
        } else {
            for (int i = 0; i <= hs; ++i) {
                fpacked[i][1] = 0.f;
            }
        }                
    }

    void packDouble(const double *R__ re, const double *R__ im) {
        const int hs = m_size/2;
        fftw_complex *const R__ dpacked = m_dpacked; 
        for (int i = 0; i <= hs; ++i) {
            dpacked[i][0] = re[i];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                dpacked[i][1] = im[i];
            }
        } else {
            for (int i = 0; i <= hs; ++i) {
                dpacked[i][1] = 0.0;
            }
        }
    }

    void unpackFloat(float *R__ re, float *R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            re[i] = m_fpacked[i][0];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                im[i] = m_fpacked[i][1];
            }
        }
    }        

    void unpackDouble(double *R__ re, double *R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            re[i] = m_dpacked[i][0];
        }
        if (im) {
            for (int i = 0; i <= hs; ++i) {
                im[i] = m_dpacked[i][1];
            }
        }
    }        

    void forward(const double *R__ realIn, double *R__ realOut, double *R__ imagOut) {
        if (!m_dplanf) initDouble();
        const int sz = m_size;
        fft_double_type *const R__ dbuf = m_dbuf;
#ifndef FFTW_FLOAT_ONLY
        if (realIn != dbuf) 
#endif
            for (int i = 0; i < sz; ++i) {
                dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        unpackDouble(realOut, imagOut);
    }

    void forwardPolar(const double *R__ realIn, double *R__ magOut, double *R__ phaseOut) {
        if (!m_dplanf) initDouble();
        fft_double_type *const R__ dbuf = m_dbuf;
        const int sz = m_size;
#ifndef FFTW_FLOAT_ONLY
        if (realIn != dbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrt(m_dpacked[i][0] * m_dpacked[i][0] +
                             m_dpacked[i][1] * m_dpacked[i][1]);
        }
        for (int i = 0; i <= hs; ++i) {
            phaseOut[i] = atan2(m_dpacked[i][1], m_dpacked[i][0]);
        }
    }

    void forwardMagnitude(const double *R__ realIn, double *R__ magOut) {
        if (!m_dplanf) initDouble();
        fft_double_type *const R__ dbuf = m_dbuf;
        const int sz = m_size;
#ifndef FFTW_FLOAT_ONLY
        if (realIn != m_dbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                dbuf[i] = realIn[i];
            }
        fftw_execute(m_dplanf);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrt(m_dpacked[i][0] * m_dpacked[i][0] +
                             m_dpacked[i][1] * m_dpacked[i][1]);
        }
    }

    void forward(const float *R__ realIn, float *R__ realOut, float *R__ imagOut) {
        if (!m_fplanf) initFloat();
        fft_float_type *const R__ fbuf = m_fbuf;
        const int sz = m_size;
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        unpackFloat(realOut, imagOut);
    }

    void forwardPolar(const float *R__ realIn, float *R__ magOut, float *R__ phaseOut) {
        if (!m_fplanf) initFloat();
        fft_float_type *const R__ fbuf = m_fbuf;
        const int sz = m_size;
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != fbuf) 
#endif
            for (int i = 0; i < sz; ++i) {
                fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrtf(m_fpacked[i][0] * m_fpacked[i][0] +
                              m_fpacked[i][1] * m_fpacked[i][1]);
        }
        for (int i = 0; i <= hs; ++i) {
            phaseOut[i] = atan2f(m_fpacked[i][1], m_fpacked[i][0]) ;
        }
    }

    void forwardMagnitude(const float *R__ realIn, float *R__ magOut) {
        if (!m_fplanf) initFloat();
        fft_float_type *const R__ fbuf = m_fbuf;
        const int sz = m_size;
#ifndef FFTW_DOUBLE_ONLY
        if (realIn != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                fbuf[i] = realIn[i];
            }
        fftwf_execute(m_fplanf);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrtf(m_fpacked[i][0] * m_fpacked[i][0] +
                              m_fpacked[i][1] * m_fpacked[i][1]);
        }
    }

    void inverse(const double *R__ realIn, const double *R__ imagIn, double *R__ realOut) {
        if (!m_dplanf) initDouble();
        packDouble(realIn, imagIn);
        fftw_execute(m_dplani);
        const int sz = m_size;
        fft_double_type *const R__ dbuf = m_dbuf;
#ifndef FFTW_FLOAT_ONLY
        if (realOut != dbuf) 
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = dbuf[i];
            }
    }

    void inversePolar(const double *R__ magIn, const double *R__ phaseIn, double *R__ realOut) {
        if (!m_dplanf) initDouble();
        const int hs = m_size/2;
        fftw_complex *const R__ dpacked = m_dpacked;
        for (int i = 0; i <= hs; ++i) {
            dpacked[i][0] = magIn[i] * cos(phaseIn[i]);
        }
        for (int i = 0; i <= hs; ++i) {
            dpacked[i][1] = magIn[i] * sin(phaseIn[i]);
        }
        fftw_execute(m_dplani);
        const int sz = m_size;
        fft_double_type *const R__ dbuf = m_dbuf;
#ifndef FFTW_FLOAT_ONLY
        if (realOut != dbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = dbuf[i];
            }
    }

    void inverseCepstral(const double *R__ magIn, double *R__ cepOut) {
        if (!m_dplanf) initDouble();
        fft_double_type *const R__ dbuf = m_dbuf;
        fftw_complex *const R__ dpacked = m_dpacked;
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            dpacked[i][0] = log(magIn[i] + 0.000001);
        }
        for (int i = 0; i <= hs; ++i) {
            dpacked[i][1] = 0.0;
        }
        fftw_execute(m_dplani);
        const int sz = m_size;
#ifndef FFTW_FLOAT_ONLY
        if (cepOut != dbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                cepOut[i] = dbuf[i];
            }
    }

    void inverse(const float *R__ realIn, const float *R__ imagIn, float *R__ realOut) {
        if (!m_fplanf) initFloat();
        packFloat(realIn, imagIn);
        fftwf_execute(m_fplani);
        const int sz = m_size;
        fft_float_type *const R__ fbuf = m_fbuf;
#ifndef FFTW_DOUBLE_ONLY
        if (realOut != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = fbuf[i];
            }
    }

    void inversePolar(const float *R__ magIn, const float *R__ phaseIn, float *R__ realOut) {
        if (!m_fplanf) initFloat();
        const int hs = m_size/2;
        fftwf_complex *const R__ fpacked = m_fpacked;
        for (int i = 0; i <= hs; ++i) {
            fpacked[i][0] = magIn[i] * cosf(phaseIn[i]);
        }
        for (int i = 0; i <= hs; ++i) {
            fpacked[i][1] = magIn[i] * sinf(phaseIn[i]);
        }
        fftwf_execute(m_fplani);
        const int sz = m_size;
        fft_float_type *const R__ fbuf = m_fbuf;
#ifndef FFTW_DOUBLE_ONLY
        if (realOut != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                realOut[i] = fbuf[i];
            }
    }

    void inverseCepstral(const float *R__ magIn, float *R__ cepOut) {
        if (!m_fplanf) initFloat();
        const int hs = m_size/2;
        fftwf_complex *const R__ fpacked = m_fpacked;
        for (int i = 0; i <= hs; ++i) {
            fpacked[i][0] = logf(magIn[i] + 0.000001f);
        }
        for (int i = 0; i <= hs; ++i) {
            fpacked[i][1] = 0.f;
        }
        fftwf_execute(m_fplani);
        const int sz = m_size;
        fft_float_type *const R__ fbuf = m_fbuf;
#ifndef FFTW_DOUBLE_ONLY
        if (cepOut != fbuf)
#endif
            for (int i = 0; i < sz; ++i) {
                cepOut[i] = fbuf[i];
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
    fftw_complex * m_dpacked;
    const int m_size;
    static int m_extantf;
    static int m_extantd;
    static Mutex m_extantMutex;
};

int
D_FFTW::m_extantf = 0;

int
D_FFTW::m_extantd = 0;

Mutex
D_FFTW::m_extantMutex;

#endif /* HAVE_FFTW3 */

#ifdef USE_KISSFFT

class D_KISSFFT : public FFTImpl
{
public:
    D_KISSFFT(int size) :
        m_size(size),
        m_frb(0),
        m_drb(0),
        m_fplanf(0),  
        m_fplani(0)
    {
#ifdef FIXED_POINT
#error KISSFFT is not configured for float values
#endif
        if (sizeof(kiss_fft_scalar) != sizeof(float)) {
            std::cerr << "ERROR: KISSFFT is not configured for float values"
                      << std::endl;
        }

        m_fbuf = new kiss_fft_scalar[m_size + 2];
        m_fpacked = new kiss_fft_cpx[m_size + 2];
        m_fplanf = kiss_fftr_alloc(m_size, 0, NULL, NULL);
        m_fplani = kiss_fftr_alloc(m_size, 1, NULL, NULL);
    }

    ~D_KISSFFT() {
        kiss_fftr_free(m_fplanf);
        kiss_fftr_free(m_fplani);
        kiss_fft_cleanup();

        delete[] m_fbuf;
        delete[] m_fpacked;

        if (m_frb) delete[] m_frb;
        if (m_drb) delete[] m_drb;
    }

    void initFloat() { }
    void initDouble() { }

    void packFloat(const float *R__ re, const float *R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = re[i];
            m_fpacked[i].i = im[i];
        }
    }

    void unpackFloat(float *R__ re, float *R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            re[i] = m_fpacked[i].r;
            im[i] = m_fpacked[i].i;
        }
    }        

    void packDouble(const double *R__ re, const double *R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = float(re[i]);
            m_fpacked[i].i = float(im[i]);
        }
    }

    void unpackDouble(double *R__ re, double *R__ im) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            re[i] = double(m_fpacked[i].r);
            im[i] = double(m_fpacked[i].i);
        }
    }        

    void forward(const double *R__ realIn, double *R__ realOut, double *R__ imagOut) {

        for (int i = 0; i < m_size; ++i) {
            m_fbuf[i] = float(realIn[i]);
        }

        kiss_fftr(m_fplanf, m_fbuf, m_fpacked);
        unpackDouble(realOut, imagOut);
    }

    void forwardPolar(const double *R__ realIn, double *R__ magOut, double *R__ phaseOut) {

        for (int i = 0; i < m_size; ++i) {
            m_fbuf[i] = float(realIn[i]);
        }

        kiss_fftr(m_fplanf, m_fbuf, m_fpacked);

        const int hs = m_size/2;

        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrt(double(m_fpacked[i].r) * double(m_fpacked[i].r) +
                             double(m_fpacked[i].i) * double(m_fpacked[i].i));
        }

        for (int i = 0; i <= hs; ++i) {
            phaseOut[i] = atan2(double(m_fpacked[i].i), double(m_fpacked[i].r));
        }
    }

    void forwardMagnitude(const double *R__ realIn, double *R__ magOut) {

        for (int i = 0; i < m_size; ++i) {
            m_fbuf[i] = float(realIn[i]);
        }

        kiss_fftr(m_fplanf, m_fbuf, m_fpacked);

        const int hs = m_size/2;

        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrt(double(m_fpacked[i].r) * double(m_fpacked[i].r) +
                             double(m_fpacked[i].i) * double(m_fpacked[i].i));
        }
    }

    void forward(const float *R__ realIn, float *R__ realOut, float *R__ imagOut) {

        kiss_fftr(m_fplanf, realIn, m_fpacked);
        unpackFloat(realOut, imagOut);
    }

    void forwardPolar(const float *R__ realIn, float *R__ magOut, float *R__ phaseOut) {

        kiss_fftr(m_fplanf, realIn, m_fpacked);

        const int hs = m_size/2;

        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrtf(m_fpacked[i].r * m_fpacked[i].r +
                              m_fpacked[i].i * m_fpacked[i].i);
        }

        for (int i = 0; i <= hs; ++i) {
            phaseOut[i] = atan2f(m_fpacked[i].i, m_fpacked[i].r);
        }
    }

    void forwardMagnitude(const float *R__ realIn, float *R__ magOut) {

        kiss_fftr(m_fplanf, realIn, m_fpacked);

        const int hs = m_size/2;

        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrtf(m_fpacked[i].r * m_fpacked[i].r +
                              m_fpacked[i].i * m_fpacked[i].i);
        }
    }

    void inverse(const double *R__ realIn, const double *R__ imagIn, double *R__ realOut) {

        packDouble(realIn, imagIn);

        kiss_fftri(m_fplani, m_fpacked, m_fbuf);

        for (int i = 0; i < m_size; ++i) {
            realOut[i] = m_fbuf[i];
        }
    }

    void inversePolar(const double *R__ magIn, const double *R__ phaseIn, double *R__ realOut) {

        const int hs = m_size/2;

        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = float(magIn[i] * cos(phaseIn[i]));
            m_fpacked[i].i = float(magIn[i] * sin(phaseIn[i]));
        }

        kiss_fftri(m_fplani, m_fpacked, m_fbuf);

        for (int i = 0; i < m_size; ++i) {
            realOut[i] = m_fbuf[i];
        }
    }

    void inverseCepstral(const double *R__ magIn, double *R__ cepOut) {

        const int hs = m_size/2;

        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = float(log(magIn[i] + 0.000001));
            m_fpacked[i].i = 0.0f;
        }

        kiss_fftri(m_fplani, m_fpacked, m_fbuf);

        for (int i = 0; i < m_size; ++i) {
            cepOut[i] = m_fbuf[i];
        }
    }
    
    void inverse(const float *R__ realIn, const float *R__ imagIn, float *R__ realOut) {

        packFloat(realIn, imagIn);
        kiss_fftri(m_fplani, m_fpacked, realOut);
    }

    void inversePolar(const float *R__ magIn, const float *R__ phaseIn, float *R__ realOut) {

        const int hs = m_size/2;

        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = magIn[i] * cosf(phaseIn[i]);
            m_fpacked[i].i = magIn[i] * sinf(phaseIn[i]);
        }

        kiss_fftri(m_fplani, m_fpacked, realOut);
    }

    void inverseCepstral(const float *R__ magIn, float *R__ cepOut) {

        const int hs = m_size/2;

        for (int i = 0; i <= hs; ++i) {
            m_fpacked[i].r = logf(magIn[i] + 0.000001f);
            m_fpacked[i].i = 0.0f;
        }

        kiss_fftri(m_fplani, m_fpacked, cepOut);
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
    const int m_size;
    float* m_frb;
    double* m_drb;
    kiss_fftr_cfg m_fplanf;
    kiss_fftr_cfg m_fplani;
    kiss_fft_scalar *m_fbuf;
    kiss_fft_cpx *m_fpacked;
};

#endif /* USE_KISSFFT */

#ifdef USE_BUILTIN_FFT

class D_Cross : public FFTImpl
{
public:
    D_Cross(int size) : m_size(size), m_table(0), m_frb(0), m_drb(0) {
        
        m_a = new double[size];
        m_b = new double[size];
        m_c = new double[size];
        m_d = new double[size];

        m_table = new int[m_size];
    
        int bits;
        int i, j, k, m;

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

    void forward(const double *R__ realIn, double *R__ realOut, double *R__ imagOut) {
        basefft(false, realIn, 0, m_c, m_d);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) realOut[i] = m_c[i];
        if (imagOut) {
            for (int i = 0; i <= hs; ++i) imagOut[i] = m_d[i];
        }
    }

    void forwardPolar(const double *R__ realIn, double *R__ magOut, double *R__ phaseOut) {
        basefft(false, realIn, 0, m_c, m_d);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrt(m_c[i] * m_c[i] + m_d[i] * m_d[i]);
            phaseOut[i] = atan2(m_d[i], m_c[i]) ;
        }
    }

    void forwardMagnitude(const double *R__ realIn, double *R__ magOut) {
        basefft(false, realIn, 0, m_c, m_d);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrt(m_c[i] * m_c[i] + m_d[i] * m_d[i]);
        }
    }

    void forward(const float *R__ realIn, float *R__ realOut, float *R__ imagOut) {
        for (int i = 0; i < m_size; ++i) m_a[i] = realIn[i];
        basefft(false, m_a, 0, m_c, m_d);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) realOut[i] = m_c[i];
        if (imagOut) {
            for (int i = 0; i <= hs; ++i) imagOut[i] = m_d[i];
        }
    }

    void forwardPolar(const float *R__ realIn, float *R__ magOut, float *R__ phaseOut) {
        for (int i = 0; i < m_size; ++i) m_a[i] = realIn[i];
        basefft(false, m_a, 0, m_c, m_d);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrt(m_c[i] * m_c[i] + m_d[i] * m_d[i]);
            phaseOut[i] = atan2(m_d[i], m_c[i]) ;
        }
    }

    void forwardMagnitude(const float *R__ realIn, float *R__ magOut) {
        for (int i = 0; i < m_size; ++i) m_a[i] = realIn[i];
        basefft(false, m_a, 0, m_c, m_d);
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            magOut[i] = sqrt(m_c[i] * m_c[i] + m_d[i] * m_d[i]);
        }
    }

    void inverse(const double *R__ realIn, const double *R__ imagIn, double *R__ realOut) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
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

    void inversePolar(const double *R__ magIn, const double *R__ phaseIn, double *R__ realOut) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
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

    void inverseCepstral(const double *R__ magIn, double *R__ cepOut) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            double real = log(magIn[i] + 0.000001);
            m_a[i] = real;
            m_b[i] = 0.0;
            if (i > 0) {
                m_a[m_size-i] = real;
                m_b[m_size-i] = 0.0;
            }
        }
        basefft(true, m_a, m_b, cepOut, m_d);
    }

    void inverse(const float *R__ realIn, const float *R__ imagIn, float *R__ realOut) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
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
        for (int i = 0; i < m_size; ++i) realOut[i] = m_c[i];
    }

    void inversePolar(const float *R__ magIn, const float *R__ phaseIn, float *R__ realOut) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
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
        for (int i = 0; i < m_size; ++i) realOut[i] = m_c[i];
    }

    void inverseCepstral(const float *R__ magIn, float *R__ cepOut) {
        const int hs = m_size/2;
        for (int i = 0; i <= hs; ++i) {
            float real = logf(magIn[i] + 0.000001);
            m_a[i] = real;
            m_b[i] = 0.0;
            if (i > 0) {
                m_a[m_size-i] = real;
                m_b[m_size-i] = 0.0;
            }
        }
        basefft(true, m_a, m_b, m_c, m_d);
        for (int i = 0; i < m_size; ++i) cepOut[i] = m_c[i];
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
    const int m_size;
    int *m_table;
    float *m_frb;
    double *m_drb;
    double *m_a;
    double *m_b;
    double *m_c;
    double *m_d;
    void basefft(bool inverse, const double *R__ ri, const double *R__ ii, double *R__ ro, double *R__ io);
};

void
D_Cross::basefft(bool inverse, const double *R__ ri, const double *R__ ii, double *R__ ro, double *R__ io)
{
    if (!ri || !ro || !io) return;

    int i, j, k, m;
    int blockSize, blockEnd;

    double tr, ti;

    double angle = 2.0 * M_PI;
    if (inverse) angle = -angle;

    const int n = m_size;

    if (ii) {
	for (i = 0; i < n; ++i) {
	    ro[m_table[i]] = ri[i];
        }
	for (i = 0; i < n; ++i) {
	    io[m_table[i]] = ii[i];
	}
    } else {
	for (i = 0; i < n; ++i) {
	    ro[m_table[i]] = ri[i];
        }
	for (i = 0; i < n; ++i) {
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

#endif /* USE_BUILTIN_FFT */

} /* end namespace FFTs */

int
FFT::m_method = -1;

FFT::FFT(int size, int debugLevel)
{
    if ((size < 2) ||
        (size & (size-1))) {
        std::cerr << "FFT::FFT(" << size << "): power-of-two sizes only supported, minimum size 2" << std::endl;
        throw InvalidSize;
    }

    if (m_method == -1) {
        m_method = 3;
#ifdef USE_KISSFFT
        m_method = 2;
#endif
#ifdef HAVE_FFTW3
        m_method = 1;
#endif
    }

    switch (m_method) {

    case 0:
        std::cerr << "FFT::FFT(" << size << "): WARNING: Selected implementation not available" << std::endl;
#ifdef USE_BUILTIN_FFT
        d = new FFTs::D_Cross(size);
#else
        std::cerr << "FFT::FFT(" << size << "): ERROR: Fallback implementation not available!" << std::endl;
        abort();
#endif
        break;

    case 1:
#ifdef HAVE_FFTW3
        if (debugLevel > 0) {
            std::cerr << "FFT::FFT(" << size << "): using FFTW3 implementation"
                      << std::endl;
        }
        d = new FFTs::D_FFTW(size);
#else
        std::cerr << "FFT::FFT(" << size << "): WARNING: Selected implementation not available" << std::endl;
#ifdef USE_BUILTIN_FFT
        d = new FFTs::D_Cross(size);
#else
        std::cerr << "FFT::FFT(" << size << "): ERROR: Fallback implementation not available!" << std::endl;
        abort();
#endif
#endif
        break;

    case 2:
#ifdef USE_KISSFFT
        if (debugLevel > 0) {
            std::cerr << "FFT::FFT(" << size << "): using KISSFFT implementation"
                      << std::endl;
        }
        d = new FFTs::D_KISSFFT(size);
#else
        std::cerr << "FFT::FFT(" << size << "): WARNING: Selected implementation not available" << std::endl;
#ifdef USE_BUILTIN_FFT
        d = new FFTs::D_Cross(size);
#else
        std::cerr << "FFT::FFT(" << size << "): ERROR: Fallback implementation not available!" << std::endl;
        abort();
#endif
#endif
        break;

    default:
#ifdef USE_BUILTIN_FFT
        std::cerr << "FFT::FFT(" << size << "): WARNING: using slow built-in implementation" << std::endl;
        d = new FFTs::D_Cross(size);
#else
        std::cerr << "FFT::FFT(" << size << "): ERROR: Fallback implementation not available!" << std::endl;
        abort();
#endif
        break;
    }
}

FFT::~FFT()
{
    delete d;
}

void
FFT::forward(const double *R__ realIn, double *R__ realOut, double *R__ imagOut)
{
    d->forward(realIn, realOut, imagOut);
}

void
FFT::forwardPolar(const double *R__ realIn, double *R__ magOut, double *R__ phaseOut)
{
    d->forwardPolar(realIn, magOut, phaseOut);
}

void
FFT::forwardMagnitude(const double *R__ realIn, double *R__ magOut)
{
    d->forwardMagnitude(realIn, magOut);
}

void
FFT::forward(const float *R__ realIn, float *R__ realOut, float *R__ imagOut)
{
    d->forward(realIn, realOut, imagOut);
}

void
FFT::forwardPolar(const float *R__ realIn, float *R__ magOut, float *R__ phaseOut)
{
    d->forwardPolar(realIn, magOut, phaseOut);
}

void
FFT::forwardMagnitude(const float *R__ realIn, float *R__ magOut)
{
    d->forwardMagnitude(realIn, magOut);
}

void
FFT::inverse(const double *R__ realIn, const double *R__ imagIn, double *R__ realOut)
{
    d->inverse(realIn, imagIn, realOut);
}

void
FFT::inversePolar(const double *R__ magIn, const double *R__ phaseIn, double *R__ realOut)
{
    d->inversePolar(magIn, phaseIn, realOut);
}

void
FFT::inverseCepstral(const double *R__ magIn, double *R__ cepOut)
{
    d->inverseCepstral(magIn, cepOut);
}

void
FFT::inverse(const float *R__ realIn, const float *R__ imagIn, float *R__ realOut)
{
    d->inverse(realIn, imagIn, realOut);
}

void
FFT::inversePolar(const float *R__ magIn, const float *R__ phaseIn, float *R__ realOut)
{
    d->inversePolar(magIn, phaseIn, realOut);
}

void
FFT::inverseCepstral(const float *R__ magIn, float *R__ cepOut)
{
    d->inverseCepstral(magIn, cepOut);
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
