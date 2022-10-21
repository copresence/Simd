/*
* Tests for Simd Library (http://ermig1979.github.io/Simd).
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
#include "Test/TestCompare.h"
#include "Test/TestPerformance.h"
#include "Test/TestRandom.h"

namespace Test
{
    namespace
    {
        struct Func
        {
            typedef void(*FuncPtr)(const uint8_t * src, size_t width, size_t height, size_t srcStride, uint8_t * bgra, size_t bgraStride, uint8_t alpha);
            FuncPtr func;
            String description;

            Func(const FuncPtr & f, const String & d) : func(f), description(d) {}

            void Call(const View & src, View & bgra, uint8_t alpha) const
            {
                TEST_PERFORMANCE_TEST(description);
                func(src.data, src.width, src.height, src.stride, bgra.data, bgra.stride, alpha);
            }
        };
    }

#define FUNC(func) Func(func, #func)

    bool AnyToBgraAutoTest(int width, int height, View::Format srcType, const Func & f1, const Func & f2)
    {
        bool result = true;

        TEST_LOG_SS(Info, "Test " << f1.description << " & " << f2.description << " for size [" << width << "," << height << "].");

        View src(width, height, srcType, NULL, TEST_ALIGN(width));
        FillRandom(src);

        View dst1(width, height, View::Bgra32, NULL, TEST_ALIGN(width));
        View dst2(width, height, View::Bgra32, NULL, TEST_ALIGN(width));

        uint8_t alpha = 0xFF;

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f1.Call(src, dst1, alpha));

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f2.Call(src, dst2, alpha));

        result = result && Compare(dst1, dst2, 0, true, 64);

        return result;
    }

    bool AnyToBgraAutoTest(View::Format srcType, const Func & f1, const Func & f2)
    {
        bool result = true;

        result = result && AnyToBgraAutoTest(W, H, srcType, f1, f2);
        result = result && AnyToBgraAutoTest(W + O, H - O, srcType, f1, f2);

        return result;
    }

    bool BgrToBgraAutoTest()
    {
        bool result = true;

        result = result && AnyToBgraAutoTest(View::Bgr24, FUNC(Simd::Base::BgrToBgra), FUNC(SimdBgrToBgra));

#ifdef SIMD_SSE41_ENABLE
        if (Simd::Sse41::Enable && W >= Simd::Sse41::A)
            result = result && AnyToBgraAutoTest(View::Bgr24, FUNC(Simd::Sse41::BgrToBgra), FUNC(SimdBgrToBgra));
#endif 

#if defined(SIMD_AVX2_ENABLE) && !defined(SIMD_CLANG_AVX2_BGR_TO_BGRA_ERROR)
        if (Simd::Avx2::Enable && W >= Simd::Avx2::A)
            result = result && AnyToBgraAutoTest(View::Bgr24, FUNC(Simd::Avx2::BgrToBgra), FUNC(SimdBgrToBgra));
#endif 

#ifdef SIMD_AVX512BW_ENABLE
        if (Simd::Avx512bw::Enable)
            result = result && AnyToBgraAutoTest(View::Bgr24, FUNC(Simd::Avx512bw::BgrToBgra), FUNC(SimdBgrToBgra));
#endif 

#ifdef SIMD_VMX_ENABLE
        if (Simd::Vmx::Enable && W >= Simd::Vmx::A)
            result = result && AnyToBgraAutoTest(View::Bgr24, FUNC(Simd::Vmx::BgrToBgra), FUNC(SimdBgrToBgra));
#endif 

#ifdef SIMD_NEON_ENABLE
        if (Simd::Neon::Enable && W >= Simd::Neon::A)
            result = result && AnyToBgraAutoTest(View::Bgr24, FUNC(Simd::Neon::BgrToBgra), FUNC(SimdBgrToBgra));
#endif 

        return result;
    }

    bool GrayToBgraAutoTest()
    {
        bool result = true;

        result = result && AnyToBgraAutoTest(View::Gray8, FUNC(Simd::Base::GrayToBgra), FUNC(SimdGrayToBgra));

#ifdef SIMD_SSE41_ENABLE
        if (Simd::Sse41::Enable && W >= Simd::Sse41::A)
            result = result && AnyToBgraAutoTest(View::Gray8, FUNC(Simd::Sse41::GrayToBgra), FUNC(SimdGrayToBgra));
#endif 

#ifdef SIMD_AVX2_ENABLE
        if (Simd::Avx2::Enable && W >= Simd::Avx2::A)
            result = result && AnyToBgraAutoTest(View::Gray8, FUNC(Simd::Avx2::GrayToBgra), FUNC(SimdGrayToBgra));
#endif 

#ifdef SIMD_AVX512BW_ENABLE
        if (Simd::Avx512bw::Enable)
            result = result && AnyToBgraAutoTest(View::Gray8, FUNC(Simd::Avx512bw::GrayToBgra), FUNC(SimdGrayToBgra));
#endif

#ifdef SIMD_VMX_ENABLE
        if (Simd::Vmx::Enable && W >= Simd::Vmx::A)
            result = result && AnyToBgraAutoTest(View::Gray8, FUNC(Simd::Vmx::GrayToBgra), FUNC(SimdGrayToBgra));
#endif 

#ifdef SIMD_NEON_ENABLE
        if (Simd::Neon::Enable && W >= Simd::Neon::A)
            result = result && AnyToBgraAutoTest(View::Gray8, FUNC(Simd::Neon::GrayToBgra), FUNC(SimdGrayToBgra));
#endif

        return result;
    }

    bool RgbToBgraAutoTest()
    {
        bool result = true;

        result = result && AnyToBgraAutoTest(View::Rgb24, FUNC(Simd::Base::RgbToBgra), FUNC(SimdRgbToBgra));

#ifdef SIMD_SSE41_ENABLE
        if (Simd::Sse41::Enable && W >= Simd::Sse41::A)
            result = result && AnyToBgraAutoTest(View::Rgb24, FUNC(Simd::Sse41::RgbToBgra), FUNC(SimdRgbToBgra));
#endif 

#if defined(SIMD_AVX2_ENABLE) && !defined(SIMD_CLANG_AVX2_BGR_TO_BGRA_ERROR)
        if (Simd::Avx2::Enable && W >= Simd::Avx2::A)
            result = result && AnyToBgraAutoTest(View::Rgb24, FUNC(Simd::Avx2::RgbToBgra), FUNC(SimdRgbToBgra));
#endif 

#ifdef SIMD_AVX512BW_ENABLE
        if (Simd::Avx512bw::Enable)
            result = result && AnyToBgraAutoTest(View::Rgb24, FUNC(Simd::Avx512bw::RgbToBgra), FUNC(SimdRgbToBgra));
#endif 

#ifdef SIMD_NEON_ENABLE
        if (Simd::Neon::Enable && W >= Simd::Neon::A)
            result = result && AnyToBgraAutoTest(View::Rgb24, FUNC(Simd::Neon::RgbToBgra), FUNC(SimdRgbToBgra));
#endif

        return result;
    }
}
