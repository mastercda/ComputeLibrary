/*
 * Copyright (c) 2016, 2017 ARM Limited.
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "arm_compute/core/NEON/kernels/NEArithmeticSubtractionKernel.h"

#include "arm_compute/core/Error.h"
#include "arm_compute/core/Helpers.h"
#include "arm_compute/core/ITensor.h"
#include "arm_compute/core/NEON/NEFixedPoint.h"
#include "arm_compute/core/TensorInfo.h"
#include "arm_compute/core/Validate.h"

#include <algorithm>
#include <arm_neon.h>
#include <cstdint>
#include <map>
#include <string>

using namespace arm_compute;

namespace arm_compute
{
class Coordinates;
} // namespace arm_compute

namespace
{
void sub_wrap_QS8_QS8_QS8(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const qint8x16_t a = vld1q_qs8(reinterpret_cast<const qint8_t *>(input1.ptr()));
        const qint8x16_t b = vld1q_qs8(reinterpret_cast<const qint8_t *>(input2.ptr()));

        vst1q_qs8(reinterpret_cast<qint8_t *>(output.ptr()), vsubq_qs8(a, b));
    },
    input1, input2, output);
}

void sub_saturate_QS8_QS8_QS8(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const qint8x16_t a = vld1q_qs8(reinterpret_cast<const qint8_t *>(input1.ptr()));
        const qint8x16_t b = vld1q_qs8(reinterpret_cast<const qint8_t *>(input2.ptr()));

        vst1q_qs8(reinterpret_cast<qint8_t *>(output.ptr()), vqsubq_qs8(a, b));
    },
    input1, input2, output);
}

void sub_wrap_U8_U8_U8(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const uint8x16_t ta1 = vld1q_u8(input1.ptr());
        const uint8x16_t ta2 = vld1q_u8(input2.ptr());

        vst1q_u8(output.ptr(), vsubq_u8(ta1, ta2));
    },
    input1, input2, output);
}

void sub_saturate_U8_U8_U8(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const uint8x16_t ta1 = vld1q_u8(input1.ptr());
        const uint8x16_t ta2 = vld1q_u8(input2.ptr());

        vst1q_u8(output.ptr(), vqsubq_u8(ta1, ta2));
    },
    input1, input2, output);
}

void sub_wrap_S16_S16_S16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const int16x8x2_t ta1 = vld2q_s16(reinterpret_cast<const int16_t *>(input1.ptr()));
        const int16x8x2_t ta2 = vld2q_s16(reinterpret_cast<const int16_t *>(input2.ptr()));

        const int16x8x2_t ta3 =
        {
            {
                vsubq_s16(ta1.val[0], ta2.val[0]),
                vsubq_s16(ta1.val[1], ta2.val[1])
            }
        };

        vst2q_s16(reinterpret_cast<int16_t *>(output.ptr()), ta3);
    },
    input1, input2, output);
}

void sub_saturate_S16_S16_S16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const int16x8x2_t ta1 = vld2q_s16(reinterpret_cast<const int16_t *>(input1.ptr()));
        const int16x8x2_t ta2 = vld2q_s16(reinterpret_cast<const int16_t *>(input2.ptr()));

        const int16x8x2_t ta3 =
        {
            {
                vqsubq_s16(ta1.val[0], ta2.val[0]),
                vqsubq_s16(ta1.val[1], ta2.val[1])
            }
        };

        vst2q_s16(reinterpret_cast<int16_t *>(output.ptr()), ta3);
    },
    input1, input2, output);
}

#ifdef ARM_COMPUTE_ENABLE_FP16
inline float16x8x2_t vsub2q_f16(const float16x8x2_t &a, const float16x8x2_t &b)
{
    const float16x8x2_t res =
    {
        {
            vsubq_f16(a.val[0], b.val[0]),
            vsubq_f16(a.val[1], b.val[1])
        }
    };

    return res;
}
#endif /* ARM_COMPUTE_ENABLE_FP16 */

