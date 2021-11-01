/*
 * Copyright 2021 Intel Corporation
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
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>
#include <tuple>
#include <cinttypes>

#include "isl/isl.h"
#include "isl/isl_priv.h"

#define LIN_OFF(y, tw, x) ((y * tw) + x)

enum TILE_CONV {LIN_TO_TILE, TILE_TO_LIN};

#define LIN_TO_TILE_COORD std::make_tuple(0, 128, 0, 32), \
                          std::make_tuple(19, 20, 25, 32), \
                          std::make_tuple(59, 83, 13, 32), \
                          std::make_tuple(10, 12, 5, 8)

#define TILE_TO_LIN_COORD std::make_tuple(0, 128, 0, 32), \
                          std::make_tuple(10, 65, 17, 25), \
                          std::make_tuple(3, 4, 5, 6), \
                          std::make_tuple(64, 88, 5, 8)

class tileTFixture: public ::testing::Test {

protected:
   uint8_t *buf_dst;
   uint8_t *buf_src;
   uint32_t tile_width, tile_height;
   uint32_t tile_sz;
   TILE_CONV conv;
   enum isl_tiling tiling;

public:
   void SetUp(uint32_t tw, uint32_t th,
              TILE_CONV convert, enum isl_tiling format);
   void TearDown();
   uint32_t swizzle_bitops(uint32_t num, uint8_t field,
                           uint8_t curr_ind, uint8_t swizzle_ind);
   void bounded_byte_fill(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2);
   void hex_oword_print(const uint8_t *buf, uint32_t size);
   void convert_texture(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2);
   virtual uint8_t *linear_to_tile_swizzle(const uint8_t *base_addr, uint32_t x_B,
                                           uint32_t y_px) = 0;
   void compare_conv_result(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2);
};

class tileYFixture : public tileTFixture,
                     public ::testing::WithParamInterface<std::tuple<int, int,
                                                                     int, int>>
{

public:
   uint8_t *linear_to_tile_swizzle(const uint8_t *base_addr, uint32_t x_B,
                                   uint32_t y_px);
};

class tile4Fixture : public tileTFixture,
                     public ::testing::WithParamInterface<std::tuple<int, int,
                                                                     int, int>>
{

public:
   uint8_t *linear_to_tile_swizzle(const uint8_t *base_addr, uint32_t x_B,
                                   uint32_t y_px);
};

void tileTFixture::SetUp(uint32_t tw, uint32_t th,
                         TILE_CONV convert, enum isl_tiling format)
{
   conv = convert;
   tiling = format;

   tile_width = tw;
   tile_height = th;
   tile_sz = tile_width * tile_height;

   buf_src = (uint8_t *) std::calloc(tile_sz, sizeof(uint8_t));
   ASSERT_TRUE(buf_src != nullptr);

   buf_dst = (uint8_t *) std::calloc(tile_sz, sizeof(uint8_t));
   ASSERT_TRUE(buf_src != nullptr);
}

void tileTFixture::TearDown()
{
   free(buf_src);
   buf_src = nullptr;

   free(buf_dst);
   buf_dst = nullptr;
}

uint32_t tileTFixture::swizzle_bitops(uint32_t num, uint8_t field,
               uint8_t curr_ind, uint8_t swizzle_ind)
{
   uint32_t bitmask = (1 << field) - 1;
   uint32_t maskednum = num & (bitmask << curr_ind);
   uint32_t bits = maskednum >> curr_ind;
   return bits << swizzle_ind;
}

void tileTFixture::bounded_byte_fill(uint8_t x1, uint8_t x2,
                                     uint8_t y1, uint8_t y2)
{
   uint8_t *itr = (uint8_t *) buf_src;

   for(auto y = y1; y < y2; y++)
      for (auto x = x1; x < x2; x++)
         if (conv == LIN_TO_TILE)
            *(itr + LIN_OFF(y, tile_width, x)) = LIN_OFF(y, tile_width, x)/16;
         else
            *(linear_to_tile_swizzle(buf_src, x, y)) = LIN_OFF(y, tile_width, x)/16;
}


void tileTFixture::hex_oword_print(const uint8_t *buf, uint32_t size)
{
   uint64_t *itr;
   uint32_t i;

   for (itr = (uint64_t *)buf, i=0; itr < (uint64_t *)(buf + size); i++) {
      std::cout << std::setfill('0') << std::setw(16);
      std::cout << std::hex <<__builtin_bswap64(*(itr));
      std::cout << std::setfill('0') << std::setw(16);
      std::cout << std::hex <<__builtin_bswap64(*(itr+1));

      itr = itr+2;

      if((i+1) % 8 == 0 && i > 0)
         std::cout << std::endl;
      else
         std::cout << "  ";
   }
}

uint8_t *tileYFixture::linear_to_tile_swizzle(const uint8_t *base_addr, uint32_t x_B,
                                              uint32_t y_px)
{
   /* The table below represents the mapping from coordinate (x_B, y_px) to
    * byte offset in a 128x32px 1Bpp image:
    *
    *    Bit ind : 11 10  9  8  7  6  5  4  3  2  1  0
    *     Tile-Y : u6 u5 u4 v4 v3 v2 v1 v0 u3 u2 u1 u0
    */
   uint32_t tiled_off;

   tiled_off = swizzle_bitops(x_B, 4, 0, 0) |
               swizzle_bitops(y_px, 5, 0, 4) |
               swizzle_bitops(x_B, 3, 4, 9);

   return (uint8_t *)(base_addr + tiled_off);
}

