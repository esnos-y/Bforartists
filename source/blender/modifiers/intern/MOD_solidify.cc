/* SPDX-FileCopyrightText: 2005 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup modifiers
 */

#include <cstring>

#include "BLI_utildefines.h"

#include "BLT_translation.hh"

#include "DNA_defaults.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"

#include "BKE_context.hh"
#include "BKE_particle.h"
#include "BKE_screen.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_modifiertypes.hh"
#include "MOD_ui_common.hh"

#include "MOD_solidify_util.hh"

static void init_data(ModifierData *md)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;

  BLI_assert(MEMCMP_STRUCT_AFTER_IS_ZERO(smd, modifier));

  MEMCPY_STRUCT_AFTER(smd, DNA_struct_default_get(SolidifyModifierData), modifier);
}

#ifdef __GNUC__
#  pragma GCC diagnostic error "-Wsign-conversion"
#endif

static void required_data_mask(ModifierData *md, CustomData_MeshMasks *r_cddata_masks)
{
  SolidifyModifierData *smd = (SolidifyModifierData *)md;

  /* Ask for vertex-groups if we need them. */
  if (smd->defgrp_name[0] != '\0' || smd->shell_defgrp_name[0] != '\0' ||
      smd->rim_defgrp_name[0] != '\0')
  {
    r_cddata_masks->vmask |= CD_MASK_MDEFORMVERT;
  }
}

static Mesh *modify_mesh(ModifierData *md, const ModifierEvalContext *ctx, Mesh *mesh)
{
  const SolidifyModifierData *smd = (SolidifyModifierData *)md;
  switch (smd->mode) {
    case MOD_SOLIDIFY_MODE_EXTRUDE:
      return MOD_solidify_extrude_modifyMesh(md, ctx, mesh);
    case MOD_SOLIDIFY_MODE_NONMANIFOLD:
      return MOD_solidify_nonmanifold_modifyMesh(md, ctx, mesh);
    default:
      BLI_assert_unreachable();
  }
  return mesh;
}

