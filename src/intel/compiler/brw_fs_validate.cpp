/*
 * Copyright © 2015 Intel Corporation
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

/** @file
 *
 * Implements a pass that validates various invariants of the IR.  The current
 * pass only validates that GRF's uses are sane.  More can be added later.
 */

#include "brw_fs.h"
#include "brw_cfg.h"

#define fsv_assert(assertion)                                           \
   {                                                                    \
      if (!(assertion)) {                                               \
         fprintf(stderr, "ASSERT: Scalar %s validation failed!\n",      \
                 _mesa_shader_stage_to_abbrev(s.stage));                \
         brw_print_instruction(s, inst, stderr);                        \
         fprintf(stderr, "%s:%d: '%s' failed\n", __FILE__, __LINE__, #assertion);  \
         abort();                                                       \
      }                                                                 \
   }

#define fsv_assert_eq(A, B)                                             \
   {                                                                    \
      unsigned a = (A);                                                 \
      unsigned b = (B);                                                 \
      if (a != b) {                                                     \
         fprintf(stderr, "ASSERT: Scalar %s validation failed!\n",      \
                 _mesa_shader_stage_to_abbrev(s.stage));                \
         brw_print_instruction(s, inst, stderr);                        \
         fprintf(stderr, "%s:%d: A == B failed\n", __FILE__, __LINE__); \
         fprintf(stderr, "  A = %s = %u\n", #A, a);                     \
         fprintf(stderr, "  B = %s = %u\n", #B, b);                     \
         abort();                                                       \
      }                                                                 \
   }

#define fsv_assert_ne(A, B)                                             \
   {                                                                    \
      unsigned a = (A);                                                 \
      unsigned b = (B);                                                 \
      if (a == b) {                                                     \
         fprintf(stderr, "ASSERT: Scalar %s validation failed!\n",      \
                 _mesa_shader_stage_to_abbrev(s.stage));                \
         brw_print_instruction(s, inst, stderr);                        \
         fprintf(stderr, "%s:%d: A != B failed\n", __FILE__, __LINE__); \
         fprintf(stderr, "  A = %s = %u\n", #A, a);                     \
         fprintf(stderr, "  B = %s = %u\n", #B, b);                     \
         abort();                                                       \
      }                                                                 \
   }

#define fsv_assert_lte(A, B)                                            \
   {                                                                    \
      unsigned a = (A);                                                 \
      unsigned b = (B);                                                 \
      if (a > b) {                                                      \
         fprintf(stderr, "ASSERT: Scalar %s validation failed!\n",      \
                 _mesa_shader_stage_to_abbrev(s.stage));                \
         brw_print_instruction(s, inst, stderr);                        \
         fprintf(stderr, "%s:%d: A <= B failed\n", __FILE__, __LINE__); \
         fprintf(stderr, "  A = %s = %u\n", #A, a);                     \
         fprintf(stderr, "  B = %s = %u\n", #B, b);                     \
         abort();                                                       \
      }                                                                 \
   }

#ifndef NDEBUG
void
brw_fs_validate(const fs_visitor &s)
{
   const intel_device_info *devinfo = s.devinfo;

   s.cfg->validate(_mesa_shader_stage_to_abbrev(s.stage));

   foreach_block_and_inst (block, fs_inst, inst, s.cfg) {
      switch (inst->opcode) {
      case SHADER_OPCODE_SEND:
         fsv_assert(is_uniform(inst->src[0]) && is_uniform(inst->src[1]));
         break;

      case BRW_OPCODE_MOV:
         fsv_assert(inst->sources == 1);
         break;

      default:
         break;
      }

      /* On Xe2, the "write the accumulator in addition to the explicit
       * destination" bit no longer exists. Try to catch uses of this feature
       * earlier in the process.
       */
      if (devinfo->ver >= 20 && inst->writes_accumulator) {
         fsv_assert(inst->dst.is_accumulator() ||
                    inst->opcode == BRW_OPCODE_ADDC ||
                    inst->opcode == BRW_OPCODE_MACH ||
                    inst->opcode == BRW_OPCODE_SUBB);
      }

      if (inst->is_3src(s.compiler)) {
         const unsigned integer_sources =
            brw_type_is_int(inst->src[0].type) +
            brw_type_is_int(inst->src[1].type) +
            brw_type_is_int(inst->src[2].type);
         const unsigned float_sources =
            brw_type_is_float(inst->src[0].type) +
            brw_type_is_float(inst->src[1].type) +
            brw_type_is_float(inst->src[2].type);

         fsv_assert((integer_sources == 3 && float_sources == 0) ||
                    (integer_sources == 0 && float_sources == 3));

         if (devinfo->ver >= 10) {
            for (unsigned i = 0; i < 3; i++) {
               if (inst->src[i].file == BRW_IMMEDIATE_VALUE)
                  continue;

               switch (inst->src[i].vstride) {
               case BRW_VERTICAL_STRIDE_0:
               case BRW_VERTICAL_STRIDE_4:
               case BRW_VERTICAL_STRIDE_8:
               case BRW_VERTICAL_STRIDE_16:
                  break;

               case BRW_VERTICAL_STRIDE_1:
                  fsv_assert_lte(12, devinfo->ver);
                  break;

               case BRW_VERTICAL_STRIDE_2:
                  fsv_assert_lte(devinfo->ver, 11);
                  break;

               default:
                  fsv_assert(!"invalid vstride");
                  break;
               }
            }
         } else if (s.grf_used != 0) {
            /* Only perform the pre-Gfx10 checks after register allocation has
             * occured.
             *
             * Many passes (e.g., constant copy propagation) will genenerate
             * invalid 3-source instructions with the expectation that later
             * passes (e.g., combine constants) will fix them.
             */
            for (unsigned i = 0; i < 3; i++) {
               fsv_assert_ne(inst->src[i].file, BRW_IMMEDIATE_VALUE);

               /* A stride of 1 (the usual case) or 0, with a special
                * "repctrl" bit, is allowed. The repctrl bit doesn't work for
                * 64-bit datatypes, so if the source type is 64-bit then only
                * a stride of 1 is allowed. From the Broadwell PRM, Volume 7
                * "3D Media GPGPU", page 944:
                *
                *    This is applicable to 32b datatypes and 16b datatype. 64b
                *    datatypes cannot use the replicate control.
                */
               fsv_assert_lte(inst->src[i].vstride, 1);

               if (brw_type_size_bytes(inst->src[i].type) > 4)
                  fsv_assert_eq(inst->src[i].vstride, 1);
            }
         }
      }

      if (inst->dst.file == VGRF) {
         fsv_assert_lte(inst->dst.offset / REG_SIZE + regs_written(inst),
                        s.alloc.sizes[inst->dst.nr]);
      }

      for (unsigned i = 0; i < inst->sources; i++) {
         if (inst->src[i].file == VGRF) {
            fsv_assert_lte(inst->src[i].offset / REG_SIZE + regs_read(inst, i),
                           s.alloc.sizes[inst->src[i].nr]);
         }
      }

      /* Accumulator Registers, bspec 47251:
       *
       * "When destination is accumulator with offset 0, destination
       * horizontal stride must be 1."
       */
      if (intel_needs_workaround(devinfo, 14014617373) &&
          inst->dst.is_accumulator() &&
          phys_subnr(devinfo, inst->dst) == 0) {
         fsv_assert_eq(inst->dst.hstride, 1);
      }

      if (inst->is_math() && intel_needs_workaround(devinfo, 22016140776)) {
         /* Wa_22016140776:
          *
          *    Scalar broadcast on HF math (packed or unpacked) must not be
          *    used.  Compiler must use a mov instruction to expand the scalar
          *    value to a vector before using in a HF (packed or unpacked)
          *    math operation.
          *
          * Since copy propagation knows about this restriction, nothing
          * should be able to generate these invalid source strides. Detect
          * potential problems sooner rather than later.
          */
         for (unsigned i = 0; i < inst->sources; i++) {
            fsv_assert(!is_uniform(inst->src[i]) ||
                       inst->src[i].type != BRW_TYPE_HF);
         }
      }
   }
}

static void
brw_fs_validate_register_region_special_restrictions(const fs_visitor &s)
{
   const intel_device_info *devinfo = s.devinfo;

   foreach_block_and_inst (block, fs_inst, inst, s.cfg) {
      /* "Src0 Restrictions" in "Special Restrictions" in Bspec 56640. */
      if (devinfo->ver >= 20 &&
          inst->sources > 0 &&
          (inst->src[0].file == FIXED_GRF || inst->src[0].file == ARF)) {
         brw_reg &src0 = inst->src[0];
         brw_reg &dst  = inst->dst;

         const unsigned v = 1 << (MAX2(src0.vstride, 1) - 1);
         const unsigned w = 1 << src0.width;
         const unsigned h = 1 << (MAX2(src0.hstride, 1) - 1);

         /* TODO: Handle usage of address register once is available in the backend IR. */
         const bool is_Vx1 = false;
         const bool is_VxH = false;

         const unsigned src0_stride         = w == 1 ? v : h;
         const unsigned src0_uniform_stride = (w == 1) || (h * w == v) || is_Vx1;
         const unsigned dst_stride          = dst.hstride;

         const unsigned src0_size = brw_type_size_bytes(src0.type);
         const unsigned dst_size = brw_type_size_bytes(dst.type);
         const bool dst_dword_aligned = (dst_size >= 4) ||
                                        (dst_size == 2 && (dst.subnr % 2 == 0)) ||
                                        (dst_size == 1 && (dst.subnr % 4 == 0));

         /* The section below follows the pseudo-code in the spec to make
          * easier to verify.
          */
         bool allowed = false;
         if ((dst_size >= 4) ||
             (src0_size >= 4) ||
             (dst_size == 2 && dst_stride > 1) ||
             (dst_size == 1 && dst_stride > 2) ||
             is_VxH) {
            /* One element per DWord channel. */
            allowed = true;

         } else if (src0_uniform_stride || dst_dword_aligned) {
            if (src0_size == 2 && dst_size == 2) {
               if ((src0_stride < 2) ||
                   (src0_stride == 2 && src0_uniform_stride && (dst.subnr % 16 == src0.subnr / 2)))
                  allowed = true;

            } else if (src0_size == 2 && dst_size == 1 && dst_stride == 2) {
               if ((src0_stride < 2) ||
                   (src0_stride == 2 && src0_uniform_stride && (dst.subnr % 32 == src0.subnr)))
                  allowed = true;

            } else if (src0_size == 1 && dst_size == 2) {
               if ((src0_stride < 4) ||
                   (src0_stride == 4 && src0_uniform_stride && ((2 * dst.subnr) % 16 == src0.subnr / 2)) ||
                   (src0_stride == 8 && src0_uniform_stride && ((2 * dst.subnr) % 8 == src0.subnr / 4)))
                  allowed = true;

            } else if (src0_size == 1 && dst_size == 1 && dst_stride == 2) {
               if ((src0_stride < 4) ||
                   (src0_stride == 4 && src0_uniform_stride && (dst.subnr % 32 == src0.subnr / 2)) ||
                   (src0_stride == 8 && src0_uniform_stride && (dst.subnr % 16 == src0.subnr / 4)))
                  allowed = true;

            } else if (src0_size == 1 && dst_size == 1 && dst_stride == 1 && w != 2) {
               if ((src0_stride < 2) ||
                   (src0_stride == 2 && src0_uniform_stride && (dst.subnr % 32 == src0.subnr / 2)) ||
                   (src0_stride == 4 && src0_uniform_stride && (dst.subnr % 16 == src0.subnr / 4)))
                  allowed = true;

            } else if (src0_size == 1 && dst_size == 1 && dst_stride == 1 && w == 2) {
               if ((h == 0 && v < 4) ||
                   (h == 1 && v < 4) ||
                   (h == 2 && v < 2) ||
                   (h == 1 && v == 4 && (dst.subnr % 32 == 2 * (src0.subnr / 4)) && (src0.subnr % 2 == 0)) ||
                   (h == 2 && v == 4 && (dst.subnr % 32 == src0.subnr / 2)) ||
                   (h == 4 && v == 8 && (dst.subnr % 32 == src0.subnr / 4)))
                  allowed = true;
            }
         }

         fsv_assert(allowed && "Invalid register region for source 0.");
      }

      /* "Src1 Restrictions" in "Special Restrictions" in Bspec 56640. */
      if (devinfo->ver >= 20 &&
          inst->sources > 1 &&
          (inst->src[1].file == FIXED_GRF || inst->src[1].file == ARF)) {
         brw_reg &src1 = inst->src[1];
         brw_reg &dst  = inst->dst;

         const unsigned v = 1 << (MAX2(src1.vstride, 1) - 1);
         const unsigned w = 1 << src1.width;
         const unsigned h = 1 << (MAX2(src1.hstride, 1) - 1);

         /* TODO: Handle usage of address register once is available in the backend IR. */
         const bool is_Vx1 = false;

         const unsigned src1_stride         = w == 1 ? v : h;
         const unsigned src1_uniform_stride = (w == 1) || (h * w == v) || is_Vx1;
         const unsigned dst_stride          = dst.hstride;

         const unsigned src1_size = brw_type_size_bytes(src1.type);
         const unsigned dst_size = brw_type_size_bytes(dst.type);
         const bool dst_dword_aligned = (dst_size >= 4) ||
                                        (dst_size == 2 && (dst.subnr % 2 == 0)) ||
                                        (dst_size == 1 && (dst.subnr % 4 == 0));

         /* The section below follows the pseudo-code in the spec to make
          * easier to verify.
          */
         bool allowed = false;
         if ((dst_size >= 4) ||
             (src1_size >= 4) ||
             (dst_size == 2 && dst_stride > 1) ||
             (dst_size == 1 && dst_stride > 2)) {
            /* One element per DWord channel. */
            allowed = true;

         } else if (src1_uniform_stride || dst_dword_aligned) {
            if (src1_size == 2 && dst_size == 2) {
               if ((src1_stride < 2) ||
                   (src1_stride == 2 && src1_uniform_stride && (dst.subnr % 16 == src1.subnr / 2)))
                  allowed = true;

            } else if (src1_size == 2 && dst_size == 1 && dst_stride == 2) {
               if ((src1_stride < 2) ||
                   (src1_stride == 2 && src1_uniform_stride && (dst.subnr % 32 == src1.subnr)))
                  allowed = true;
            }
         }

         fsv_assert(allowed && "Invalid register region for source 1.");
      }
   }
}

void
brw_fs_validate_late(const fs_visitor &s)
{
   brw_fs_validate_register_region_special_restrictions(s);
}

#endif
