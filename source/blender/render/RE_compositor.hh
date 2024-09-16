/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <memory>

namespace blender::realtime_compositor {
class RenderContext;
class Profiler;
}  // namespace blender::realtime_compositor

struct bNodeTree;
struct Render;
struct RenderData;
struct Scene;

/* ------------------------------------------------------------------------------------------------
 * Render Realtime Compositor
 *
 * Implementation of the compositor for final rendering, as opposed to the viewport compositor
 * that is part of the draw manager. The input and output of this is pre-existing RenderResult
 * buffers in scenes, that are uploaded to and read back from the GPU. */

namespace blender::render {
class RealtimeCompositor;
}

/* Execute compositor. */
void RE_compositor_execute(Render &render,
                           const Scene &scene,
                           const RenderData &render_data,
                           const bNodeTree &node_tree,
                           const char *view_name,
                           blender::realtime_compositor::RenderContext *render_context,
                           blender::realtime_compositor::Profiler *profiler);

/* Free compositor caches. */
void RE_compositor_free(Render &render);
