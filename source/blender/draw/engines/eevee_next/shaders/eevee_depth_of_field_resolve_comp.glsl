/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Recombine Pass: Load separate convolution layer and composite with self
 * slight defocus convolution and in-focus fields.
 *
 * The half-resolution gather methods are fast but lack precision for small CoC areas.
 * To fix this we do a brute-force gather to have a smooth transition between
 * in-focus and defocus regions.
 */

#pragma BLENDER_REQUIRE(eevee_depth_of_field_accumulator_lib.glsl)

/* Workarounds for Metal/AMD issue where atomicMax lead to incorrect results.
 * See #123052 */
#if defined(GPU_METAL)
#  define threadgroup_size (gl_WorkGroupSize.x * gl_WorkGroupSize.y)
shared float array_of_values[threadgroup_size];

/* Only works for 2D thread-groups where the size is a power of 2. */
float parallelMax(const float value)
{
  uint thread_id = gl_LocalInvocationIndex;
  array_of_values[thread_id] = value;
  threadgroup_barrier(mem_flags::mem_threadgroup);

  for (uint i = threadgroup_size; i > 0; i >>= 1) {
    uint half_width = i >> 1;
    if (thread_id < half_width) {
      array_of_values[thread_id] = max(array_of_values[thread_id],
                                       array_of_values[thread_id + half_width]);
    }
    threadgroup_barrier(mem_flags::mem_threadgroup);
  }

  return array_of_values[0];
}
#endif

shared uint shared_max_slight_focus_abs_coc;

/**
 * Returns The max CoC in the Slight Focus range inside this compute tile.
 */
float dof_slight_focus_coc_tile_get(vec2 frag_coord)
{
  float local_abs_max = 0.0;
  /* Sample in a cross (X) pattern. This covers all pixels over the whole tile, as long as
   * dof_max_slight_focus_radius is less than the group size. */
  for (int i = 0; i < 4; i++) {
    vec2 sample_uv = (frag_coord + quad_offsets[i] * 2.0 * dof_max_slight_focus_radius) /
                     vec2(textureSize(color_tx, 0));
    float coc = dof_coc_from_depth(dof_buf, sample_uv, textureLod(depth_tx, sample_uv, 0.0).r);
    coc = clamp(coc, -dof_buf.coc_abs_max, dof_buf.coc_abs_max);
    if (abs(coc) < dof_max_slight_focus_radius) {
      local_abs_max = max(local_abs_max, abs(coc));
    }
  }

#if defined(GPU_METAL) && defined(GPU_ATI)
  return parallelMax(local_abs_max);

#else
  if (gl_LocalInvocationIndex == 0u) {
    shared_max_slight_focus_abs_coc = floatBitsToUint(0.0);
  }
  barrier();
  /* Use atomic reduce operation. */
  atomicMax(shared_max_slight_focus_abs_coc, floatBitsToUint(local_abs_max));
  /* "Broadcast" result across all threads. */
  barrier();

  return uintBitsToFloat(shared_max_slight_focus_abs_coc);
#endif
}

vec3 dof_neighborhood_clamp(vec2 frag_coord, vec3 color, float center_coc, float weight)
{
  /* Stabilize color by clamping with the stable half res neighborhood. */
  vec3 neighbor_min, neighbor_max;
  const vec2 corners[4] = vec2[4](vec2(-1, -1), vec2(1, -1), vec2(-1, 1), vec2(1, 1));
  for (int i = 0; i < 4; i++) {
    /**
     * Visit the 4 half-res texels around (and containing) the full-resolution texel.
     * Here a diagram of a full-screen texel (f) in the bottom left corner of a half res texel.
     * We sample the stable half-resolution texture at the 4 location denoted by (h).
     * ┌───────┬───────┐
     * │     h │     h │
     * │       │       │
     * │       │ f     │
     * ├───────┼───────┤
     * │     h │     h │
     * │       │       │
     * │       │       │
     * └───────┴───────┘
     */
    vec2 uv_sample = ((frag_coord + corners[i]) * 0.5) / vec2(textureSize(stable_color_tx, 0));
    /* Reminder: The content of this buffer is YCoCg + CoC. */
    vec3 ycocg_sample = textureLod(stable_color_tx, uv_sample, 0.0).rgb;
    neighbor_min = (i == 0) ? ycocg_sample : min(neighbor_min, ycocg_sample);
    neighbor_max = (i == 0) ? ycocg_sample : max(neighbor_max, ycocg_sample);
  }
  /* Pad the bounds in the near in focus region to get back a bit of detail. */
  float padding = 0.125 * saturate(1.0 - square(center_coc) / square(8.0));
  neighbor_max += abs(neighbor_min) * padding;
  neighbor_min -= abs(neighbor_min) * padding;
  /* Progressively apply the clamp to avoid harsh transition. Also mask by weight. */
  float fac = saturate(square(max(0.0, abs(center_coc) - 0.5)) * 4.0) * weight;
  /* Clamp in YCoCg space to avoid too much color drift. */
  color = colorspace_YCoCg_from_scene_linear(color);
  color = mix(color, clamp(color, neighbor_min, neighbor_max), fac);
  color = colorspace_scene_linear_from_YCoCg(color);
  return color;
}