void sub_F16_F16_F16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
#ifdef ARM_COMPUTE_ENABLE_FP16
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const float16x8x2_t a = vld2q_f16(reinterpret_cast<const float16_t *>(input1.ptr()));
        const float16x8x2_t b = vld2q_f16(reinterpret_cast<const float16_t *>(input2.ptr()));

        vst2q_f16(reinterpret_cast<float16_t *>(output.ptr()), vsub2q_f16(a, b));
    },
    input1, input2, output);
#else  /* ARM_COMPUTE_ENABLE_FP16 */
    ARM_COMPUTE_UNUSED(in1);
    ARM_COMPUTE_UNUSED(in2);
    ARM_COMPUTE_UNUSED(out);
    ARM_COMPUTE_UNUSED(window);
    ARM_COMPUTE_ERROR("Not supported, recompile the library with arch=arm64-v8.2-a");
#endif /* ARM_COMPUTE_ENABLE_FP16 */
}

void sub_F32_F32_F32(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const float32x4x4_t ta1 = vld4q_f32(reinterpret_cast<const float *>(input1.ptr()));
        const float32x4x4_t ta2 = vld4q_f32(reinterpret_cast<const float *>(input2.ptr()));

        const float32x4x4_t ta3 =
        {
            {
                vsubq_f32(ta1.val[0], ta2.val[0]),
                vsubq_f32(ta1.val[1], ta2.val[1]),
                vsubq_f32(ta1.val[2], ta2.val[2]),
                vsubq_f32(ta1.val[3], ta2.val[3]),
            }
        };

        vst4q_f32(reinterpret_cast<float *>(output.ptr()), ta3);
    },
    input1, input2, output);
}
void sub_wrap_S16_U8_S16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const uint8x16_t bv_0 = vld1q_u8(input2.ptr());
        int16x8_t        a1_0 = vld1q_s16(reinterpret_cast<const int16_t *>(input1.ptr()));
        int16x8_t        a2_0 = vld1q_s16(reinterpret_cast<const int16_t *>(input1.ptr()) + 8);

        a1_0 = vsubq_s16(a1_0, vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(bv_0))));
        a2_0 = vsubq_s16(a2_0, vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(bv_0))));

        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()), a1_0);
        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()) + 8, a2_0);
    },
    input1, input2, output);
}

void sub_saturate_S16_U8_S16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const uint8x16_t bv_0 = vld1q_u8(input2.ptr());
        int16x8_t        a1_0 = vld1q_s16(reinterpret_cast<const int16_t *>(input1.ptr()));
        int16x8_t        a2_0 = vld1q_s16(reinterpret_cast<const int16_t *>(input1.ptr()) + 8);

        a1_0 = vqsubq_s16(a1_0, vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(bv_0))));
        a2_0 = vqsubq_s16(a2_0, vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(bv_0))));

        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()), a1_0);
        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()) + 8, a2_0);
    },
    input1, input2, output);
}

void sub_wrap_U8_S16_S16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const uint8x16_t bv_0 = vld1q_u8(input1.ptr());
        int16x8_t        a1_0 = vld1q_s16(reinterpret_cast<const int16_t *>(input2.ptr()));
        int16x8_t        a2_0 = vld1q_s16(reinterpret_cast<const int16_t *>(input2.ptr()) + 8);

        a1_0 = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(bv_0))), a1_0);
        a2_0 = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(bv_0))), a2_0);

        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()), a1_0);
        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()) + 8, a2_0);
    },
    input1, input2, output);
}

void sub_saturate_U8_S16_S16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const uint8x16_t bv_0 = vld1q_u8(input1.ptr());
        int16x8_t        a1_0 = vld1q_s16(reinterpret_cast<const int16_t *>(input2.ptr()));
        int16x8_t        a2_0 = vld1q_s16(reinterpret_cast<const int16_t *>(input2.ptr()) + 8);

        a1_0 = vqsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(bv_0))), a1_0);
        a2_0 = vqsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(bv_0))), a2_0);

        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()), a1_0);
        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()) + 8, a2_0);
    },
    input1, input2, output);
}

