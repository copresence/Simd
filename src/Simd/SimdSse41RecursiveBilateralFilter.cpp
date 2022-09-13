/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2022 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#include "Simd/SimdMemory.h"
#include "Simd/SimdRecursiveBilateralFilter.h"

namespace Simd
{
#ifdef SIMD_SSE41_ENABLE
    namespace Sse41
    {
        typedef Base::RbfAlg RbfAlg;

        template<size_t channels> int Diff(const uint8_t* src1, const uint8_t* src2)
        {
            int diff, diffs[4];
            for (int c = 0; c < channels; c++)
                diffs[c] = abs(src1[c] - src2[c]);
            switch (channels)
            {
            case 1:
                diff = diffs[0];
                break;
            case 2:
                diff = ((diffs[0] + diffs[1]) >> 1);
                break;
            case 3:
                diff = ((diffs[0] + diffs[2]) >> 2) + (diffs[1] >> 1);
                break;
            case 4:
                //diff = ((diffs[0] + diffs[2]) >> 2) + (diffs[1] >> 1);
                diff = ((diffs[0] + diffs[1] + diffs[2] + diffs[3]) >> 2);
                break;
            default:
                diff = 0;
            }
            assert(diff >= 0 && diff <= 255);
            return diff;
        }

        template<size_t channels> SIMD_INLINE void SetOut(const float* bc, const float* bf, const float* ec, const float* ef, size_t width, uint8_t* dst)
        {
            for (size_t x = 0; x < width; x++)
            {
                float factor = 1.f / (bf[x] + ef[x]);
                for (size_t c = 0; c < channels; c++)
                    dst[c] = uint8_t(factor * (bc[c] + ec[c]));
                bc += channels;
                ec += channels;
                dst += channels;
            }
        }

