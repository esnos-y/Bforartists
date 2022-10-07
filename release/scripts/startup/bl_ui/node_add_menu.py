# SPDX-License-Identifier: GPL-2.0-or-later
import bpy
from bpy.types import Menu
from bpy.app.translations import (
    pgettext_iface as iface_,
    contexts as i18n_contexts,
)


def add_node_type(layout, node_type, *, label=None):
    """Add a node type to a menu."""
    bl_rna = bpy.types.Node.bl_rna_get_subclass(node_type)
    if not label:
        label = bl_rna.name if bl_rna else iface_("Unknown")
    translation_context = bl_rna.translation_context if bl_rna else i18n_contexts.default
    props = layout.operator("node.add_node", text=label, text_ctxt=translation_context, icon=bl_rna.icon)
    props.type = node_type
    props.use_transform = True
    return props


def draw_node_group_add_menu(context, layout):
    """Add items to the layout used for interacting with node groups."""
    space_node = context.space_data
    node_tree = space_node.edit_tree
    all_node_groups = context.blend_data.node_groups

    layout.operator("node.group_make")
    layout.operator("node.group_ungroup")
    if node_tree in all_node_groups.values():
        layout.separator()
        add_node_type(layout, "NodeGroupInput")
        add_node_type(layout, "NodeGroupOutput")

    if node_tree:
        from nodeitems_builtins import node_tree_group_type

        def contains_group(nodetree, group):
            if nodetree == group:
                return True
            for node in nodetree.nodes:
                if node.bl_idname in node_tree_group_type.values() and node.node_tree is not None:
                    if contains_group(node.node_tree, group):
                        return True
            return False

        groups = [
            group for group in context.blend_data.node_groups
            if (group.bl_idname == node_tree.bl_idname and
                not contains_group(group, node_tree) and
                not group.name.startswith('.'))
        ]
        if groups:
            layout.separator()
            for group in groups:
                props = add_node_type(layout, node_tree_group_type[group.bl_idname], label=group.name)
                ops = props.settings.add()
                ops.name = "node_tree"
                ops.value = "bpy.data.node_groups[%r]" % group.name


classes = (
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
