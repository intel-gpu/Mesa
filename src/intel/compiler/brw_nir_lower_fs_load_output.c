/*
 * Copyright © 2025 Intel Corporation
 * SPDX-License-Identifier: MIT
 */

#include "brw_nir.h"
#include "compiler/nir/nir_builder.h"

/**
 * Lower fragment shader output reads into sampler operations.
 */

static bool
brw_nir_lower_fs_load_output_instr(nir_builder *b,
                                   nir_intrinsic_instr *intrin,
                                   void *data)
{
   if (intrin->intrinsic != nir_intrinsic_load_output)
      return false;

   const struct brw_wm_prog_key *key = data;

   const unsigned l = GET_FIELD(nir_intrinsic_base(intrin),
                                BRW_NIR_FRAG_OUTPUT_LOCATION);
   assert(l >= FRAG_RESULT_DATA0);
   const unsigned load_offset = nir_src_as_uint(intrin->src[0]);
   const unsigned target = l - FRAG_RESULT_DATA0 + load_offset;

   /* Only used by Iris that never sets this to SOMETIMES */
   assert(key->multisample_fbo != INTEL_SOMETIMES);

   b->cursor = nir_before_instr(&intrin->instr);

   /* Query the framebuffer size to figure out where the layer index should go
    * in the coordinates. RESINFO returns 0 in the 3rd component for 1D
    * images.
    */
   nir_tex_instr *query = nir_tex_instr_create(b->shader, 0);
   query->op = nir_texop_txs;
   query->sampler_dim = GLSL_SAMPLER_DIM_3D;
   query->is_array = true;
   query->texture_index = target;
   query->dest_type = nir_type_uint32;

   nir_def_init(&query->instr, &query->def, 4, 32);
   nir_builder_instr_insert(b, &query->instr);

   nir_def *coords[3] = {
      nir_f2u32(b, nir_channel(b, nir_load_frag_coord(b), 0)),
      nir_bcsel(b,
                nir_ieq_imm(b, nir_channel(b, &query->def, 2), 0),
                nir_load_layer_id(b),
                nir_f2u32(b, nir_channel(b, nir_load_frag_coord(b), 1))),
      nir_load_layer_id(b),
   };
   nir_def *coord = nir_vec(
      b, coords, key->multisample_fbo == INTEL_NEVER ? 3 : 2);

   nir_def *tex =
      key->multisample_fbo == INTEL_NEVER ?
      nir_build_tex(b, nir_texop_txf, coord,
                    .texture_index = target,
                    .dim = GLSL_SAMPLER_DIM_2D,
                    .is_array = true,
                    .dest_type = nir_type_uint32) :
      nir_build_tex(b, nir_texop_txf_ms, coord,
                    .texture_index = target,
                    .ms_index = nir_load_sample_id(b),
                    .dim = GLSL_SAMPLER_DIM_MS,
                    .dest_type = nir_type_uint32);

   nir_def_replace(&intrin->def, tex);

   return true;
}

bool
brw_nir_lower_fs_load_output(nir_shader *shader,
                             const struct brw_wm_prog_key *key)
{
   return nir_shader_intrinsics_pass(shader,
                                     brw_nir_lower_fs_load_output_instr,
                                     nir_metadata_control_flow,
                                     (void *) key);
}
