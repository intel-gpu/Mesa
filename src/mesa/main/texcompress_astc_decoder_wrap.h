/*
 * Copyright © 2022 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef __TEXCOMPRESS_ASTC_DECODER_WRAP_H__
#define __TEXCOMPRESS_ASTC_DECODER_WRAP_H__

#include <stdint.h>

/* C wrapper for Granite::ASTCLutHolder. */

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   void *data;
   uint32_t size;
   struct gl_buffer_object *buf;
   struct gl_texture_object *tex;
} astc_decoder_lut;

typedef struct
{
   astc_decoder_lut color_endpoint;
   astc_decoder_lut color_endpoint_unquant;
   astc_decoder_lut weights;
   astc_decoder_lut weights_unquant;
   astc_decoder_lut trits_quints;

   void *partition_table;
   uint32_t partition_table_width;
   uint32_t partition_table_height;
   struct gl_texture_object *partition_tex;
   struct hash_table *partition_table_hash;
} astc_decoder_lut_holder;

void _mesa_init_astc_decoder_luts(astc_decoder_lut_holder *holder);
void _mesa_init_astc_decoder_partition_table(astc_decoder_lut_holder *holder,
                                             uint32_t block_width,
                                             uint32_t block_height);

#ifdef __cplusplus
}
#endif

#endif