        template<size_t channels> void HorFilter(const RbfParam& p, RbfAlg& a, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
        {
            size_t size = p.width * channels, cLast = size - 1, fLast = p.width - 1;
            for (size_t y = 0; y < p.height; y++)
            {
                const uint8_t* sl = src, * sr = src + cLast;
                float* lc = a.cb0.data, * rc = a.cb1.data + cLast;
                float* lf = a.fb0.data, * rf = a.fb1.data + fLast;
                *lf++ = 1.f;
                *rf-- = 1.f;
                for (int c = 0; c < channels; c++)
                {
                    *lc++ = *sl++;
                    *rc-- = *sr--;
                }
                for (size_t x = 1; x < p.width; x++)
                {
                    int ld = Diff<channels>(sl, sl - channels);
                    int rd = Diff<channels>(sr + 1 - channels, sr + 1);
                    float la = a.ranges[ld];
                    float ra = a.ranges[rd];
                    *lf++ = a.alpha + la * lf[-1];
                    *rf-- = a.alpha + ra * rf[+1];
                    for (int c = 0; c < channels; c++)
                    {
                        *lc++ = (a.alpha * (*sl++) + la * lc[-channels]);
                        *rc-- = (a.alpha * (*sr--) + ra * rc[+channels]);
                    }
                }
                SetOut<channels>(a.cb0.data, a.fb0.data, a.cb1.data, a.fb1.data, p.width, dst);
                src += srcStride;
                dst += dstStride;
            }
        }

        template<size_t channels> void VerSetEdge(const uint8_t* src, size_t width, float* factor, float* colors)
        {
            size_t widthF = AlignLo(width, F);
            size_t x = 0;
            __m128 _1 = _mm_set1_ps(1.0f);
            for (; x < widthF; x += F)
                _mm_storeu_ps(factor + x, _1);
            for (; x < width; x++)
                factor[x] = 1.0f;

            size_t size = width * channels, sizeF = AlignLo(size, F);
            size_t i = 0;
            for (; i < sizeF; i += F)
            {
                __m128i i32 = _mm_cvtepu8_epi32( _mm_cvtsi32_si128(*(int32_t*)(src + i)));
                _mm_storeu_ps(colors + i, _mm_cvtepi32_ps(i32));
            }
            for (; i < size; i++)
                colors[i] = src[i];
        }

        template<size_t channels> void VerFilter(const RbfParam& p, RbfAlg& a, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
        {
            size_t size = p.width * channels, srcTail = srcStride - size, dstTail = dstStride - size;
            float* dcb = a.cb0.data, * dfb = a.fb0.data, * ucb = a.cb1.data, * ufb = a.fb1.data;

            const uint8_t* suc = src + srcStride * (p.height - 1);
            const uint8_t* duc = dst + dstStride * (p.height - 1);
            float* uf = ufb + p.width * (p.height - 1);
            float* uc = ucb + size * (p.height - 1);

            VerSetEdge<channels>(duc, p.width, uf, uc);
            duc -= dstStride;
            suc -= srcStride;
            uf -= p.width;
            uc -= size;
            for (int y = 1; y < p.height; y++)
            {
                for (int x = 0, o = 0; x < p.width; x++, o += channels)
                {
                    int ud = Diff<channels>(suc + o, suc + o + srcStride);
                    float ua = a.ranges[ud];
                    uf[x] = a.alpha + ua * uf[x + p.width];
                    for (int c = 0; c < channels; c++)
                        uc[o + c] = a.alpha * duc[o + c] + ua * uc[o + c + size];
                }
                duc -= dstStride;
                suc -= srcStride;
                uf -= p.width;
                uc -= size;
            }

            VerSetEdge<channels>(dst, p.width, dfb, dcb);
            SetOut<channels>(dcb, dfb, ucb, ufb, p.width, dst);
            for (int y = 1; y < p.height; y++)
            {
                const uint8_t* sdc = src + y * srcStride;
                const uint8_t* ddc = dst + y * dstStride;
                float* dc = dcb + (y & 1) * size;
                float* df = dfb + (y & 1) * p.width;
                const float* dpc = dcb + ((y - 1) & 1) * size;
                const float* dpf = dfb + ((y - 1) & 1) * p.width;

                for (int x = 0; x < p.width; x++)
                {
                    int dd = Diff<channels>(sdc, sdc - srcStride);
                    sdc += channels;
                    float da = a.ranges[dd];
                    *df++ = a.alpha + da * (*dpf++);
                    for (int c = 0; c < channels; c++)
                        *dc++ = a.alpha * (*ddc++) + da * (*dpc++);
                }
                ddc += dstTail;
                sdc += srcTail;
                SetOut<channels>(dcb + (y & 1) * size, dfb + (y & 1) * p.width, ucb + y * size, ufb + y * p.width, p.width, dst + y * dstStride);
            }
        }

		//-----------------------------------------------------------------------------------------

        RecursiveBilateralFilterDefault::RecursiveBilateralFilterDefault(const RbfParam& param)
            :Base::RecursiveBilateralFilterDefault(param)
        {
            switch (_param.channels)
            {
            case 1: _hFilter = HorFilter<1>; _vFilter = VerFilter<1>; break;
            case 2: _hFilter = HorFilter<2>; _vFilter = VerFilter<2>; break;
            case 3: _hFilter = HorFilter<3>; _vFilter = VerFilter<3>; break;
            case 4: _hFilter = HorFilter<4>; _vFilter = VerFilter<4>; break;
            default:
                assert(0);
            }
        }

        void RecursiveBilateralFilterDefault::Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
        {
            InitBuf();
            _hFilter(_param, _alg, src, srcStride, dst, dstStride);
            _vFilter(_param, _alg, src, srcStride, dst, dstStride);
        }

        //-----------------------------------------------------------------------------------------

        void* RecursiveBilateralFilterInit(size_t width, size_t height, size_t channels, const float* sigmaSpatial, const float* sigmaRange)
        {
            RbfParam param(width, height, channels, sigmaSpatial, sigmaRange, sizeof(void*));
            if (!param.Valid())
                return NULL;
            return new RecursiveBilateralFilterDefault(param);
        }
    }
#endif
}

