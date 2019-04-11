/*
* Tests for Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2019 Yermalayeu Ihar.
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
#include "Test/TestUtils.h"
#include "Test/TestPerformance.h"
#include "Test/TestData.h"

#include "Simd/SimdResizer.h"

namespace Test
{
    namespace
    {
        struct FuncRB
        {
            typedef void(*FuncPtr)(
                const uint8_t *src, size_t srcWidth, size_t srcHeight, size_t srcStride,
                uint8_t *dst, size_t dstWidth, size_t dstHeight, size_t dstStride, size_t channelCount);

            FuncPtr func;
            String description;

            FuncRB(const FuncPtr & f, const String & d) : func(f), description(d) {}

            void Call(const View & src, View & dst) const
            {
                TEST_PERFORMANCE_TEST(description);
                func(src.data, src.width, src.height, src.stride,
                    dst.data, dst.width, dst.height, dst.stride, View::PixelSize(src.format));
            }
        };
    }

#define ARGS_RB1(format, width, height, k, function1, function2) \
    format, width, height, k, \
    FuncRB(function1.func, function1.description + ColorDescription(format)), \
    FuncRB(function2.func, function2.description + ColorDescription(format))

#define ARGS_RB2(format, src, dst, function1, function2) \
    format, src, dst, \
    FuncRB(function1.func, function1.description + ColorDescription(format)), \
    FuncRB(function2.func, function2.description + ColorDescription(format))

#define FUNC_RB(function) \
    FuncRB(function, std::string(#function))

    bool ResizeAutoTest(View::Format format, int width, int height, double k, const FuncRB & f1, const FuncRB & f2)
    {
        bool result = true;

        TEST_LOG_SS(Info, "Test " << f1.description << " & " << f2.description
            << " [" << size_t(width*k) << ", " << size_t(height*k) << "] -> [" << width << ", " << height << "].");

        View s(size_t(width*k), size_t(height*k), format, NULL, TEST_ALIGN(size_t(k*width)));
        FillRandom(s);

        View d1(width, height, format, NULL, TEST_ALIGN(width));
        View d2(width, height, format, NULL, TEST_ALIGN(width));

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f1.Call(s, d1));

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f2.Call(s, d2));

        result = result && Compare(d1, d2, 0, true, 64);

        return result;
    }

    bool ResizeAutoTest(const FuncRB & f1, const FuncRB & f2)
    {
        bool result = true;

        for (View::Format format = View::Gray8; format <= View::Bgra32; format = View::Format(format + 1))
        {
            result = result && ResizeAutoTest(ARGS_RB1(format, W, H, 0.9, f1, f2));
            result = result && ResizeAutoTest(ARGS_RB1(format, W + O, H - O, 1.3, f1, f2));
            result = result && ResizeAutoTest(ARGS_RB1(format, W - O, H + O, 0.7, f1, f2));
        }

        return result;
    }

    bool ResizeBilinearAutoTest()
    {
        bool result = true;

        result = result && ResizeAutoTest(FUNC_RB(Simd::Base::ResizeBilinear), FUNC_RB(SimdResizeBilinear));

#ifdef SIMD_SSE2_ENABLE
        if (Simd::Sse2::Enable)
            result = result && ResizeAutoTest(FUNC_RB(Simd::Sse2::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif 

#ifdef SIMD_SSSE3_ENABLE
        if (Simd::Ssse3::Enable)
            result = result && ResizeAutoTest(FUNC_RB(Simd::Ssse3::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif 

#ifdef SIMD_AVX2_ENABLE
        if (Simd::Avx2::Enable)
            result = result && ResizeAutoTest(FUNC_RB(Simd::Avx2::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif 

#ifdef SIMD_AVX512BW_ENABLE
        if (Simd::Avx512bw::Enable)
            result = result && ResizeAutoTest(FUNC_RB(Simd::Avx512bw::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif 

#ifdef SIMD_VMX_ENABLE
        if (Simd::Vmx::Enable)
            result = result && ResizeAutoTest(FUNC_RB(Simd::Vmx::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif 

#ifdef SIMD_NEON_ENABLE
        if (Simd::Neon::Enable)
            result = result && ResizeAutoTest(FUNC_RB(Simd::Neon::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif

        return result;
    }

    String ToString(SimdResizeMethodType method)
    {
        switch (method)
        {
        case SimdResizeMethodBilinear: return "B";
        case SimdResizeMethodCaffeInterp: return "C";
        case SimdResizeMethodArea: return "A";
        default: assert(0); return "";
        }
    }

    String ToString(SimdResizeChannelType type)
    {
        switch (type)
        {
        case SimdResizeChannelByte:  return "b";
        case SimdResizeChannelFloat:  return "f";
        default: assert(0); return "";
        }
    }

    namespace
    {
        struct FuncRS
        {
            typedef void*(*FuncPtr)(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);

            FuncPtr func;
            String description;

            FuncRS(const FuncPtr & f, const String & d) : func(f), description(d) {}

            void Update(SimdResizeMethodType m, SimdResizeChannelType t, size_t c)
            {
                std::stringstream ss;
                ss << description;
                ss << "[" << ToString(m) << "-" << ToString(t) << "-" << c << "]";
                description = ss.str();
            }

            void Call(const View & src, View & dst, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method) const
            {
                void * resizer = NULL;
                if(src.format == View::Float)
                    resizer = func(src.width / channels, src.height, dst.width / channels, dst.height, channels, type, method);
                else
                    resizer = func(src.width, src.height, dst.width, dst.height, channels, type, method);
                {
                    TEST_PERFORMANCE_TEST(description);
                    SimdResizerRun(resizer, src.data, src.stride, dst.data, dst.stride);
                }
                SimdRelease(resizer);
            }
        };
    }

#define FUNC_RS(function) \
    FuncRS(function, std::string(#function))

#define TEST_RESIZE_REAL_IMAGE

    bool ResizerAutoTest(SimdResizeMethodType method, SimdResizeChannelType type, int channels, int width, int height, double k, FuncRS f1, FuncRS f2)
    {
        bool result = true;

        f1.Update(method, type, channels);
        f2.Update(method, type, channels);

        TEST_LOG_SS(Info, "Test " << f1.description << " & " << f2.description
            << " [" << size_t(width*k) << ", " << size_t(height*k) << "] -> [" << width << ", " << height << "].");

        View::Format format;
        if (type == SimdResizeChannelFloat)
        {
            format = View::Float;
            width *= channels;
        }
        else if (type == SimdResizeChannelByte)
        {
            switch (channels)
            {
            case 1: format = View::Gray8; break;
            case 2: format = View::Uv16; break;
            case 3: format = View::Bgr24; break;
            case 4: format = View::Bgra32; break;
            default:
                assert(0);
            }
        }
        else
            assert(0);

        View s(size_t(width*k), size_t(height*k), format, NULL, TEST_ALIGN(size_t(k*width)));
        if(format == View::Float)
            FillRandom32f(s);
        else
        {
#ifdef TEST_RESIZE_REAL_IMAGE
            FillPicture(s);
#else
            FillRandom(s);
#endif
        }

        View d1(width, height, format, NULL, TEST_ALIGN(width));
        View d2(width, height, format, NULL, TEST_ALIGN(width));

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f1.Call(s, d1, channels, type, method));

        TEST_EXECUTE_AT_LEAST_MIN_TIME(f2.Call(s, d2, channels, type, method));

        if (format == View::Float)
            result = result && Compare(d1, d2, EPS, true, 64, DifferenceAbsolute);
        else
            result = result && Compare(d1, d2, 0, true, 64);

#ifdef TEST_RESIZE_REAL_IMAGE
        if (format == View::Bgr24)
        {
            s.Save("src.ppm");
            d1.Save("dst.ppm");
        }
#endif

        return result;
    }

    bool ResizerAutoTest(SimdResizeMethodType method, SimdResizeChannelType type, int channels, const FuncRS & f1, const FuncRS & f2)
    {
        bool result = true;

#ifdef TEST_RESIZE_REAL_IMAGE
        result = result && ResizerAutoTest(method, type, channels, W / 3, H / 3, 3.3, f1, f2);
#else
        result = result && ResizerAutoTest(method, type, channels, W, H, 0.9, f1, f2);
        result = result && ResizerAutoTest(method, type, channels, W + O, H - O, 1.3, f1, f2);
        result = result && ResizerAutoTest(method, type, channels, W - O, H + O, 0.7, f1, f2);
#endif

        return result;
    }

    bool ResizerAutoTest(const FuncRS & f1, const FuncRS & f2)
    {
        bool result = true;
#if 0
        for (SimdResizeMethodType method = SimdResizeMethodBilinear; method <= SimdResizeMethodBilinear; method = SimdResizeMethodType(method + 1))
        {
            result = result && ResizerAutoTest(method, SimdResizeChannelByte, 1, f1, f2);
            result = result && ResizerAutoTest(method, SimdResizeChannelByte, 2, f1, f2);
            result = result && ResizerAutoTest(method, SimdResizeChannelByte, 3, f1, f2);
            result = result && ResizerAutoTest(method, SimdResizeChannelByte, 4, f1, f2);
#ifndef __aarch64__
            result = result && ResizerAutoTest(method, SimdResizeChannelFloat, 1, f1, f2);
            result = result && ResizerAutoTest(method, SimdResizeChannelFloat, 3, f1, f2);
#endif
        }
#else
        result = result && ResizerAutoTest(SimdResizeMethodArea, SimdResizeChannelByte, 1, f1, f2);
        result = result && ResizerAutoTest(SimdResizeMethodArea, SimdResizeChannelByte, 2, f1, f2);
        result = result && ResizerAutoTest(SimdResizeMethodArea, SimdResizeChannelByte, 3, f1, f2);
        result = result && ResizerAutoTest(SimdResizeMethodArea, SimdResizeChannelByte, 4, f1, f2);
#endif
        return result;
    }

    bool ResizerAutoTest()
    {
        bool result = true;

        result = result && ResizerAutoTest(FUNC_RS(Simd::Base::ResizerInit), FUNC_RS(SimdResizerInit));

#ifdef SIMD_SSE_ENABLE
        if (Simd::Sse::Enable)
            result = result && ResizerAutoTest(FUNC_RS(Simd::Sse::ResizerInit), FUNC_RS(SimdResizerInit));
#endif 

#ifdef SIMD_AVX_ENABLE
        if (Simd::Avx::Enable)
            result = result && ResizerAutoTest(FUNC_RS(Simd::Avx::ResizerInit), FUNC_RS(SimdResizerInit));
#endif 

#ifdef SIMD_AVX2_ENABLE
        if (Simd::Avx2::Enable)
            result = result && ResizerAutoTest(FUNC_RS(Simd::Avx2::ResizerInit), FUNC_RS(SimdResizerInit));
#endif

#ifdef SIMD_AVX512F_ENABLE
        if (Simd::Avx512f::Enable)
            result = result && ResizerAutoTest(FUNC_RS(Simd::Avx512f::ResizerInit), FUNC_RS(SimdResizerInit));
#endif

#ifdef SIMD_NEON_ENABLE
        if (Simd::Neon::Enable)
            result = result && ResizerAutoTest(FUNC_RS(Simd::Neon::ResizerInit), FUNC_RS(SimdResizerInit));
#endif 

        return result;
    }

    //-----------------------------------------------------------------------

    bool ResizeDataTest(bool create, int width, int height, View::Format format, const FuncRB & f)
    {
        bool result = true;

        Data data(f.description);

        TEST_LOG_SS(Info, (create ? "Create" : "Verify") << " test " << f.description << " [" << width << ", " << height << "].");

        const double k = 0.7;

        View s(size_t(width*k), size_t(height*k), format, NULL, TEST_ALIGN(size_t(k*width)));

        View d1(width, height, format, NULL, TEST_ALIGN(width));
        View d2(width, height, format, NULL, TEST_ALIGN(width));

        if (create)
        {
            FillRandom(s);
            TEST_SAVE(s);

            f.Call(s, d1);

            TEST_SAVE(d1);
        }
        else
        {
            TEST_LOAD(s);

            TEST_LOAD(d1);

            f.Call(s, d2);

            TEST_SAVE(d2);

            result = result && Compare(d1, d2, 0, true, 64);
        }

        return result;
    }

    bool ResizeBilinearDataTest(bool create)
    {
        bool result = true;

        FuncRB f = FUNC_RB(SimdResizeBilinear);
        for (View::Format format = View::Gray8; format <= View::Bgra32; format = View::Format(format + 1))
        {
            result = result && ResizeDataTest(create, DW, DH, format, FuncRB(f.func, f.description + Data::Description(format)));
        }

        return result;
    }

    //-----------------------------------------------------------------------

    bool ResizeSpecialTest(View::Format format, const Size & src, const Size & dst, const FuncRB & f1, const FuncRB & f2)
    {
        bool result = true;

        TEST_LOG_SS(Info, "Test " << f1.description << " & " << f2.description << " [" << src.x << ", " << src.y << "] -> [" << dst.x << ", " << dst.y << "].");

        View s(src.x, src.y, format, NULL, TEST_ALIGN(src.x));
        FillRandom(s);

        View d1(dst.x, dst.y, format, NULL, TEST_ALIGN(dst.x));
        View d2(dst.x, dst.y, format, NULL, TEST_ALIGN(dst.x));

        f1.Call(s, d1);

        f2.Call(s, d2);

        result = result && Compare(d1, d2, 0, true, 64);

        return result;
    }

    bool ResizeSpecialTest(const FuncRB & f1, const FuncRB & f2)
    {
        bool result = true;

        result = result && ResizeSpecialTest(ARGS_RB2(View::Bgr24, Size(1920, 1080), Size(224, 224), f1, f2));
        result = result && ResizeSpecialTest(ARGS_RB2(View::Gray8, Size(352, 240), Size(174, 94), f1, f2));

        for (Size dst(128, 8); dst.x < 144; ++dst.x)
            for (Size src(32, 12); src.x < 512; ++src.x)
                result = result && ResizeSpecialTest(ARGS_RB2(View::Gray8, src, dst, f1, f2));

        return result;
    }

    bool ResizeBilinearSpecialTest()
    {
        bool result = true;

        result = result && ResizeSpecialTest(FUNC_RB(Simd::Base::ResizeBilinear), FUNC_RB(SimdResizeBilinear));

#ifdef SIMD_SSE2_ENABLE
        if (Simd::Sse2::Enable)
            result = result && ResizeSpecialTest(FUNC_RB(Simd::Sse2::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif

#ifdef SIMD_SSSE3_ENABLE
        if (Simd::Ssse3::Enable)
            result = result && ResizeSpecialTest(FUNC_RB(Simd::Ssse3::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif 

#ifdef SIMD_AVX2_ENABLE
        if (Simd::Avx2::Enable)
            result = result && ResizeSpecialTest(FUNC_RB(Simd::Avx2::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif 

#ifdef SIMD_AVX512BW_ENABLE
        if (Simd::Avx512bw::Enable)
            result = result && ResizeSpecialTest(FUNC_RB(Simd::Avx512bw::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif 

#ifdef SIMD_VMX_ENABLE
        if (Simd::Vmx::Enable)
            result = result && ResizeSpecialTest(FUNC_RB(Simd::Vmx::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif

#ifdef SIMD_NEON_ENABLE
        if (Simd::Neon::Enable)
            result = result && ResizeSpecialTest(FUNC_RB(Simd::Neon::ResizeBilinear), FUNC_RB(SimdResizeBilinear));
#endif

        return result;
    }
}
