/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_symmetric_separable_blur_variable_size_shared)
    .local_group_size(16, 16)
    .push_constant(Type::BOOL, "is_vertical_pass")
    .sampler(0, ImageType::FLOAT_2D, "input_tx")
    .sampler(1, ImageType::FLOAT_1D, "weights_tx")
    .sampler(2, ImageType::FLOAT_2D, "radius_tx")
    .compute_source("compositor_symmetric_separable_blur_variable_size.glsl");

GPU_SHADER_CREATE_INFO(compositor_symmetric_separable_blur_variable_size_float)
    .additional_info("compositor_symmetric_separable_blur_variable_size_shared")
    .image(0, GPU_R16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_symmetric_separable_blur_variable_size_float2)
    .additional_info("compositor_symmetric_separable_blur_variable_size_shared")
    .image(0, GPU_RG16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(compositor_symmetric_separable_blur_variable_size_float4)
    .additional_info("compositor_symmetric_separable_blur_variable_size_shared")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "output_img")
    .do_static_compilation(true);
