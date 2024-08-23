/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edsculpt
 */

#pragma once

#include "BLI_math_vector.hh"
#include "BLI_span.hh"

struct Brush;
struct Depsgraph;
struct Object;
struct Sculpt;
struct SculptPoseIKChainPreview;
struct SculptSession;
namespace blender::bke::pbvh {
class Node;
}

namespace blender::ed::sculpt_paint::pose {

/**
 * Main Brush Function.
 */
void do_pose_brush(const Depsgraph &depsgraph,
                   const Sculpt &sd,
                   Object &ob,
                   Span<bke::pbvh::Node *> nodes);
/**
 * Calculate the pose origin and (Optionally the pose factor)
 * that is used when using the pose brush.
 *
 * \param r_pose_origin: Must be a valid pointer.
 * \param r_pose_factor: Optional, when set to NULL it won't be calculated.
 */
void calc_pose_data(const Depsgraph &depsgraph,
                    Object &ob,
                    SculptSession &ss,
                    const float3 &initial_location,
                    float radius,
                    float pose_offset,
                    float3 &r_pose_origin,
                    MutableSpan<float> r_pose_factor);
void pose_brush_init(const Depsgraph &depsgraph,
                     Object &ob,
                     SculptSession &ss,
                     const Brush &brush);
std::unique_ptr<SculptPoseIKChainPreview> preview_ik_chain_init(const Depsgraph &depsgraph,
                                                                Object &ob,
                                                                SculptSession &ss,
                                                                const Brush &brush,
                                                                const float3 &initial_location,
                                                                float radius);

}  // namespace blender::ed::sculpt_paint::pose
