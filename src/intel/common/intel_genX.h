/*
 * Copyright (c) 2022 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#ifndef INTEL_GENX_H
#define INTEL_GENX_H

#ifndef GFX_VERx10
#error This file should only be included by genX files.
#endif

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#if GFX_VERx10 >= 125

UNUSED static int
preferred_slm_allocation_size(const struct intel_device_info *devinfo)
{
   /* Wa_14010744585 */
   if (intel_device_info_is_dg2(devinfo) && devinfo->revision == 0)
      return SLM_ENCODES_128K;

   /* Wa_14017341140 */
   if (intel_device_info_is_mtl(devinfo) && devinfo->revision == 0)
      return SLM_ENCODES_96K;

   return 0;
}

#endif

#ifdef __cplusplus
}
#endif

#endif /* INTEL_GENX_H */
