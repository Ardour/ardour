#ifndef NOISE_H
#define NOISE_H

/* Can be overrriden with any code that produces whitenoise between 0.0f and
 * 1.0f, eg (random() / (float)RAND_MAX) should be a good source of noise, but
 * its expensive */
#ifndef GDITHER_NOISE
#define GDITHER_NOISE gdither_noise()
#endif

inline static float gdither_noise()
{
    static uint32_t rnd = 23232323;
    rnd = (rnd * 196314165) + 907633515;

    return rnd * 2.3283064365387e-10f;
}

#endif