static void panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col; /*bfa - removed *sub*/
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int solidify_mode = RNA_enum_get(ptr, "solidify_mode");
  bool has_vertex_group = RNA_string_length(ptr, "vertex_group") != 0;

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "solidify_mode", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    uiItemR(layout,
            ptr,
            "nonmanifold_thickness_mode",
            UI_ITEM_NONE,
            IFACE_("Thickness Mode"),
            ICON_NONE);
    uiItemR(layout, ptr, "nonmanifold_boundary_mode", UI_ITEM_NONE, IFACE_("Boundary"), ICON_NONE);
  }

  uiItemR(layout, ptr, "thickness", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "offset", UI_ITEM_NONE, nullptr, ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    uiItemR(layout, ptr, "nonmanifold_merge_threshold", UI_ITEM_NONE, nullptr, ICON_NONE);
  }
  else {
    /*------------------- bfa - original props */
    // uiItemR(layout, ptr, "use_even_offset", UI_ITEM_NONE, nullptr, ICON_NONE);

    col = uiLayoutColumn(layout, true);
    row = uiLayoutRow(col, true);
    uiLayoutSetPropSep(row, false); /* bfa - use_property_split = False */
    uiItemR(row, ptr, "use_even_offset", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemDecoratorR(row, ptr, "use_even_offset", 0); /*bfa - decorator*/
    /* ------------ end bfa */
  }

  /*------------------- bfa - original props */
  // col = uiLayoutColumnWithHeading(layout, false, CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Rim"));
  // uiItemR(col, ptr, "use_rim", UI_ITEM_NONE, IFACE_("Fill"), ICON_NONE);
  // sub = uiLayoutColumn(col, false);
  // uiLayoutSetActive(sub, RNA_boolean_get(ptr, "use_rim"));
  // uiItemR(sub, ptr, "use_rim_only", UI_ITEM_NONE, nullptr, ICON_NONE);

  col = uiLayoutColumn(layout, true);
  row = uiLayoutRow(col, true);
  uiLayoutSetPropSep(row, false); /* bfa - use_property_split = False */
  uiItemR(row, ptr, "use_rim", UI_ITEM_NONE, IFACE_("Fill Rim"), ICON_NONE);
  uiItemDecoratorR(row, ptr, "use_rim", 0); /*bfa - decorator*/

  if (RNA_boolean_get(ptr, "use_rim")) {
    row = uiLayoutRow(col, true);
    uiLayoutSetPropSep(row, false); /* bfa - use_property_split = False */
    uiItemS(row);
    uiItemR(row, ptr, "use_rim_only", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemDecoratorR(row, ptr, "use_rim_only", 0); /*bfa - decorator*/
  }
  /* ------------ end bfa */

  uiItemS(layout);

  modifier_vgroup_ui(layout, ptr, &ob_ptr, "vertex_group", "invert_vertex_group", nullptr);
  row = uiLayoutRow(layout, false);
  uiLayoutSetActive(row, has_vertex_group);
  uiItemS(row); /*bfa -indent*/
  uiItemR(row, ptr, "thickness_vertex_group", UI_ITEM_NONE, IFACE_("Factor"), ICON_NONE);

  if (solidify_mode == MOD_SOLIDIFY_MODE_NONMANIFOLD) {
    row = uiLayoutRow(layout, false);
    uiLayoutSetActive(row, has_vertex_group);

    /*------------------- bfa - original props */
    // uiItemR(row, ptr, "use_flat_faces", UI_ITEM_NONE, nullptr, ICON_NONE);

    uiLayoutSetPropSep(row, false); /* bfa - use_property_split = False */
    uiItemS(row);                   /*bfa -indent*/
    uiItemR(row, ptr, "use_flat_faces", UI_ITEM_NONE, nullptr, ICON_NONE);
    uiItemDecoratorR(row, ptr, "use_flat_faces", 0); /*bfa - decorator*/
    /* ------------ end bfa */
  }

  modifier_panel_end(layout, ptr);
}

static void normals_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col, *row; /*bfa - added *row*/
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int solidify_mode = RNA_enum_get(ptr, "solidify_mode");

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);

  /*------------------- bfa - original props */
  // uiItemR(col, ptr, "use_flip_normals", UI_ITEM_NONE, IFACE_("Flip"), ICON_NONE);

  row = uiLayoutRow(col, true);
  uiLayoutSetPropSep(row, false); /* bfa - use_property_split = False */
  uiItemR(row, ptr, "use_flip_normals", UI_ITEM_NONE, IFACE_("Flip"), ICON_NONE);
  uiItemDecoratorR(row, ptr, "use_flip_normals", 0); /*bfa - decorator*/
  /* ------------ end bfa */

  if (solidify_mode == MOD_SOLIDIFY_MODE_EXTRUDE) {
    /*------------------- bfa - original props */
    // uiItemR(col, ptr, "use_quality_normals", UI_ITEM_NONE, IFACE_("High Quality"), ICON_NONE);

    row = uiLayoutRow(col, true);
    uiLayoutSetPropSep(row, false); /* bfa - use_property_split = False */
    uiItemR(row, ptr, "use_quality_normals", UI_ITEM_NONE, IFACE_("High Quality"), ICON_NONE);
    uiItemDecoratorR(row, ptr, "use_quality_normals", 0); /*bfa - decorator*/
    /* ------------ end bfa */
  }
}

static void materials_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  uiItemR(layout, ptr, "material_offset", UI_ITEM_NONE, nullptr, ICON_NONE);
  col = uiLayoutColumn(layout, true);
  uiLayoutSetActive(col, RNA_boolean_get(ptr, "use_rim"));
  uiItemR(col,
          ptr,
          "material_offset_rim",
          UI_ITEM_NONE,
          CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Rim"),
          ICON_NONE);
}

static void edge_data_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  int solidify_mode = RNA_enum_get(ptr, "solidify_mode");

  uiLayoutSetPropSep(layout, true);

  if (solidify_mode == MOD_SOLIDIFY_MODE_EXTRUDE) {
    uiLayout *col;
    col = uiLayoutColumn(layout, true);
    uiItemR(col, ptr, "edge_crease_inner", UI_ITEM_NONE, IFACE_("Crease Inner"), ICON_NONE);
    uiItemR(col, ptr, "edge_crease_outer", UI_ITEM_NONE, IFACE_("Outer"), ICON_NONE);
    uiItemR(col,
            ptr,
            "edge_crease_rim",
            UI_ITEM_NONE,
            CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Rim"),
            ICON_NONE);
  }
  uiItemR(layout, ptr, "bevel_convex", UI_ITEM_R_SLIDER, nullptr, ICON_NONE);
}

