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
#include "Simd/SimdStore.h"
#include "Simd/SimdRecursiveBilateralFilter.h"
#include "Simd/SimdPerformance.h"

namespace Simd
{
#ifdef SIMD_SSE41_ENABLE
    namespace Sse41
    {
        namespace Prec
        {
            template<size_t channels> void RowRanges(const uint8_t* src0, const uint8_t* src1, size_t width, const float* ranges, float* dst);

            SIMD_INLINE void Ranges1(const uint8_t* src0, const uint8_t* src1, const float* ranges, float* dst)
            {
                __m128i s0 = _mm_loadu_si128((__m128i*)src0);
                __m128i s1 = _mm_loadu_si128((__m128i*)src1);
                __m128i d = _mm_sub_epi8(_mm_max_epu8(s0, s1), _mm_min_epu8(s0, s1));
                SIMD_ALIGNED(A) uint8_t diff[A];
                _mm_storeu_si128((__m128i*)diff, d);
                for (size_t i = 0; i < A; ++i)
                    dst[i] = ranges[diff[i]];
            }

            template<> void RowRanges<1>(const uint8_t* src0, const uint8_t* src1, size_t width, const float* ranges, float* dst)
            {
                size_t widthA = AlignLo(width, A), x = 0;
                for (; x < widthA; x += A)
                    Ranges1(src0 + x, src1 + x, ranges, dst + x);
                if(widthA < width)
                {
                    x = width - A;
                    Ranges1(src0 + x, src1 + x, ranges, dst + x);
                }
            }

            SIMD_INLINE void Ranges2(const uint8_t* src0, const uint8_t* src1, const float* ranges, float* dst)
            {
                __m128i s0 = _mm_loadu_si128((__m128i*)src0);
                __m128i s1 = _mm_loadu_si128((__m128i*)src1);
                __m128i d8 = _mm_sub_epi8(_mm_max_epu8(s0, s1), _mm_min_epu8(s0, s1));
                __m128i d16_0 = _mm_and_si128(d8, K16_00FF);
                __m128i d16_1 = _mm_and_si128(_mm_srli_si128(d8, 1), K16_00FF);
                __m128i a16 = _mm_srli_epi16(_mm_add_epi16(d16_0, d16_1), 1);
                SIMD_ALIGNED(A) uint16_t diff[HA];
                _mm_storeu_si128((__m128i*)diff, a16);
                for (size_t i = 0; i < HA; ++i)
                    dst[i] = ranges[diff[i]];
            }

            template<> void RowRanges<2>(const uint8_t* src0, const uint8_t* src1, size_t width, const float* ranges, float* dst)
            {
                size_t widthHA = AlignLo(width, HA), x = 0, o = 0;
                for (; x < widthHA; x += HA, o += A)
                    Ranges2(src0 + o, src1 + o, ranges, dst + x);
                if (widthHA < width)
                {
                    x = width - HA, o = x * 2;
                    Ranges2(src0 + o, src1 + o, ranges, dst + x);
                }
            }

            SIMD_INLINE void Ranges3(const uint8_t* src0, const uint8_t* src1, const float* ranges, float* dst)
            {
                static const __m128i K0 = SIMD_MM_SETR_EPI8(0x0, -1, -1, -1, 0x3, -1, -1, -1, 0x6, -1, -1, -1, 0x9, -1, -1, -1);
                static const __m128i K1 = SIMD_MM_SETR_EPI8(0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xa, -1, -1, -1);
                static const __m128i K2 = SIMD_MM_SETR_EPI8(0x2, -1, -1, -1, 0x5, -1, -1, -1, 0x8, -1, -1, -1, 0xb, -1, -1, -1);
                __m128i s0 = _mm_loadu_si128((__m128i*)src0);
                __m128i s1 = _mm_loadu_si128((__m128i*)src1);
                __m128i d8 = _mm_sub_epi8(_mm_max_epu8(s0, s1), _mm_min_epu8(s0, s1));
                __m128i d32_0 = _mm_shuffle_epi8(d8, K0);
                __m128i d32_1 = _mm_shuffle_epi8(d8, K1);
                __m128i d32_2 = _mm_shuffle_epi8(d8, K2);
                __m128i a32 = _mm_srli_epi16(_mm_add_epi32(_mm_add_epi32(d32_0, d32_1), _mm_add_epi32(d32_1, d32_2)), 2);
                SIMD_ALIGNED(A) uint32_t diff[F];
                _mm_storeu_si128((__m128i*)diff, a32);
                for (size_t i = 0; i < F; ++i)
                    dst[i] = ranges[diff[i]];
            }

