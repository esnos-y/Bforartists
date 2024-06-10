/* SPDX-FileCopyrightText: 2013 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <set>

#include "BLI_multi_value_map.hh"

#include "BKE_node_runtime.hh"

#include "COM_Converter.h"
#include "COM_Debug.h"

#include "COM_PreviewOperation.h"
#include "COM_SetColorOperation.h"
#include "COM_SetValueOperation.h"
#include "COM_SetVectorOperation.h"
#include "COM_ViewerOperation.h"

#include "COM_ConstantFolder.h"
#include "COM_NodeOperationBuilder.h" /* own include */

namespace blender::compositor {

NodeOperationBuilder::NodeOperationBuilder(const CompositorContext *context,
                                           bNodeTree *b_nodetree,
                                           ExecutionSystem *system)
    : context_(context), exec_system_(system), current_node_(nullptr), active_viewer_(nullptr)
{
  graph_.from_bNodeTree(*context, b_nodetree);
}

void NodeOperationBuilder::convert_to_operations(ExecutionSystem *system)
{
  /* interface handle for nodes */
  NodeConverter converter(this);

  for (Node *node : graph_.nodes()) {
    current_node_ = node;

    DebugInfo::node_to_operations(node);
    node->convert_to_operations(converter, *context_);
  }

  current_node_ = nullptr;

  /* The input map constructed by nodes maps operation inputs to node inputs.
   * Inverting yields a map of node inputs to all connected operation inputs,
   * so multiple operations can use the same node input.
   */
  blender::MultiValueMap<NodeInput *, NodeOperationInput *> inverse_input_map;
  for (MutableMapItem<NodeOperationInput *, NodeInput *> item : input_map_.items()) {
    inverse_input_map.add(item.value, item.key);
  }

  for (const NodeGraph::Link &link : graph_.links()) {
    NodeOutput *from = link.from;
    NodeInput *to = link.to;

    NodeOperationOutput *op_from = output_map_.lookup_default(from, nullptr);

    const blender::Span<NodeOperationInput *> op_to_list = inverse_input_map.lookup(to);
    if (!op_from || op_to_list.is_empty()) {
      /* XXX allow this? error/debug message? */
      // BLI_assert(false);
      /* XXX NOTE: this can happen with certain nodes (e.g. OutputFile)
       * which only generate operations in certain circumstances (rendering)
       * just let this pass silently for now ...
       */
      continue;
    }

    for (NodeOperationInput *op_to : op_to_list) {
      add_link(op_from, op_to);
    }
  }

  add_operation_input_constants();

  resolve_proxies();

  add_datatype_conversions();

  save_graphviz("compositor_prior_folding");
  ConstantFolder folder(*this);
  folder.fold_operations();

  determine_canvases();

  save_graphviz("compositor_prior_merging");
  merge_equal_operations();

  /* links not available from here on */
  /* XXX make links_ a local variable to avoid confusion! */
  links_.clear();

  prune_operations();

  /* ensure topological (link-based) order of nodes */
  // sort_operations(); /* not needed yet. */

  /* transfer resulting operations to the system */
  system->set_operations(operations_);
}

void NodeOperationBuilder::add_operation(NodeOperation *operation)
{
  operation->set_id(operations_.size());
  operations_.append(operation);
  if (current_node_) {
    operation->set_name(current_node_->get_bnode()->name);
    operation->set_node_instance_key(current_node_->get_instance_key());
  }
  operation->set_execution_system(exec_system_);
}

void NodeOperationBuilder::replace_operation_with_constant(NodeOperation *operation,
                                                           ConstantOperation *constant_operation)
{
  BLI_assert(constant_operation->get_number_of_input_sockets() == 0);
  unlink_inputs_and_relink_outputs(operation, constant_operation);
  add_operation(constant_operation);
}

void NodeOperationBuilder::unlink_inputs_and_relink_outputs(NodeOperation *unlinked_op,
                                                            NodeOperation *linked_op)
{
  int i = 0;
  while (i < links_.size()) {
    Link &link = links_[i];
    if (&link.to()->get_operation() == unlinked_op) {
      link.to()->set_link(nullptr);
      links_.remove(i);
      continue;
    }

    if (&link.from()->get_operation() == unlinked_op) {
      link.to()->set_link(linked_op->get_output_socket());
      links_[i] = Link(linked_op->get_output_socket(), link.to());
    }
    i++;
  }
}

void NodeOperationBuilder::map_input_socket(NodeInput *node_socket,
                                            NodeOperationInput *operation_socket)
{
  BLI_assert(current_node_);
  BLI_assert(node_socket->get_node() == current_node_);

  /* NOTE: this maps operation sockets to node sockets.
   * for resolving links the map will be inverted first in convert_to_operations,
   * to get a list of links for each node input socket.
   */
  input_map_.add_new(operation_socket, node_socket);
}

void NodeOperationBuilder::map_output_socket(NodeOutput *node_socket,
                                             NodeOperationOutput *operation_socket)
{
  BLI_assert(current_node_);
  BLI_assert(node_socket->get_node() == current_node_);

  output_map_.add_new(node_socket, operation_socket);
}

void NodeOperationBuilder::add_link(NodeOperationOutput *from, NodeOperationInput *to)
{
  if (to->is_connected()) {
    return;
  }

  links_.append(Link(from, to));

  /* register with the input */
  to->set_link(from);
}

void NodeOperationBuilder::remove_input_link(NodeOperationInput *to)
{
  int index = 0;
  for (Link &link : links_) {
    if (link.to() == to) {
      /* unregister with the input */
      to->set_link(nullptr);

      links_.remove(index);
      return;
    }
    index++;
  }
}

PreviewOperation *NodeOperationBuilder::make_preview_operation() const
{
  BLI_assert(current_node_);

  if (!(current_node_->get_bnode()->flag & NODE_PREVIEW)) {
    return nullptr;
  }
  /* previews only in the active group */
  if (!current_node_->is_in_active_group()) {
    return nullptr;
  }
  /* do not calculate previews of hidden nodes */
  if (current_node_->get_bnode()->flag & NODE_HIDDEN) {
    return nullptr;
  }

  bke::bNodeInstanceHash *previews = context_->get_preview_hash();
  if (previews) {
    Scene *scene = context_->get_scene();
    PreviewOperation *operation = new PreviewOperation(
        &scene->view_settings,
        &scene->display_settings,
        current_node_->get_bnode()->runtime->preview_xsize,
        current_node_->get_bnode()->runtime->preview_ysize);
    operation->set_bnodetree(context_->get_bnodetree());
    operation->verify_preview(previews, current_node_->get_instance_key());
    return operation;
  }

  return nullptr;
}

void NodeOperationBuilder::add_preview(NodeOperationOutput *output)
{
  PreviewOperation *operation = make_preview_operation();
  if (operation) {
    add_operation(operation);

    add_link(output, operation->get_input_socket(0));
  }
}

void NodeOperationBuilder::add_node_input_preview(NodeInput *input)
{
  PreviewOperation *operation = make_preview_operation();
  if (operation) {
    add_operation(operation);

    map_input_socket(input, operation->get_input_socket(0));
  }
}

void NodeOperationBuilder::register_viewer(ViewerOperation *viewer)
{
  if (!active_viewer_) {
    active_viewer_ = viewer;
    viewer->set_active(true);
    return;
  }

  /* A viewer is already registered, so we active this viewer but only if it is in the active node
   * tree, since it takes precedence over viewer nodes in other trees. So deactivate existing
   * viewer and set this viewer as active. */
  if (current_node_->is_in_active_group()) {
    active_viewer_->set_active(false);

    active_viewer_ = viewer;
    viewer->set_active(true);
  }
}

/****************************
 **** Optimization Steps ****
 ****************************/

void NodeOperationBuilder::add_datatype_conversions()
{
  Vector<Link> convert_links;
  for (const Link &link : links_) {
    /* proxy operations can skip data type conversion */
    NodeOperation *from_op = &link.from()->get_operation();
    NodeOperation *to_op = &link.to()->get_operation();
    if (!(from_op->get_flags().use_datatype_conversion ||
          to_op->get_flags().use_datatype_conversion))
    {
      continue;
    }

    if (link.from()->get_data_type() != link.to()->get_data_type()) {
      convert_links.append(link);
    }
  }
  for (const Link &link : convert_links) {
    NodeOperation *converter = COM_convert_data_type(*link.from(), *link.to());
    if (converter) {
      add_operation(converter);

      remove_input_link(link.to());
      add_link(link.from(), converter->get_input_socket(0));
      add_link(converter->get_output_socket(0), link.to());
    }
  }
}

void NodeOperationBuilder::add_operation_input_constants()
{
  /* NOTE: unconnected inputs cached first to avoid modifying
   *       operations_ while iterating over it
   */
  Vector<NodeOperationInput *> pending_inputs;
  for (NodeOperation *op : operations_) {
    for (int k = 0; k < op->get_number_of_input_sockets(); ++k) {
      NodeOperationInput *input = op->get_input_socket(k);
      if (!input->is_connected()) {
        pending_inputs.append(input);
      }
    }
  }
  for (NodeOperationInput *input : pending_inputs) {
    add_input_constant_value(input, input_map_.lookup_default(input, nullptr));
  }
}

void NodeOperationBuilder::add_input_constant_value(NodeOperationInput *input,
                                                    const NodeInput *node_input)
{
  switch (input->get_data_type()) {
    case DataType::Value: {
      float value;
      if (node_input && node_input->get_bnode_socket()) {
        value = node_input->get_editor_value_float();
      }
      else {
        value = 0.0f;
      }

      SetValueOperation *op = new SetValueOperation();
      op->set_value(value);
      add_operation(op);
      add_link(op->get_output_socket(), input);
      break;
    }
    case DataType::Color: {
      float value[4];
      if (node_input && node_input->get_bnode_socket()) {
        node_input->get_editor_value_color(value);
      }
      else {
        zero_v4(value);
      }

      SetColorOperation *op = new SetColorOperation();
      op->set_channels(value);
      add_operation(op);
      add_link(op->get_output_socket(), input);
      break;
    }
    case DataType::Vector: {
      float value[3];
      if (node_input && node_input->get_bnode_socket()) {
        node_input->get_editor_value_vector(value);
      }
      else {
        zero_v3(value);
      }

      SetVectorOperation *op = new SetVectorOperation();
      op->set_vector(value);
      add_operation(op);
      add_link(op->get_output_socket(), input);
      break;
    }
    case DataType::Float2:
      /* An internal type that needn't be handled. */
      BLI_assert_unreachable();
      break;
  }
}

void NodeOperationBuilder::resolve_proxies()
{
  Vector<Link> proxy_links;
  for (const Link &link : links_) {
    /* don't replace links from proxy to proxy, since we may need them for replacing others! */
    if (link.from()->get_operation().get_flags().is_proxy_operation &&
        !link.to()->get_operation().get_flags().is_proxy_operation)
    {
      proxy_links.append(link);
    }
  }

  for (const Link &link : proxy_links) {
    NodeOperationInput *to = link.to();
    NodeOperationOutput *from = link.from();
    do {
      /* walk upstream bypassing the proxy operation */
      from = from->get_operation().get_input_socket(0)->get_link();
    } while (from && from->get_operation().get_flags().is_proxy_operation);

    remove_input_link(to);
    /* we may not have a final proxy input link,
     * in that case it just gets dropped
     */
    if (from) {
      add_link(from, to);
    }
  }
}

void NodeOperationBuilder::determine_canvases()
{
  /* Determine all canvas areas of the operations. */
  const rcti &preferred_area = COM_AREA_NONE;
  for (NodeOperation *op : operations_) {
    if (op->is_output_operation(context_->is_rendering()) && !op->get_flags().is_preview_operation)
    {
      rcti canvas = COM_AREA_NONE;
      op->determine_canvas(preferred_area, canvas);
      op->set_canvas(canvas);
    }
  }

  for (NodeOperation *op : operations_) {
    if (op->is_output_operation(context_->is_rendering()) && op->get_flags().is_preview_operation)
    {
      rcti canvas = COM_AREA_NONE;
      op->determine_canvas(preferred_area, canvas);
      op->set_canvas(canvas);
    }
  }

  /* Convert operation canvases when needed. */
  {
    Vector<Link> convert_links;
    for (const Link &link : links_) {
      if (link.to()->get_resize_mode() != ResizeMode::None) {
        const rcti &from_canvas = link.from()->get_operation().get_canvas();
        const rcti &to_canvas = link.to()->get_operation().get_canvas();

        bool needs_conversion;
        if (link.to()->get_resize_mode() == ResizeMode::Align) {
          needs_conversion = from_canvas.xmin != to_canvas.xmin ||
                             from_canvas.ymin != to_canvas.ymin;
        }
        else {
          needs_conversion = !BLI_rcti_compare(&from_canvas, &to_canvas);
        }

        if (needs_conversion) {
          convert_links.append(link);
        }
      }
    }
    for (const Link &link : convert_links) {
      COM_convert_canvas(*this, link.from(), link.to());
    }
  }
}

static Vector<NodeOperationHash> generate_hashes(Span<NodeOperation *> operations)
{
  Vector<NodeOperationHash> hashes;
  for (NodeOperation *op : operations) {
    std::optional<NodeOperationHash> hash = op->generate_hash();
    if (hash) {
      hashes.append(std::move(*hash));
    }
  }
  return hashes;
}

void NodeOperationBuilder::merge_equal_operations()
{
  bool check_for_next_merge = true;
  while (check_for_next_merge) {
    /* Re-generate hashes with any change. */
    Vector<NodeOperationHash> hashes = generate_hashes(operations_);

    /* Make hashes be consecutive when they are equal. */
    std::sort(hashes.begin(), hashes.end());

    bool any_merged = false;
    const NodeOperationHash *prev_hash = nullptr;
    for (const NodeOperationHash &hash : hashes) {
      if (prev_hash && *prev_hash == hash) {
        merge_equal_operations(prev_hash->get_operation(), hash.get_operation());
        any_merged = true;
      }
      prev_hash = &hash;
    }

    check_for_next_merge = any_merged;
  }
}

void NodeOperationBuilder::merge_equal_operations(NodeOperation *from, NodeOperation *into)
{
  unlink_inputs_and_relink_outputs(from, into);
  operations_.remove_first_occurrence_and_reorder(from);
  delete from;
}

Vector<NodeOperationInput *> NodeOperationBuilder::cache_output_links(
    NodeOperationOutput *output) const
{
  Vector<NodeOperationInput *> inputs;
  for (const Link &link : links_) {
    if (link.from() == output) {
      inputs.append(link.to());
    }
  }
  return inputs;
}

using Tags = std::set<NodeOperation *>;

static void find_reachable_operations_recursive(Tags &reachable, NodeOperation *op)
{
  if (reachable.find(op) != reachable.end()) {
    return;
  }
  reachable.insert(op);

  for (int i = 0; i < op->get_number_of_input_sockets(); i++) {
    NodeOperationInput *input = op->get_input_socket(i);
    if (input->is_connected()) {
      find_reachable_operations_recursive(reachable, &input->get_link()->get_operation());
    }
  }
}

void NodeOperationBuilder::prune_operations()
{
  Tags reachable;
  for (NodeOperation *op : operations_) {
    /* output operations are primary executed operations */
    if (op->is_output_operation(context_->is_rendering())) {
      find_reachable_operations_recursive(reachable, op);
    }
  }

  /* delete unreachable operations */
  Vector<NodeOperation *> reachable_ops;
  for (NodeOperation *op : operations_) {
    if (reachable.find(op) != reachable.end()) {
      reachable_ops.append(op);
    }
    else {
      delete op;
    }
  }
  /* finally replace the operations list with the pruned list */
  operations_ = reachable_ops;
}

/* topological (depth-first) sorting of operations */
static void sort_operations_recursive(Vector<NodeOperation *> &sorted,
                                      Tags &visited,
                                      NodeOperation *op)
{
  if (visited.find(op) != visited.end()) {
    return;
  }
  visited.insert(op);

  for (int i = 0; i < op->get_number_of_input_sockets(); i++) {
    NodeOperationInput *input = op->get_input_socket(i);
    if (input->is_connected()) {
      sort_operations_recursive(sorted, visited, &input->get_link()->get_operation());
    }
  }

  sorted.append(op);
}

void NodeOperationBuilder::sort_operations()
{
  Vector<NodeOperation *> sorted;
  sorted.reserve(operations_.size());
  Tags visited;

  for (NodeOperation *operation : operations_) {
    sort_operations_recursive(sorted, visited, operation);
  }

  operations_ = sorted;
}

void NodeOperationBuilder::save_graphviz(StringRefNull name)
{
  if (COM_EXPORT_GRAPHVIZ) {
    exec_system_->set_operations(operations_);
    DebugInfo::graphviz(exec_system_, name);
  }
}

std::ostream &operator<<(std::ostream &os, const NodeOperationBuilder &builder)
{
  os << "# Builder start\n";
  os << "digraph  G {\n";
  os << "    rankdir=LR;\n";
  os << "    node [shape=box];\n";
  for (const NodeOperation *operation : builder.get_operations()) {
    os << "    op" << operation->get_id() << " [label=\"" << *operation << "\"];\n";
  }

  os << "\n";
  for (const NodeOperationBuilder::Link &link : builder.get_links()) {
    os << "    op" << link.from()->get_operation().get_id() << " -> op"
       << link.to()->get_operation().get_id() << ";\n";
  }

  os << "}\n";
  os << "# Builder end\n";
  return os;
}

std::ostream &operator<<(std::ostream &os, const NodeOperationBuilder::Link &link)
{
  os << link.from()->get_operation().get_id() << " -> " << link.to()->get_operation().get_id();
  return os;
}

}  // namespace blender::compositor
