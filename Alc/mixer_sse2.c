/**
 * OpenAL cross platform audio library
 * Copyright (C) 2014 by Timothy Arceri <t_arceri@yahoo.com.au>.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <xmmintrin.h>
#include <emmintrin.h>

#include "alu.h"
#include "mixer_defs.h"


#if (_MSC_VER == 1400)
	__inline __m128i _mm_castps_si128(__m128 PS) { union { __m128 ps; __m128i pi; } c; c.ps = PS; return c.pi; }
	__inline __m128 _mm_castsi128_ps(__m128i PI) { union { __m128 ps; __m128i pi; } c; c.pi = PI; return c.ps; }
#endif

const ALfloat *Resample_lerp32_SSE2(const ALfloat *src, ALuint frac, ALuint increment,
                                    ALfloat *restrict dst, ALuint numsamples)
{
    const __m128i increment4 = _mm_set1_epi32(increment*4);
    const __m128 fracOne4 = _mm_set1_ps(1.0f/FRACTIONONE);
    const __m128i fracMask4 = _mm_set1_epi32(FRACTIONMASK);
    alignas(16) union { ALuint i[4]; float f[4]; } pos_;
    alignas(16) union { ALuint i[4]; float f[4]; } frac_;
    __m128i frac4, pos4;
    ALuint pos;
    ALuint i;

    InitiatePositionArrays(frac, increment, frac_.i, pos_.i, 4);

    frac4 = _mm_castps_si128(_mm_load_ps(frac_.f));
    pos4 = _mm_castps_si128(_mm_load_ps(pos_.f));

    for(i = 0;numsamples-i > 3;i += 4)
    {
        const __m128 val1 = _mm_setr_ps(src[pos_.i[0]], src[pos_.i[1]], src[pos_.i[2]], src[pos_.i[3]]);
        const __m128 val2 = _mm_setr_ps(src[pos_.i[0]+1], src[pos_.i[1]+1], src[pos_.i[2]+1], src[pos_.i[3]+1]);

        /* val1 + (val2-val1)*mu */
        const __m128 r0 = _mm_sub_ps(val2, val1);
        const __m128 mu = _mm_mul_ps(_mm_cvtepi32_ps(frac4), fracOne4);
        const __m128 out = _mm_add_ps(val1, _mm_mul_ps(mu, r0));

        _mm_store_ps(&dst[i], out);

        frac4 = _mm_add_epi32(frac4, increment4);
        pos4 = _mm_add_epi32(pos4, _mm_srli_epi32(frac4, FRACTIONBITS));
        frac4 = _mm_and_si128(frac4, fracMask4);

        _mm_store_ps(pos_.f, _mm_castsi128_ps(pos4));
    }

    pos = pos_.i[0];
    frac = _mm_cvtsi128_si32(frac4);

    for(;i < numsamples;i++)
    {
        dst[i] = lerp(src[pos], src[pos+1], frac * (1.0f/FRACTIONONE));

        frac += increment;
        pos  += frac>>FRACTIONBITS;
        frac &= FRACTIONMASK;
    }
    return dst;
}

const ALfloat *Resample_cubic32_SSE2(const ALfloat *src, ALuint frac, ALuint increment,
                                     ALfloat *restrict dst, ALuint numsamples)
{
    const __m128i increment4 = _mm_set1_epi32(increment*4);
    const __m128i fracMask4 = _mm_set1_epi32(FRACTIONMASK);
    alignas(16) union { ALuint i[4]; float f[4]; } pos_;
    alignas(16) union { ALuint i[4]; float f[4]; } frac_;
    __m128i frac4, pos4;
    ALuint pos;
    ALuint i;

    InitiatePositionArrays(frac, increment, frac_.i, pos_.i, 4);

    frac4 = _mm_castps_si128(_mm_load_ps(frac_.f));
    pos4 = _mm_castps_si128(_mm_load_ps(pos_.f));

    --src;
    for(i = 0;numsamples-i > 3;i += 4)
    {
        const __m128 val0 = _mm_loadu_ps(&src[pos_.i[0]]);
        const __m128 val1 = _mm_loadu_ps(&src[pos_.i[1]]);
        const __m128 val2 = _mm_loadu_ps(&src[pos_.i[2]]);
        const __m128 val3 = _mm_loadu_ps(&src[pos_.i[3]]);
        __m128 k0 = _mm_load_ps(CubicLUT[frac_.i[0]]);
        __m128 k1 = _mm_load_ps(CubicLUT[frac_.i[1]]);
        __m128 k2 = _mm_load_ps(CubicLUT[frac_.i[2]]);
        __m128 k3 = _mm_load_ps(CubicLUT[frac_.i[3]]);
        __m128 out;

        k0 = _mm_mul_ps(k0, val0);
        k1 = _mm_mul_ps(k1, val1);
        k2 = _mm_mul_ps(k2, val2);
        k3 = _mm_mul_ps(k3, val3);
        _MM_TRANSPOSE4_PS(k0, k1, k2, k3);
        out = _mm_add_ps(k0, k1);
        out = _mm_add_ps(out, k2);
        out = _mm_add_ps(out, k3);

        _mm_store_ps(&dst[i], out);

        frac4 = _mm_add_epi32(frac4, increment4);
        pos4 = _mm_add_epi32(pos4, _mm_srli_epi32(frac4, FRACTIONBITS));
        frac4 = _mm_and_si128(frac4, fracMask4);

        _mm_store_ps(pos_.f, _mm_castsi128_ps(pos4));
        _mm_store_ps(frac_.f, _mm_castsi128_ps(frac4));
    }

    pos = pos_.i[0];
    frac = frac_.i[0];

    for(;i < numsamples;i++)
    {
        dst[i] = cubic(src[pos], src[pos+1], src[pos+2], src[pos+3], frac);

        frac += increment;
        pos  += frac>>FRACTIONBITS;
        frac &= FRACTIONMASK;
    }
    return dst;
}