            template<> void RowRanges<3>(const uint8_t* src0, const uint8_t* src1, size_t width, const float* ranges, float* dst)
            {
                size_t widthF = AlignLo(width, F), x = 0, o = 0;
                for (; x < widthF; x += F, o += F * 3)
                    Ranges3(src0 + o, src1 + o, ranges, dst + x);
                if (widthF < width)
                {
                    x = width - F, o = x * 3;
                    Ranges3(src0 + o, src1 + o, ranges, dst + x);
                }
            }

            SIMD_INLINE void Ranges4(const uint8_t* src0, const uint8_t* src1, const float* ranges, float* dst)
            {
                static const __m128i K0 = SIMD_MM_SETR_EPI8(0x0, -1, -1, -1, 0x4, -1, -1, -1, 0x8, -1, -1, -1, 0xc, -1, -1, -1);
                static const __m128i K1 = SIMD_MM_SETR_EPI8(0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xd, -1, -1, -1);
                static const __m128i K2 = SIMD_MM_SETR_EPI8(0x2, -1, -1, -1, 0x6, -1, -1, -1, 0xa, -1, -1, -1, 0xe, -1, -1, -1);
                __m128i s0 = _mm_loadu_si128((__m128i*)src0);
                __m128i s1 = _mm_loadu_si128((__m128i*)src1);
                __m128i d8 = _mm_sub_epi8(_mm_max_epu8(s0, s1), _mm_min_epu8(s0, s1));
                __m128i d32_0 = _mm_shuffle_epi8(d8, K0);
                __m128i d32_1 = _mm_shuffle_epi8(d8, K1);
                __m128i d32_2 = _mm_shuffle_epi8(d8, K2);
                __m128i a32 = _mm_srli_epi16(_mm_add_epi32(_mm_add_epi32(d32_0, d32_1), _mm_add_epi32(d32_1, d32_2)), 2);
                SIMD_ALIGNED(A) uint32_t diff[F];
                _mm_storeu_si128((__m128i*)diff, a32);
                for (size_t i = 0; i < F; ++i)
                    dst[i] = ranges[diff[i]];
            }

            template<> void RowRanges<4>(const uint8_t* src0, const uint8_t* src1, size_t width, const float* ranges, float* dst)
            {
                size_t widthF = AlignLo(width, F), x = 0, o = 0;
                for (; x < widthF; x += F, o += A)
                    Ranges4(src0 + o, src1 + o, ranges, dst + x);
                if (widthF < width)
                {
                    x = width - F, o = x * 4;
                    Ranges4(src0 + o, src1 + o, ranges, dst + x);
                }
            }

            //-----------------------------------------------------------------------------------------

            template<size_t channels> SIMD_INLINE void SetOut(const float* bc, const float* bf, const float* ec, const float* ef, size_t width, uint8_t* dst);

            template<> SIMD_INLINE void SetOut<1>(const float* bc, const float* bf, const float* ec, const float* ef, size_t width, uint8_t* dst)
            {
                size_t widthF = AlignLo(width, F), x = 0;
                __m128 _1 = _mm_set1_ps(1.0f);
                for (; x < widthF; x += F)
                {
                    __m128 _bf = _mm_loadu_ps(bf + x);
                    __m128 _ef = _mm_loadu_ps(ef + x);
                    __m128 factor = _mm_div_ps(_1, _mm_add_ps(_bf, _ef));
                    __m128 _bc = _mm_loadu_ps(bc + x);
                    __m128 _ec = _mm_loadu_ps(ec + x);
                    __m128 f32 = _mm_mul_ps(factor, _mm_add_ps(_bc, _ec));
                    __m128i i32 = _mm_cvtps_epi32(_mm_floor_ps(f32));
                    __m128i u8 = _mm_packus_epi16(_mm_packs_epi32(i32, K_ZERO), K_ZERO);
                    *(int32_t*)(dst + x) = _mm_cvtsi128_si32(u8);
                }
                for (; x < width; x++)
                {
                    float factor = 1.0f / (bf[x] + ef[x]);
                    dst[x] = uint8_t(factor * (bc[x] + ec[x]));
                }
            }

