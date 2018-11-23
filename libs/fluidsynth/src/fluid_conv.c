/* FluidSynth - A Software Synthesizer
 *
 * Copyright (C) 2003  Peter Hanappe and others.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 2.1 of
 * the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA
 */

#include "fluid_conv.h"
#include "fluid_conv_tables.c"

/*
 * fluid_ct2hz
 */
fluid_real_t
fluid_ct2hz_real(fluid_real_t cents)
{
    if(cents < 0)
    {
        return (fluid_real_t) 1.0;
    }
    else if(cents < 900)
    {
        return (fluid_real_t) 6.875 * fluid_ct2hz_tab[(int)(cents + 300)];
    }
    else if(cents < 2100)
    {
        return (fluid_real_t) 13.75 * fluid_ct2hz_tab[(int)(cents - 900)];
    }
    else if(cents < 3300)
    {
        return (fluid_real_t) 27.5 * fluid_ct2hz_tab[(int)(cents - 2100)];
    }
    else if(cents < 4500)
    {
        return (fluid_real_t) 55.0 * fluid_ct2hz_tab[(int)(cents - 3300)];
    }
    else if(cents < 5700)
    {
        return (fluid_real_t) 110.0 * fluid_ct2hz_tab[(int)(cents - 4500)];
    }
    else if(cents < 6900)
    {
        return (fluid_real_t) 220.0 * fluid_ct2hz_tab[(int)(cents - 5700)];
    }
    else if(cents < 8100)
    {
        return (fluid_real_t) 440.0 * fluid_ct2hz_tab[(int)(cents - 6900)];
    }
    else if(cents < 9300)
    {
        return (fluid_real_t) 880.0 * fluid_ct2hz_tab[(int)(cents - 8100)];
    }
    else if(cents < 10500)
    {
        return (fluid_real_t) 1760.0 * fluid_ct2hz_tab[(int)(cents - 9300)];
    }
    else if(cents < 11700)
    {
        return (fluid_real_t) 3520.0 * fluid_ct2hz_tab[(int)(cents - 10500)];
    }
    else if(cents < 12900)
    {
        return (fluid_real_t) 7040.0 * fluid_ct2hz_tab[(int)(cents - 11700)];
    }
    else if(cents < 14100)
    {
        return (fluid_real_t) 14080.0 * fluid_ct2hz_tab[(int)(cents - 12900)];
    }
    else
    {
        return (fluid_real_t) 1.0; /* some loony trying to make you deaf */
    }
}

/*
 * fluid_ct2hz
 */
fluid_real_t
fluid_ct2hz(fluid_real_t cents)
{
    /* Filter fc limit: SF2.01 page 48 # 8 */
    if(cents >= 13500)
    {
        cents = 13500;             /* 20 kHz */
    }
    else if(cents < 1500)
    {
        cents = 1500;              /* 20 Hz */
    }

    return fluid_ct2hz_real(cents);
}

/*
 * fluid_cb2amp
 *
 * in: a value between 0 and 1440, 0 is no attenuation
 * out: a value between 1 and 0
 */
fluid_real_t
fluid_cb2amp(fluid_real_t cb)
{
    /*
     * cb: an attenuation in 'centibels' (1/10 dB)
     * SF2.01 page 49 # 48 limits it to 144 dB.
     * 96 dB is reasonable for 16 bit systems, 144 would make sense for 24 bit.
     */

    /* minimum attenuation: 0 dB */
    if(cb < 0)
    {
        return 1.0;
    }

    if(cb >= FLUID_CB_AMP_SIZE)
    {
        return 0.0;
    }

    return fluid_cb2amp_tab[(int) cb];
}

/*
 * fluid_tc2sec_delay
 */
fluid_real_t
fluid_tc2sec_delay(fluid_real_t tc)
{
    /* SF2.01 section 8.1.2 items 21, 23, 25, 33
     * SF2.01 section 8.1.3 items 21, 23, 25, 33
     *
     * The most negative number indicates a delay of 0. Range is limited
     * from -12000 to 5000 */
    if(tc <= -32768.0f)
    {
        return (fluid_real_t) 0.0f;
    };

    if(tc < -12000.)
    {
        tc = (fluid_real_t) -12000.0f;
    }

    if(tc > 5000.0f)
    {
        tc = (fluid_real_t) 5000.0f;
    }

    return (fluid_real_t) pow(2.0, (double) tc / 1200.0);
}