void sub_wrap_U8_U8_S16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const uint8x16_t av_0 = vld1q_u8(input1.ptr());
        const uint8x16_t bv_0 = vld1q_u8(input2.ptr());

        const int16x8_t a1_0 = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(av_0))),
                                         vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(bv_0))));
        const int16x8_t a2_0 = vsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(av_0))),
                                         vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(bv_0))));

        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()), a1_0);
        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()) + 8, a2_0);
    },
    input1, input2, output);
}

void sub_saturate_U8_U8_S16(const ITensor *in1, const ITensor *in2, ITensor *out, const Window &window)
{
    Iterator input1(in1, window);
    Iterator input2(in2, window);
    Iterator output(out, window);

    execute_window_loop(window, [&](const Coordinates & id)
    {
        const uint8x16_t av_0 = vld1q_u8(input1.ptr());
        const uint8x16_t bv_0 = vld1q_u8(input2.ptr());

        const int16x8_t a1_0 = vqsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(av_0))),
                                          vreinterpretq_s16_u16(vmovl_u8(vget_low_u8(bv_0))));
        const int16x8_t a2_0 = vqsubq_s16(vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(av_0))),
                                          vreinterpretq_s16_u16(vmovl_u8(vget_high_u8(bv_0))));

        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()), a1_0);
        vst1q_s16(reinterpret_cast<int16_t *>(output.ptr()) + 8, a2_0);
    },
    input1, input2, output);
}
} // namespace

NEArithmeticSubtractionKernel::NEArithmeticSubtractionKernel()
    : _func(nullptr), _input1(nullptr), _input2(nullptr), _output(nullptr)
{
}