            template<> SIMD_INLINE void SetOut<2>(const float* bc, const float* bf, const float* ec, const float* ef, size_t width, uint8_t* dst)
            {
                size_t widthF = AlignLo(width, F), widthHF = AlignLo(width, 2), x = 0, o = 0;
                __m128 _1 = _mm_set1_ps(1.0f);
                for (; x < widthF; x += F, o += DF)
                {
                    __m128 _bf = _mm_loadu_ps(bf + x);
                    __m128 _ef = _mm_loadu_ps(ef + x);
                    __m128 factor = _mm_div_ps(_1, _mm_add_ps(_bf, _ef));
                    __m128 f0 = _mm_mul_ps(Shuffle32f<0x50>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + 0), _mm_loadu_ps(ec + o + 0)));
                    __m128 f1 = _mm_mul_ps(Shuffle32f<0xFA>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + F), _mm_loadu_ps(ec + o + F)));
                    __m128i u8 = _mm_packus_epi16(_mm_packs_epi32(_mm_cvtps_epi32(_mm_floor_ps(f0)), _mm_cvtps_epi32(_mm_floor_ps(f1))), K_ZERO);
                    StoreHalf<0>((__m128i*)(dst + o), u8);
                }
                for (; x < widthHF; x += HF, o += F)
                {
                    __m128 _bf = _mm_loadl_pi(_mm_setzero_ps(), (__m64*)(bf + x));
                    __m128 _ef = _mm_loadl_pi(_mm_setzero_ps(), (__m64*)(ef + x));
                    __m128 factor = Shuffle32f<0x50>(_mm_div_ps(_1, _mm_add_ps(_bf, _ef)));
                    __m128 _bc = _mm_loadu_ps(bc + o);
                    __m128 _ec = _mm_loadu_ps(ec + o);
                    __m128 f32 = _mm_mul_ps(factor, _mm_add_ps(_bc, _ec));
                    __m128i i32 = _mm_cvtps_epi32(_mm_floor_ps(f32));
                    __m128i u8 = _mm_packus_epi16(_mm_packs_epi32(i32, K_ZERO), K_ZERO);
                    *(int32_t*)(dst + o) = _mm_cvtsi128_si32(u8);
                }
                for (; x < width; x++, o += 2)
                {
                    float factor = 1.0f / (bf[x] + ef[x]);
                    dst[o + 0] = uint8_t(factor * (bc[o + 0] + ec[o + 0]));
                    dst[o + 1] = uint8_t(factor * (bc[o + 1] + ec[o + 1]));
                }
            }

            template<> SIMD_INLINE void SetOut<3>(const float* bc, const float* bf, const float* ec, const float* ef, size_t width, uint8_t* dst)
            {
                size_t width1 = width - 1, widthF = AlignLo(width - 2, F), x = 0, o = 0;
                __m128 _1 = _mm_set1_ps(1.0f);
                for (; x < widthF; x += F, o += 3 * F)
                {
                    __m128 _bf = _mm_loadu_ps(bf + x);
                    __m128 _ef = _mm_loadu_ps(ef + x);
                    __m128 factor = _mm_div_ps(_1, _mm_add_ps(_bf, _ef));
                    __m128 f0 = _mm_mul_ps(Shuffle32f<0x40>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + 0 * F), _mm_loadu_ps(ec + o + 0 * F)));
                    __m128 f1 = _mm_mul_ps(Shuffle32f<0xA5>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + 1 * F), _mm_loadu_ps(ec + o + 1 * F)));
                    __m128 f2 = _mm_mul_ps(Shuffle32f<0xFE>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + 2 * F), _mm_loadu_ps(ec + o + 2 * F)));
                    __m128i i0 = _mm_cvtps_epi32(_mm_floor_ps(f0));
                    __m128i i1 = _mm_cvtps_epi32(_mm_floor_ps(f1));
                    __m128i i2 = _mm_cvtps_epi32(_mm_floor_ps(f2));
                    __m128i u8 = _mm_packus_epi16(_mm_packs_epi32(i0, i1), _mm_packs_epi32(i2, K_ZERO));
                    _mm_storeu_si128((__m128i*)(dst + o), u8);
                }
                for (; x < width1; x += 1, o += 3)
                {
                    __m128 _bf = _mm_set1_ps(bf[x]);
                    __m128 _ef = _mm_set1_ps(ef[x]);
                    __m128 factor = _mm_div_ps(_1, _mm_add_ps(_bf, _ef));
                    __m128 _bc = _mm_loadu_ps(bc + o);
                    __m128 _ec = _mm_loadu_ps(ec + o);
                    __m128 f32 = _mm_mul_ps(factor, _mm_add_ps(_bc, _ec));
                    __m128i i32 = _mm_cvtps_epi32(_mm_floor_ps(f32));
                    __m128i u8 = _mm_packus_epi16(_mm_packs_epi32(i32, K_ZERO), K_ZERO);
                    *(int32_t*)(dst + o) = _mm_cvtsi128_si32(u8);
                }
                float factor = 1.0f / (bf[x] + ef[x]);
                dst[o + 0] = uint8_t(factor * (bc[o + 0] + ec[o + 0]));
                dst[o + 1] = uint8_t(factor * (bc[o + 1] + ec[o + 1]));
                dst[o + 2] = uint8_t(factor * (bc[o + 2] + ec[o + 2]));
            }

            template<> SIMD_INLINE void SetOut<4>(const float* bc, const float* bf, const float* ec, const float* ef, size_t width, uint8_t* dst)
            {
                size_t widthF = AlignLo(width, F), x = 0, o = 0;
                __m128 _1 = _mm_set1_ps(1.0f);
                for (; x < widthF; x += F, o += 4 * F)
                {
                    __m128 _bf = _mm_loadu_ps(bf + x);
                    __m128 _ef = _mm_loadu_ps(ef + x);
                    __m128 factor = _mm_div_ps(_1, _mm_add_ps(_bf, _ef));
                    __m128 f0 = _mm_mul_ps(Shuffle32f<0x00>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + 0 * F), _mm_loadu_ps(ec + o + 0 * F)));
                    __m128 f1 = _mm_mul_ps(Shuffle32f<0x55>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + 1 * F), _mm_loadu_ps(ec + o + 1 * F)));
                    __m128 f2 = _mm_mul_ps(Shuffle32f<0xAA>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + 2 * F), _mm_loadu_ps(ec + o + 2 * F)));
                    __m128 f3 = _mm_mul_ps(Shuffle32f<0xFF>(factor), _mm_add_ps(_mm_loadu_ps(bc + o + 3 * F), _mm_loadu_ps(ec + o + 3 * F)));
                    __m128i i0 = _mm_cvtps_epi32(_mm_floor_ps(f0));
                    __m128i i1 = _mm_cvtps_epi32(_mm_floor_ps(f1));
                    __m128i i2 = _mm_cvtps_epi32(_mm_floor_ps(f2));
                    __m128i i3 = _mm_cvtps_epi32(_mm_floor_ps(f3));
                    __m128i u8 = _mm_packus_epi16(_mm_packs_epi32(i0, i1), _mm_packs_epi32(i2, i3));
                    _mm_storeu_si128((__m128i*)(dst + o), u8);
                }
                for (; x < width; x += 1, o += 4)
                {
                    __m128 _bf = _mm_set1_ps(bf[x]);
                    __m128 _ef = _mm_set1_ps(ef[x]);
                    __m128 factor = _mm_div_ps(_1, _mm_add_ps(_bf, _ef));
                    __m128 _bc = _mm_loadu_ps(bc + o);
                    __m128 _ec = _mm_loadu_ps(ec + o);
                    __m128 f32 = _mm_mul_ps(factor, _mm_add_ps(_bc, _ec));
                    __m128i i32 = _mm_cvtps_epi32(_mm_floor_ps(f32));
                    __m128i u8 = _mm_packus_epi16(_mm_packs_epi32(i32, K_ZERO), K_ZERO);
                    *(int32_t*)(dst + o) = _mm_cvtsi128_si32(u8);
                }
            }

            //-----------------------------------------------------------------------------------------

            template<size_t channels> void HorFilter(const RbfParam& p, float * buf, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
            {
                //SIMD_PERF_FUNC();
                size_t size = p.width * channels, cLast = size - 1, fLast = p.width - 1;
                float* cb0 = buf, * cb1 = cb0 + size, * fb0 = cb1 + size, * fb1 = fb0 + p.width, * rb0 = fb1 + p.width;
                for (size_t y = 0; y < p.height; y++)
                {
                    const uint8_t* sl = src, * sr = src + cLast;
                    float* lc = cb0, * rc = cb1 + cLast;
                    float* lf = fb0, * rf = fb1 + fLast;
                    *lf++ = 1.f;
                    *rf-- = 1.f;
                    for (int c = 0; c < channels; c++)
                    {
                        *lc++ = *sl++;
                        *rc-- = *sr--;
                    }
                    RowRanges<channels>(src, src + channels, p.width - 1, p.ranges, rb0 + 1);
                    for (size_t x = 1; x < p.width; x++)
                    {
                        float la = rb0[x];
                        float ra = rb0[p.width - x];
                        *lf++ = p.alpha + la * lf[-1];
                        *rf-- = p.alpha + ra * rf[+1];
                        for (int c = 0; c < channels; c++)
                        {
                            *lc++ = (p.alpha * (*sl++) + la * lc[-channels]);
                            *rc-- = (p.alpha * (*sr--) + ra * rc[+channels]);
                        }
                    }
                    SetOut<channels>(cb0, fb0, cb1, fb1, p.width, dst);
                    src += srcStride;
                    dst += dstStride;
                }
            }

            //-----------------------------------------------------------------------------------------

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

            //-----------------------------------------------------------------------------------------

            template<size_t channels> void VerSetMain(const uint8_t* hor, size_t width,
                float alpha, const float* ranges, const float* pf, const float* pc, float* cf, float* cc);

            template<> void VerSetMain<1>(const uint8_t* hor, size_t width,
                float alpha, const float* ranges, const float* pf, const float* pc, float* cf, float* cc)
            {
                size_t widthF = AlignLo(width, F), x = 0;
                __m128 _alpha = _mm_set1_ps(alpha);
                for (; x < widthF; x += F)
                {
                    __m128 _ranges = _mm_loadu_ps(ranges + x);
                    __m128 _pf = _mm_loadu_ps(pf + x);
                    _mm_storeu_ps(cf + x, _mm_add_ps(_alpha, _mm_mul_ps(_ranges, _pf)));
                    __m128 _pc = _mm_loadu_ps(pc + x);
                    __m128 _hor = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_cvtsi32_si128(*(int32_t*)(hor + x))));
                    _mm_storeu_ps(cc + x, _mm_add_ps(_mm_mul_ps(_alpha, _hor), _mm_mul_ps(_ranges, _pc)));
                }
                for (; x < width; x++)
                {
                    cf[x] = alpha + ranges[x] * pf[x];
                    cc[x] = alpha * hor[x] + ranges[x] * pc[x];
                }
            }

            template<> void VerSetMain<2>(const uint8_t* hor, size_t width,
                float alpha, const float* ranges, const float* pf, const float* pc, float* cf, float* cc)
            {
                size_t widthF = AlignLo(width, F), x = 0, o = 0;
                __m128 _alpha = _mm_set1_ps(alpha);
                for (; x < widthF; x += F, o += DF)
                {
                    __m128 _ranges = _mm_loadu_ps(ranges + x);
                    __m128 _pf = _mm_loadu_ps(pf + x);
                    _mm_storeu_ps(cf + x, _mm_add_ps(_alpha, _mm_mul_ps(_ranges, _pf)));
                    __m128i _hor = _mm_loadu_si128((__m128i*)(hor + o));
                    __m128 pc0 = _mm_loadu_ps(pc + o + 0);
                    __m128 hor0 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_hor));
                    __m128 ranges0 = Shuffle32f<0x50>(_ranges);
                    _mm_storeu_ps(cc + o + 0, _mm_add_ps(_mm_mul_ps(_alpha, hor0), _mm_mul_ps(ranges0, pc0)));
                    __m128 pc1 = _mm_loadu_ps(pc + o + F);
                    __m128 hor1 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_srli_si128(_hor, 4)));
                    __m128 ranges1 = Shuffle32f<0xFA>(_ranges);
                    _mm_storeu_ps(cc + o + F, _mm_add_ps(_mm_mul_ps(_alpha, hor1), _mm_mul_ps(ranges1, pc1)));
                }
                for (; x < width; x++, o += 2)
                {
                    cf[x] = alpha + ranges[x] * pf[x];
                    cc[o + 0] = alpha * hor[o + 0] + ranges[x] * pc[o + 0];
                    cc[o + 1] = alpha * hor[o + 1] + ranges[x] * pc[o + 1];
                }
            }

            template<> void VerSetMain<3>(const uint8_t* hor, size_t width,
                float alpha, const float* ranges, const float* pf, const float* pc, float* cf, float* cc)
            {
                size_t widthF = AlignLo(width, F), x = 0, o = 0;
                __m128 _alpha = _mm_set1_ps(alpha);
                for (; x < widthF; x += F, o += F * 3)
                {
                    __m128 _ranges = _mm_loadu_ps(ranges + x);
                    __m128 _pf = _mm_loadu_ps(pf + x);
                    _mm_storeu_ps(cf + x, _mm_add_ps(_alpha, _mm_mul_ps(_ranges, _pf)));
                    __m128i _hor = _mm_loadu_si128((__m128i*)(hor + o));
                    __m128 pc0 = _mm_loadu_ps(pc + o + 0 * F);
                    __m128 hor0 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_hor));
                    __m128 ranges0 = Shuffle32f<0x40>(_ranges);
                    _mm_storeu_ps(cc + o + 0 * F, _mm_add_ps(_mm_mul_ps(_alpha, hor0), _mm_mul_ps(ranges0, pc0)));
                    __m128 pc1 = _mm_loadu_ps(pc + o + 1 * F);
                    __m128 hor1 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_srli_si128(_hor, 4)));
                    __m128 ranges1 = Shuffle32f<0xA5>(_ranges);
                    _mm_storeu_ps(cc + o + 1 * F, _mm_add_ps(_mm_mul_ps(_alpha, hor1), _mm_mul_ps(ranges1, pc1)));
                    __m128 pc2 = _mm_loadu_ps(pc + o + 2 * F);
                    __m128 hor2 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_srli_si128(_hor, 8)));
                    __m128 ranges2 = Shuffle32f<0xFE>(_ranges);
                    _mm_storeu_ps(cc + o + 2 * F, _mm_add_ps(_mm_mul_ps(_alpha, hor2), _mm_mul_ps(ranges2, pc2)));
                }
                for (; x < width; x++, o += 3)
                {
                    cf[x] = alpha + ranges[x] * pf[x];
                    cc[o + 0] = alpha * hor[o + 0] + ranges[x] * pc[o + 0];
                    cc[o + 1] = alpha * hor[o + 1] + ranges[x] * pc[o + 1];
                    cc[o + 2] = alpha * hor[o + 2] + ranges[x] * pc[o + 2];
                }
            }

            template<> void VerSetMain<4>(const uint8_t* hor, size_t width,
                float alpha, const float* ranges, const float* pf, const float* pc, float* cf, float* cc)
            {
                size_t widthF = AlignLo(width, F), x = 0, o = 0;
                __m128 _alpha = _mm_set1_ps(alpha);
                for (; x < widthF; x += F, o += F * 4)
                {
                    __m128 _ranges = _mm_loadu_ps(ranges + x);
                    __m128 _pf = _mm_loadu_ps(pf + x);
                    _mm_storeu_ps(cf + x, _mm_add_ps(_alpha, _mm_mul_ps(_ranges, _pf)));
                    __m128i _hor = _mm_loadu_si128((__m128i*)(hor + o));
                    __m128 pc0 = _mm_loadu_ps(pc + o + 0 * F);
                    __m128 hor0 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_hor));
                    __m128 ranges0 = Shuffle32f<0x00>(_ranges);
                    _mm_storeu_ps(cc + o + 0 * F, _mm_add_ps(_mm_mul_ps(_alpha, hor0), _mm_mul_ps(ranges0, pc0)));
                    __m128 pc1 = _mm_loadu_ps(pc + o + 1 * F);
                    __m128 hor1 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_srli_si128(_hor, 4)));
                    __m128 ranges1 = Shuffle32f<0x55>(_ranges);
                    _mm_storeu_ps(cc + o + 1 * F, _mm_add_ps(_mm_mul_ps(_alpha, hor1), _mm_mul_ps(ranges1, pc1)));
                    __m128 pc2 = _mm_loadu_ps(pc + o + 2 * F);
                    __m128 hor2 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_srli_si128(_hor, 8)));
                    __m128 ranges2 = Shuffle32f<0xAA>(_ranges);
                    _mm_storeu_ps(cc + o + 2 * F, _mm_add_ps(_mm_mul_ps(_alpha, hor2), _mm_mul_ps(ranges2, pc2)));
                    __m128 pc3 = _mm_loadu_ps(pc + o + 3 * F);
                    __m128 hor3 = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_srli_si128(_hor, 12)));
                    __m128 ranges3 = Shuffle32f<0xFF>(_ranges);
                    _mm_storeu_ps(cc + o + 3 * F, _mm_add_ps(_mm_mul_ps(_alpha, hor3), _mm_mul_ps(ranges3, pc3)));
                }
                for (; x < width; x++, o += 4)
                {
                    __m128 _ranges = _mm_set1_ps(ranges[x]);
                    _mm_store_ss(cf + x, _mm_add_ss(_alpha, _mm_mul_ss(_ranges, _mm_load_ss(pf + x))));
                    __m128 _hor = _mm_cvtepi32_ps(_mm_cvtepu8_epi32(_mm_loadu_si128((__m128i*)(hor + o))));
                    __m128 _pc = _mm_loadu_ps(pc + o);
                    _mm_storeu_ps(cc + o, _mm_add_ps(_mm_mul_ps(_alpha, _hor), _mm_mul_ps(_ranges, _pc)));
                }
            }

            //-----------------------------------------------------------------------------------------

            template<size_t channels> void VerFilter(const RbfParam& p, float * buf, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
            {
                //SIMD_PERF_FUNC();
                size_t size = p.width * channels, srcTail = srcStride - size, dstTail = dstStride - size;
                float* rb0 = buf, * dcb = rb0 + p.width, * dfb = dcb + size * 2, * ucb = dfb + p.width * 2, * ufb = ucb + size * p.height;

                const uint8_t* suc = src + srcStride * (p.height - 1);
                const uint8_t* duc = dst + dstStride * (p.height - 1);
                float* uf = ufb + p.width * (p.height - 1);
                float* uc = ucb + size * (p.height - 1);
                VerSetEdge<channels>(duc, p.width, uf, uc);
                for (size_t y = 1; y < p.height; y++)
                {
                    duc -= dstStride;
                    suc -= srcStride;
                    uf -= p.width;
                    uc -= size;
                    RowRanges<channels>(suc, suc + srcStride, p.width, p.ranges, rb0);
                    VerSetMain<channels>(duc, p.width, p.alpha, rb0, uf + p.width, uc + size, uf, uc);
                }

                VerSetEdge<channels>(dst, p.width, dfb, dcb);
                SetOut<channels>(dcb, dfb, ucb, ufb, p.width, dst);
                for (size_t y = 1; y < p.height; y++)
                {
                    src += srcStride;
                    dst += dstStride;
                    float* dc = dcb + (y & 1) * size;
                    float* df = dfb + (y & 1) * p.width;
                    const float* dpc = dcb + ((y - 1) & 1) * size;
                    const float* dpf = dfb + ((y - 1) & 1) * p.width;
                    RowRanges<channels>(src, src - srcStride, p.width, p.ranges, rb0);
                    VerSetMain<channels>(dst, p.width, p.alpha, rb0, dpf, dpc, df, dc);
                    SetOut<channels>(dc, df, ucb + y * size, ufb + y * p.width, p.width, dst);
                }
            }
        }

		//-----------------------------------------------------------------------------------------

        RecursiveBilateralFilterPrecize::RecursiveBilateralFilterPrecize(const RbfParam& param)
            :Base::RecursiveBilateralFilterPrecize(param)
        {
            if (_param.width * _param.channels >= A)
            {
                switch (_param.channels)
                {
                case 1: _hFilter = Prec::HorFilter<1>; _vFilter = Prec::VerFilter<1>; break;
                case 2: _hFilter = Prec::HorFilter<2>; _vFilter = Prec::VerFilter<2>; break;
                case 3: _hFilter = Prec::HorFilter<3>; _vFilter = Prec::VerFilter<3>; break;
                case 4: _hFilter = Prec::HorFilter<4>; _vFilter = Prec::VerFilter<4>; break;
                default:
                    assert(0);
                }
            }
        }

        //-----------------------------------------------------------------------------------------

        namespace Fast
        {
            template<int add> SIMD_INLINE void Set(float factor, float color, uint8_t* dst);

            template<> SIMD_INLINE void Set<0>(float factor, float color, uint8_t* dst)
            {
                dst[0] = uint8_t(color / factor);
            }

            template<> SIMD_INLINE void Set<1>(float factor, float color, uint8_t* dst)
            {
                dst[0] = uint8_t((int(color / factor) + dst[0] + 1) / 2);
            }

            template<size_t channels, int add> SIMD_INLINE void SetOut(const float* factor, const float* colors, size_t width, uint8_t* dst)
            {
                for (size_t x = 0, o = 0; x < width; x++)
                {
                    for (size_t e = o + channels; o < e; o++)
                        Set<add>(factor[x], colors[o], dst + o);
                }
            }

            template<size_t channels> void HorFilter(const RbfParam& p, float* buf, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
            {
                size_t size = p.width * channels, cLast = size - 1, fLast = p.width - 1;
                for (size_t y = 0; y < p.height; y++)
                {
                    const uint8_t* sl = src;
                    float* lc = buf, * lf = buf + size;
                    *lf++ = 1.f;
                    for (int c = 0; c < channels; c++)
                        *lc++ = *sl++;
                    for (size_t x = 1; x < p.width; x += 1)
                    {
                        float la = p.ranges[Base::Diff<channels>(sl, sl - channels)];
                        *lf++ = p.alpha + la * lf[-1];
                        for (int c = 0; c < channels; c++)
                            *lc++ = (p.alpha * (*sl++) + la * lc[-channels]);
                    }
                    SetOut<channels, 0>(buf + size, buf, p.width, dst);

                    const uint8_t* sr = src + cLast;
                    float* rc = buf + cLast, * rf = buf + size + fLast;
                    *rf-- = 1.f;
                    for (int c = 0; c < channels; c++)
                        *rc-- = *sr--;
                    for (size_t x = 1; x < p.width; x++)
                    {
                        float ra = p.ranges[Base::Diff<channels>(sr + 1, sr - channels + 1)];
                        *rf-- = p.alpha + ra * rf[+1];
                        for (int c = 0; c < channels; c++)
                            *rc-- = (p.alpha * (*sr--) + ra * rc[+channels]);
                    }
                    SetOut<channels, 1>(buf + size, buf, p.width, dst);
                    src += srcStride;
                    dst += dstStride;
                }
            }

            template<size_t channels> void VerSetEdge(const uint8_t* src, size_t width, float* factor, float* colors)
            {
                for (size_t x = 0; x < width; x++)
                    factor[x] = 1.0f;
                for (size_t i = 0, n = width * channels; i < n; i++)
                    colors[i] = src[i];
            }

            template<size_t channels> void VerSetMain(const uint8_t* src0, const uint8_t* src1, size_t width, float alpha,
                const float* ranges, float* factor, float* colors)
            {
                for (size_t x = 0, o = 0; x < width; x++)
                {
                    float ua = ranges[Base::Diff<channels>(src0 + o, src1 + o)];
                    factor[x] = alpha + ua * factor[x];
                    for (size_t e = o + channels; o < e; o++)
                        colors[o] = alpha * src0[o] + ua * colors[o];
                }
            }

            template<size_t channels> void VerFilter(const RbfParam& p, float* buf, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
            {
                size_t size = p.width * channels;

                const uint8_t* suc = src + srcStride * (p.height - 1);
                uint8_t* duc = dst + dstStride * (p.height - 1);
                VerSetEdge<channels>(suc, p.width, buf + size, buf);
                SetOut<channels, 0>(buf + size, buf, p.width, duc);
                for (size_t y = 1; y < p.height; y++)
                {
                    duc -= dstStride;
                    suc -= srcStride;
                    VerSetMain<channels>(suc, suc + srcStride, p.width, p.alpha, p.ranges, buf + size, buf);
                    SetOut<channels, 0>(buf + size, buf, p.width, duc);
                }

                VerSetEdge<channels>(dst, p.width, buf + size, buf);
                SetOut<channels, 1>(buf + size, buf, p.width, duc);
                for (size_t y = 1; y < p.height; y++)
                {
                    src += srcStride;
                    dst += dstStride;
                    VerSetMain<channels>(src, src - srcStride, p.width, p.alpha, p.ranges, buf + size, buf);
                    SetOut<channels, 1>(buf + size, buf, p.width, dst);
                }
            }
        }

        //-----------------------------------------------------------------------------------------

        RecursiveBilateralFilterFast::RecursiveBilateralFilterFast(const RbfParam& param)
            : Base::RecursiveBilateralFilterFast(param)
        {
            switch (_param.channels)
            {
            case 1: _hFilter = Fast::HorFilter<1>; _vFilter = Fast::VerFilter<1>; break;
            case 2: _hFilter = Fast::HorFilter<2>; _vFilter = Fast::VerFilter<2>; break;
            case 3: _hFilter = Fast::HorFilter<3>; _vFilter = Fast::VerFilter<3>; break;
            case 4: _hFilter = Fast::HorFilter<4>; _vFilter = Fast::VerFilter<4>; break;
            default:
                assert(0);
            }
        }

        //-----------------------------------------------------------------------------------------

        void* RecursiveBilateralFilterInit(size_t width, size_t height, size_t channels, 
            const float* sigmaSpatial, const float* sigmaRange, SimdRecursiveBilateralFilterFlags flags)
        {
            RbfParam param(width, height, channels, sigmaSpatial, sigmaRange, flags, A);
            if (!param.Valid())
                return NULL;
            if (Precise(flags))
                return new RecursiveBilateralFilterPrecize(param);
            else
                return new RecursiveBilateralFilterFast(param);
        }
    }
#endif
}