/*
 * fluid_tc2sec_attack
 */
fluid_real_t
fluid_tc2sec_attack(fluid_real_t tc)
{
    /* SF2.01 section 8.1.2 items 26, 34
     * SF2.01 section 8.1.3 items 26, 34
     * The most negative number indicates a delay of 0
     * Range is limited from -12000 to 8000 */
    if(tc <= -32768.)
    {
        return (fluid_real_t) 0.0;
    };

    if(tc < -12000.)
    {
        tc = (fluid_real_t) -12000.0;
    };

    if(tc > 8000.)
    {
        tc = (fluid_real_t) 8000.0;
    };

    return (fluid_real_t) pow(2.0, (double) tc / 1200.0);
}

/*
 * fluid_tc2sec
 */
fluid_real_t
fluid_tc2sec(fluid_real_t tc)
{
    /* No range checking here! */
    return (fluid_real_t) pow(2.0, (double) tc / 1200.0);
}

/*
 * fluid_tc2sec_release
 */
fluid_real_t
fluid_tc2sec_release(fluid_real_t tc)
{
    /* SF2.01 section 8.1.2 items 30, 38
     * SF2.01 section 8.1.3 items 30, 38
     * No 'most negative number' rule here!
     * Range is limited from -12000 to 8000 */
    if(tc <= -32768.)
    {
        return (fluid_real_t) 0.0;
    };

    if(tc < -12000.)
    {
        tc = (fluid_real_t) -12000.0;
    };

    if(tc > 8000.)
    {
        tc = (fluid_real_t) 8000.0;
    };

    return (fluid_real_t) pow(2.0, (double) tc / 1200.0);
}

/*
 * fluid_act2hz
 *
 * Convert from absolute cents to Hertz
 * 
 * The inverse operation, converting from Hertz to cents, was unused and implemented as
 *
fluid_hz2ct(fluid_real_t f)
{
    return (fluid_real_t)(6900 + (1200 / M_LN2) * log(f / 440.0));
}
 */
fluid_real_t
fluid_act2hz(fluid_real_t c)
{
    return (fluid_real_t)(8.176 * pow(2.0, (double) c / 1200.0));
}

/*
 * fluid_pan
 */
fluid_real_t
fluid_pan(fluid_real_t c, int left)
{
    if(left)
    {
        c = -c;
    }

    if(c <= -500)
    {
        return (fluid_real_t) 0.0;
    }
    else if(c >= 500)
    {
        return (fluid_real_t) 1.0;
    }
    else
    {
        return fluid_pan_tab[(int)(c + 500)];
    }
}

/*
 * Return the amount of attenuation based on the balance for the specified
 * channel. If balance is negative (turned toward left channel, only the right
 * channel is attenuated. If balance is positive, only the left channel is
 * attenuated.
 *
 * @params balance left/right balance, range [-960;960] in absolute centibels
 * @return amount of attenuation [0.0;1.0]
 */
fluid_real_t fluid_balance(fluid_real_t balance, int left)
{
    /* This is the most common case */
    if(balance == 0)
    {
        return 1.0f;
    }

    if((left && balance < 0) || (!left && balance > 0))
    {
        return 1.0f;
    }

    if(balance < 0)
    {
        balance = -balance;
    }

    return fluid_cb2amp(balance);
}

/*
 * fluid_concave
 */
fluid_real_t
fluid_concave(fluid_real_t val)
{
    if(val < 0)
    {
        return 0;
    }
    else if(val >= FLUID_VEL_CB_SIZE)
    {
        return 1;
    }

    return fluid_concave_tab[(int) val];
}

/*
 * fluid_convex
 */
fluid_real_t
fluid_convex(fluid_real_t val)
{
    if(val < 0)
    {
        return 0;
    }
    else if(val >= FLUID_VEL_CB_SIZE)
    {
        return 1;
    }

    return fluid_convex_tab[(int) val];
}