void main()
{
  vec2 frag_coord = vec2(gl_GlobalInvocationID.xy) + 0.5;
  ivec2 tile_co = ivec2(frag_coord / float(DOF_TILES_SIZE * 2));

  CocTile coc_tile = dof_coc_tile_load(in_tiles_fg_img, in_tiles_bg_img, tile_co);
  CocTilePrediction prediction = dof_coc_tile_prediction_get(coc_tile);

  vec2 uv = frag_coord / vec2(textureSize(color_tx, 0));
  vec2 uv_halfres = (frag_coord * 0.5) / vec2(textureSize(color_bg_tx, 0));

  float slight_focus_max_coc = 0.0;
  if (prediction.do_slight_focus) {
    slight_focus_max_coc = dof_slight_focus_coc_tile_get(frag_coord);
    prediction.do_slight_focus = slight_focus_max_coc >= 0.5;
    if (prediction.do_slight_focus) {
      prediction.do_focus = false;
    }
  }

  if (prediction.do_focus) {
    float center_coc = (dof_coc_from_depth(dof_buf, uv, textureLod(depth_tx, uv, 0.0).r));
    prediction.do_focus = abs(center_coc) <= 0.5;
  }

  vec4 out_color = vec4(0.0);
  float weight = 0.0;

  vec4 layer_color;
  float layer_weight;

  const vec3 hole_fill_color = vec3(0.2, 0.1, 1.0);
  const vec3 background_color = vec3(0.1, 0.2, 1.0);
  const vec3 slight_focus_color = vec3(1.0, 0.2, 0.1);
  const vec3 focus_color = vec3(1.0, 1.0, 0.1);
  const vec3 foreground_color = vec3(0.2, 1.0, 0.1);

  if (!no_hole_fill_pass && prediction.do_hole_fill) {
    layer_color = textureLod(color_hole_fill_tx, uv_halfres, 0.0);
    layer_weight = textureLod(weight_hole_fill_tx, uv_halfres, 0.0).r;
    if (do_debug_color) {
      layer_color.rgb *= hole_fill_color;
    }
    out_color = layer_color * safe_rcp(layer_weight);
    weight = float(layer_weight > 0.0);
  }

  if (!no_background_pass && prediction.do_background) {
    layer_color = textureLod(color_bg_tx, uv_halfres, 0.0);
    layer_weight = textureLod(weight_bg_tx, uv_halfres, 0.0).r;
    if (do_debug_color) {
      layer_color.rgb *= background_color;
    }
    /* Always prefer background to hole_fill pass. */
    layer_color *= safe_rcp(layer_weight);
    layer_weight = float(layer_weight > 0.0);
    /* Composite background. */
    out_color = out_color * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;
    /* Fill holes with the composited background. */
    out_color *= safe_rcp(weight);
    weight = float(weight > 0.0);
  }

  if (!no_slight_focus_pass && prediction.do_slight_focus) {
    float center_coc;
    dof_slight_focus_gather(depth_tx,
                            color_tx,
                            bokeh_lut_tx,
                            slight_focus_max_coc,
                            layer_color,
                            layer_weight,
                            center_coc);
    if (do_debug_color) {
      layer_color.rgb *= slight_focus_color;
    }

    /* Composite slight defocus. */
    out_color = out_color * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;

    // out_color.rgb = dof_neighborhood_clamp(frag_coord, out_color.rgb, center_coc, layer_weight);
  }

  if (!no_focus_pass && prediction.do_focus) {
    layer_color = colorspace_safe_color(textureLod(color_tx, uv, 0.0));
    layer_weight = 1.0;
    if (do_debug_color) {
      layer_color.rgb *= focus_color;
    }
    /* Composite in focus. */
    out_color = out_color * (1.0 - layer_weight) + layer_color;
    weight = weight * (1.0 - layer_weight) + layer_weight;
  }

  if (!no_foreground_pass && prediction.do_foreground) {
    layer_color = textureLod(color_fg_tx, uv_halfres, 0.0);
    layer_weight = textureLod(weight_fg_tx, uv_halfres, 0.0).r;
    if (do_debug_color) {
      layer_color.rgb *= foreground_color;
    }
    /* Composite foreground. */
    out_color = out_color * (1.0 - layer_weight) + layer_color;
  }

  /* Fix float precision issue in alpha compositing. */
  if (out_color.a > 0.99) {
    out_color.a = 1.0;
  }

  if (debug_resolve_perf && prediction.do_slight_focus) {
    out_color.rgb *= vec3(1.0, 0.1, 0.1);
  }

  imageStore(out_color_img, ivec2(gl_GlobalInvocationID.xy), out_color);
}
