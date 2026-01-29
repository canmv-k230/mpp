/* Copyright (c) 2026, Canaan Bright Sight Co., Ltd
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
 * CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#pragma once

// the marco is copy from https://github.com/GorgonMeducer/PLOOC
#if (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L) || defined(__cplusplus)
/*! \brief You can use __PLOOC_EVAL() to dynamically select the right API which
 *!        has the right number of parameters (no more than 8).
 */
//! @{
#undef __PLOOC_VA_NUM_ARGS_IMPL
#undef __PLOOC_VA_NUM_ARGS
#undef __16_PLOOC_EVAL
#undef __15_PLOOC_EVAL
#undef __14_PLOOC_EVAL
#undef __13_PLOOC_EVAL
#undef __12_PLOOC_EVAL
#undef __11_PLOOC_EVAL
#undef __10_PLOOC_EVAL
#undef __9_PLOOC_EVAL
#undef __8_PLOOC_EVAL
#undef __7_PLOOC_EVAL
#undef __6_PLOOC_EVAL
#undef __5_PLOOC_EVAL
#undef __4_PLOOC_EVAL
#undef __3_PLOOC_EVAL
#undef __2_PLOOC_EVAL
#undef __1_PLOOC_EVAL
#undef __0_PLOOC_EVAL
#undef __PLOOC_EVAL

#define __PLOOC_VA_NUM_ARGS_IMPL(_0, _1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, __N, ...) __N
#define __PLOOC_VA_NUM_ARGS(...)                                                                                               \
    __PLOOC_VA_NUM_ARGS_IMPL(0, ##__VA_ARGS__, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)

#define __16_PLOOC_EVAL(__FUNC, __NO_ARGS) __FUNC##__NO_ARGS
#define __15_PLOOC_EVAL(__FUNC, __NO_ARGS) __16_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __14_PLOOC_EVAL(__FUNC, __NO_ARGS) __15_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __13_PLOOC_EVAL(__FUNC, __NO_ARGS) __14_PLOOC_EVAL(__FUNC, __NO_ARGS)

#define __12_PLOOC_EVAL(__FUNC, __NO_ARGS) __13_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __11_PLOOC_EVAL(__FUNC, __NO_ARGS) __12_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __10_PLOOC_EVAL(__FUNC, __NO_ARGS) __11_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __9_PLOOC_EVAL(__FUNC, __NO_ARGS)  __10_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __8_PLOOC_EVAL(__FUNC, __NO_ARGS)  __9_PLOOC_EVAL(__FUNC, __NO_ARGS)

#define __7_PLOOC_EVAL(__FUNC, __NO_ARGS) __8_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __6_PLOOC_EVAL(__FUNC, __NO_ARGS) __7_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __5_PLOOC_EVAL(__FUNC, __NO_ARGS) __6_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __4_PLOOC_EVAL(__FUNC, __NO_ARGS) __5_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __3_PLOOC_EVAL(__FUNC, __NO_ARGS) __4_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __2_PLOOC_EVAL(__FUNC, __NO_ARGS) __3_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __1_PLOOC_EVAL(__FUNC, __NO_ARGS) __2_PLOOC_EVAL(__FUNC, __NO_ARGS)
#define __0_PLOOC_EVAL(__FUNC, __NO_ARGS) __1_PLOOC_EVAL(__FUNC, __NO_ARGS)

#define __PLOOC_EVAL(__FUNC, ...) __0_PLOOC_EVAL(__FUNC, __PLOOC_VA_NUM_ARGS(__VA_ARGS__))
//! @}
#endif