static void clamp_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *row, *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "thickness_clamp", UI_ITEM_NONE, nullptr, ICON_NONE);
  row = uiLayoutRow(col, false);
  uiLayoutSetActive(row, RNA_float_get(ptr, "thickness_clamp") > 0.0f);

  /*------------------- bfa - original props */
  // uiItemR(row, ptr, "use_thickness_angle_clamp", UI_ITEM_NONE, nullptr, ICON_NONE);

  uiLayoutSetPropSep(row, false); /* bfa - use_property_split = False */
  uiItemS(row);
  uiItemR(row, ptr, "use_thickness_angle_clamp", UI_ITEM_NONE, nullptr, ICON_NONE);
  uiItemDecoratorR(row, ptr, "use_thickness_angle_clamp", 0); /*bfa - decorator*/
  /* ------------ end bfa */
}

static void vertex_group_panel_draw(const bContext * /*C*/, Panel *panel)
{
  uiLayout *col;
  uiLayout *layout = panel->layout;

  PointerRNA ob_ptr;
  PointerRNA *ptr = modifier_panel_get_property_pointers(panel, &ob_ptr);

  uiLayoutSetPropSep(layout, true);

  col = uiLayoutColumn(layout, false);
  uiItemPointerR(
      col, ptr, "shell_vertex_group", &ob_ptr, "vertex_groups", IFACE_("Shell"), ICON_NONE);
  uiItemPointerR(col,
                 ptr,
                 "rim_vertex_group",
                 &ob_ptr,
                 "vertex_groups",
                 CTX_IFACE_(BLT_I18NCONTEXT_ID_MESH, "Rim"),
                 ICON_NONE);
}

static void panel_register(ARegionType *region_type)
{
  PanelType *panel_type = modifier_panel_register(region_type, eModifierType_Solidify, panel_draw);
  modifier_subpanel_register(
      region_type, "normals", "Normals", nullptr, normals_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "materials", "Materials", nullptr, materials_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "edge_data", "Edge Data", nullptr, edge_data_panel_draw, panel_type);
  modifier_subpanel_register(
      region_type, "clamp", "Thickness Clamp", nullptr, clamp_panel_draw, panel_type);
  modifier_subpanel_register(region_type,
                             "vertex_groups",
                             "Output Vertex Groups",
                             nullptr,
                             vertex_group_panel_draw,
                             panel_type);
}

ModifierTypeInfo modifierType_Solidify = {
    /*idname*/ "Solidify",
    /*name*/ N_("Solidify"),
    /*struct_name*/ "SolidifyModifierData",
    /*struct_size*/ sizeof(SolidifyModifierData),
    /*srna*/ &RNA_SolidifyModifier,
    /*type*/ ModifierTypeType::Constructive,

    /*flags*/ eModifierTypeFlag_AcceptsMesh | eModifierTypeFlag_AcceptsCVs |
        eModifierTypeFlag_SupportsMapping | eModifierTypeFlag_SupportsEditmode |
        eModifierTypeFlag_EnableInEditmode,
    /*icon*/ ICON_MOD_SOLIDIFY,

    /*copy_data*/ BKE_modifier_copydata_generic,

    /*deform_verts*/ nullptr,
    /*deform_matrices*/ nullptr,
    /*deform_verts_EM*/ nullptr,
    /*deform_matrices_EM*/ nullptr,
    /*modify_mesh*/ modify_mesh,
    /*modify_geometry_set*/ nullptr,

    /*init_data*/ init_data,
    /*required_data_mask*/ required_data_mask,
    /*free_data*/ nullptr,
    /*is_disabled*/ nullptr,
    /*update_depsgraph*/ nullptr,
    /*depends_on_time*/ nullptr,
    /*depends_on_normals*/ nullptr,
    /*foreach_ID_link*/ nullptr,
    /*foreach_tex_link*/ nullptr,
    /*free_runtime_data*/ nullptr,
    /*panel_register*/ panel_register,
    /*blend_write*/ nullptr,
    /*blend_read*/ nullptr,
    /*foreach_cache*/ nullptr,
};