void NEArithmeticSubtractionKernel::configure(const ITensor *input1, const ITensor *input2, ITensor *output, ConvertPolicy policy)
{
    ARM_COMPUTE_ERROR_ON_NULLPTR(input1, input2, output);

    // Auto initialize output if not initialized
    {
        set_shape_if_empty(*output->info(), input1->info()->tensor_shape());

        if(input1->info()->data_type() == DataType::S16 || input2->info()->data_type() == DataType::S16)
        {
            set_format_if_unknown(*output->info(), Format::S16);
        }
        else if(input1->info()->data_type() == DataType::F16 || input2->info()->data_type() == DataType::F16)
        {
            set_format_if_unknown(*output->info(), Format::F16);
        }
        else if(input1->info()->data_type() == DataType::F32 || input2->info()->data_type() == DataType::F32)
        {
            set_format_if_unknown(*output->info(), Format::F32);
        }
    }

    ARM_COMPUTE_ERROR_ON_MISMATCHING_SHAPES(input1, input2, output);
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(input1, 1, DataType::QS8, DataType::U8, DataType::QS16, DataType::S16, DataType::F16, DataType::F32);
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(input2, 1, DataType::QS8, DataType::U8, DataType::QS16, DataType::S16, DataType::F16, DataType::F32);
    ARM_COMPUTE_ERROR_ON_DATA_TYPE_CHANNEL_NOT_IN(output, 1, DataType::QS8, DataType::U8, DataType::QS16, DataType::S16, DataType::F16, DataType::F32);
    ARM_COMPUTE_ERROR_ON_MSG(output->info()->data_type() == DataType::U8 && (input1->info()->data_type() != DataType::U8 || input2->info()->data_type() != DataType::U8),
                             "Output can only be U8 if both inputs are U8");
    if(is_data_type_fixed_point(input1->info()->data_type()) || is_data_type_fixed_point(input2->info()->data_type()) || is_data_type_fixed_point(output->info()->data_type()))
    {
        // Check that all data types are the same and all fixed-point positions are the same
        ARM_COMPUTE_ERROR_ON_MISMATCHING_FIXED_POINT(input1, input2, output);
    }

    static std::map<std::string, SubFunction *> map_function =
    {
        { "sub_wrap_QS8_QS8_QS8", &sub_wrap_QS8_QS8_QS8 },
        { "sub_saturate_QS8_QS8_QS8", &sub_saturate_QS8_QS8_QS8 },
        { "sub_wrap_U8_U8_U8", &sub_wrap_U8_U8_U8 },
        { "sub_wrap_U8_U8_S16", &sub_wrap_U8_U8_S16 },
        { "sub_saturate_U8_U8_U8", &sub_saturate_U8_U8_U8 },
        { "sub_saturate_U8_U8_S16", &sub_saturate_U8_U8_S16 },
        { "sub_wrap_U8_S16_S16", &sub_wrap_U8_S16_S16 },
        { "sub_wrap_S16_U8_S16", &sub_wrap_S16_U8_S16 },
        { "sub_saturate_U8_S16_S16", &sub_saturate_U8_S16_S16 },
        { "sub_saturate_S16_U8_S16", &sub_saturate_S16_U8_S16 },
        { "sub_wrap_QS16_QS16_QS16", &sub_wrap_S16_S16_S16 },
        { "sub_saturate_QS16_QS16_QS16", &sub_saturate_S16_S16_S16 },
        { "sub_wrap_S16_S16_S16", &sub_wrap_S16_S16_S16 },
        { "sub_saturate_S16_S16_S16", &sub_saturate_S16_S16_S16 },
        { "sub_wrap_F32_F32_F32", &sub_F32_F32_F32 },
        { "sub_saturate_F32_F32_F32", &sub_F32_F32_F32 },
        { "sub_wrap_F16_F16_F16", &sub_F16_F16_F16 },
        { "sub_saturate_F16_F16_F16", &sub_F16_F16_F16 },

    };

    _input1 = input1;
    _input2 = input2;
    _output = output;

    std::string function_to_call("sub_");
    function_to_call += policy == ConvertPolicy::WRAP ? "wrap_" : "saturate_";
    function_to_call += string_from_data_type(input1->info()->data_type()) + "_";
    function_to_call += string_from_data_type(input2->info()->data_type()) + "_";
    function_to_call += string_from_data_type(output->info()->data_type());

    auto it = map_function.find(function_to_call);

    if(it != map_function.end())
    {
        _func = it->second;
    }
    else
    {
        ARM_COMPUTE_ERROR("You called subtract with the wrong image formats");
    }

    constexpr unsigned int num_elems_processed_per_iteration = 16;

    // Configure kernel window
    Window                 win = calculate_max_window(*input1->info(), Steps(num_elems_processed_per_iteration));
    AccessWindowHorizontal output_access(output->info(), 0, num_elems_processed_per_iteration);

    update_window_and_padding(win,
                              AccessWindowHorizontal(input1->info(), 0, num_elems_processed_per_iteration),
                              AccessWindowHorizontal(input2->info(), 0, num_elems_processed_per_iteration),
                              output_access);

    ValidRegion valid_region = intersect_valid_regions(input1->info()->valid_region(),
                                                       input2->info()->valid_region());

    output_access.set_valid_region(win, valid_region);

    INEKernel::configure(win);
}

void NEArithmeticSubtractionKernel::run(const Window &window, const ThreadInfo &info)
{
    ARM_COMPUTE_UNUSED(info);
    ARM_COMPUTE_ERROR_ON_UNCONFIGURED_KERNEL(this);
    ARM_COMPUTE_ERROR_ON_INVALID_SUBWINDOW(INEKernel::window(), window);
    ARM_COMPUTE_ERROR_ON(_func == nullptr);

    (*_func)(_input1, _input2, _output, window);
}
