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

#include "texcompress_astc_decoder_wrap.h"
#include "texcompress_astc_decoder.h"

extern "C" void
_mesa_init_astc_decoder_luts(astc_decoder_lut_holder *holder)
{
   auto &luts = Granite::get_astc_luts();

   holder->color_endpoint.size = sizeof(luts.color_endpoint.lut);
   holder->color_endpoint.data = luts.color_endpoint.lut;

   holder->color_endpoint_unquant.size = luts.color_endpoint.unquant_offset;
   holder->color_endpoint_unquant.data = luts.color_endpoint.unquant_lut;

   holder->weights.size = sizeof(luts.weights.lut);
   holder->weights.data = luts.weights.lut;

   holder->weights_unquant.size = luts.weights.unquant_offset;
   holder->weights_unquant.data = luts.weights.unquant_lut;

   holder->trits_quints.size = sizeof(luts.integer.trits_quints);
   holder->trits_quints.data = luts.integer.trits_quints;
}

extern "C" void
_mesa_init_astc_decoder_partition_table(astc_decoder_lut_holder *holder,
                                        uint32_t block_width,
                                        uint32_t block_height)
{
   auto &luts = Granite::get_astc_luts();
   auto &table = luts.get_partition_table(block_width, block_height);

   holder->partition_table_width = table.lut_width;
   holder->partition_table_height = table.lut_height;
   holder->partition_table = table.lut_buffer.data();
}