uint8_t *tile4Fixture::linear_to_tile_swizzle(const uint8_t * base_addr, uint32_t x_B,
                                               uint32_t y_px)
{
   /* The table below represents the mapping from coordinate (x_B, y_px) to
    * byte offset in a 128x32px 1Bpp image:
    *
    *    Bit ind : 11 10  9  8  7  6  5  4  3  2  1  0
    *     Tile-Y : v4 v3 u6 v2 u5 u4 v1 v0 u3 u2 u1 u0
    */
   uint32_t tiled_off;

   tiled_off = swizzle_bitops(x_B, 4, 0, 0) |
               swizzle_bitops(y_px, 2, 0, 4) |
               swizzle_bitops(x_B, 2, 4, 6) |
               swizzle_bitops(y_px, 1, 2, 8) |
               swizzle_bitops(x_B, 1, 6, 9) | swizzle_bitops(y_px, 2, 3, 10);

   return (uint8_t *) (base_addr + tiled_off);
}

void tileTFixture::convert_texture(uint8_t x1, uint8_t x2, uint8_t y1, uint8_t y2)
{

#ifdef DEBUG
   std::cout << "************** Printing src ***************" << std::endl;
   hex_oword_print((const uint8_t *)buf_src, tile_sz);
#endif

   if (conv == LIN_TO_TILE)
      isl_memcpy_linear_to_tiled(x1, x2, y1, y2,
                                 (char *)buf_dst,
                                 (const char *)buf_src + LIN_OFF(y1, tile_width, x1),
                                 tile_width, tile_width,
                                 0, tiling, ISL_MEMCPY);
   else
      isl_memcpy_tiled_to_linear(x1, x2, y1, y2,
                                 (char *)buf_dst + LIN_OFF(y1, tile_width, x1),
                                 (const char *)buf_src,
                                 tile_width, tile_width,
                                 0, tiling, ISL_MEMCPY);

#ifdef DEBUG
   std::cout << "************** Printing dest **************" << std::endl;
   hex_oword_print((const uint8_t *) buf_dst, tile_sz);
#endif

}

void tileTFixture::compare_conv_result(uint8_t x1, uint8_t x2,
                                       uint8_t y1, uint8_t y2)
{

   for(auto y = y1; y < y2; y++) {
      for (auto x = x1; x < x2; x++) {

         if (conv == LIN_TO_TILE)
            EXPECT_EQ(*(buf_src + LIN_OFF(y, tile_width, x)),
                      *(linear_to_tile_swizzle(buf_dst, x, y)))
                      << "Not matching for x:" << x << "and y:" << y << std::endl;
         else
            EXPECT_EQ(*(buf_dst + LIN_OFF(y, tile_width, x)),
                      *(linear_to_tile_swizzle(buf_src, x, y)))
                      << "Not matching for x:" << x << "and y:" << y << std::endl;

      }
   }
}

TEST_P(tileYFixture, lintotile)
{
    auto [x1, x2, y1, y2] = GetParam();
    std::cout << std::dec << x1 << " " << x2 << " " << y1 << " " << y2 << " " << std::endl;
    SetUp(128, 32, LIN_TO_TILE, ISL_TILING_Y0);
    bounded_byte_fill(x1, x2, y1, y2);
    convert_texture(x1, x2, y1, y2);
    compare_conv_result(x1, x2, y1, y2);
}

TEST_P(tileYFixture, tiletolin)
{
    auto [x1, x2, y1, y2] = GetParam();
    std::cout << std::dec << x1 << " " << x2 << " " << y1 << " " << y2 << " " << std::endl;
    SetUp(128, 32, TILE_TO_LIN, ISL_TILING_Y0);
    bounded_byte_fill(x1, x2, y1, y2);
    convert_texture(x1, x2, y1, y2);
    compare_conv_result(x1, x2, y1, y2);
}

TEST_P(tile4Fixture, lintotile)
{
    auto [x1, x2, y1, y2] = GetParam();
    std::cout << std::dec << x1 << " " << x2 << " " << y1 << " " << y2 << " " << std::endl;
    SetUp(128, 32, LIN_TO_TILE, ISL_TILING_4);
    bounded_byte_fill(x1, x2, y1, y2);
    convert_texture(x1, x2, y1, y2);
    compare_conv_result(x1, x2, y1, y2);
}

TEST_P(tile4Fixture, tiletolin)
{
    auto [x1, x2, y1, y2] = GetParam();
    std::cout << std::dec << x1 << " " << x2 << " " << y1 << " " << y2 << " " << std::endl;
    SetUp(128, 32, TILE_TO_LIN, ISL_TILING_4);
    bounded_byte_fill(x1, x2, y1, y2);
    convert_texture(x1, x2, y1, y2);
    compare_conv_result(x1, x2, y1, y2);
}

INSTANTIATE_TEST_CASE_P(Ytile, tileYFixture, testing::Values(LIN_TO_TILE_COORD));
INSTANTIATE_TEST_CASE_P(tile4, tile4Fixture, testing::Values(LIN_TO_TILE_COORD));
