/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spfile
 */

#include <cerrno>
#include <cmath>
#include <cstring>
#include <string>

#include <fmt/format.h>

#include "MEM_guardedalloc.h"

#include "AS_asset_representation.hh"

#include "BLI_blenlib.h"
#include "BLI_fileops_types.h"
#include "BLI_math_color.h"
#include "BLI_utildefines.h"

#ifdef WIN32
#  include "BLI_winstuff.h"
#endif

#include "BIF_glutil.hh"

#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_report.hh"

#include "BLO_readfile.hh"

#include "BLT_translation.hh"

#include "BLF_api.hh"

#include "IMB_imbuf.hh"
#include "IMB_imbuf_types.hh"
#include "IMB_metadata.hh"
#include "IMB_thumbs.hh"

#include "DNA_userdef_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "ED_fileselect.hh"
#include "ED_screen.hh"

#include "UI_interface.hh"
#include "UI_interface_icons.hh"
#include "UI_resources.hh"
#include "UI_view2d.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "GPU_immediate.hh"
#include "GPU_immediate_util.hh"
#include "GPU_state.hh"

#include "filelist.hh"

#include "file_intern.hh" /* own include */

void ED_file_path_button(bScreen *screen,
                         const SpaceFile *sfile,
                         FileSelectParams *params,
                         uiBlock *block)
{
  uiBut *but;

  BLI_assert_msg(params != nullptr,
                 "File select parameters not set. The caller is expected to check this.");

  PointerRNA params_rna_ptr = RNA_pointer_create(&screen->id, &RNA_FileSelectParams, params);

  /* callbacks for operator check functions */
  UI_block_func_set(block, file_draw_check_cb, nullptr, nullptr);

  but = uiDefButR(block,
                  UI_BTYPE_TEXT,
                  -1,
                  "",
                  0,
                  0,
                  UI_UNIT_X * 10,
                  UI_UNIT_Y,
                  &params_rna_ptr,
                  "directory",
                  0,
                  0.0f,
                  float(FILE_MAX),
                  TIP_("File path"));

  BLI_assert(!UI_but_flag_is_set(but, UI_BUT_UNDO));
  BLI_assert(!UI_but_is_utf8(but));

  UI_but_func_complete_set(but, autocomplete_directory, nullptr);
  UI_but_funcN_set(but, file_directory_enter_handle, nullptr, but);

  /* TODO: directory editing is non-functional while a library is loaded
   * until this is properly supported just disable it. */
  if (sfile && sfile->files && filelist_lib(sfile->files)) {
    UI_but_flag_enable(but, UI_BUT_DISABLED);
  }

  /* clear func */
  UI_block_func_set(block, nullptr, nullptr, nullptr);
}

struct FileTooltipData {
  const SpaceFile *sfile;
  const FileDirEntry *file;
};

static FileTooltipData *file_tooltip_data_create(const SpaceFile *sfile, const FileDirEntry *file)
{
  FileTooltipData *data = (FileTooltipData *)MEM_mallocN(sizeof(FileTooltipData), __func__);
  data->sfile = sfile;
  data->file = file;
  return data;
}

static void file_draw_tooltip_custom_func(bContext & /*C*/, uiTooltipData &tip, void *argN)
{
  FileTooltipData *file_data = static_cast<FileTooltipData *>(argN);
  const SpaceFile *sfile = file_data->sfile;
  const FileList *files = sfile->files;
  const FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  const FileDirEntry *file = file_data->file;

  BLI_assert_msg(!file->asset, "Asset tooltip should never be overridden here.");

  /* Check the FileDirEntry first to see if the preview is already loaded. */
  ImBuf *thumb = filelist_file_getimage(file);

  /* Only free if it is loaded later. */
  bool free_imbuf = (thumb == nullptr);

  UI_tooltip_text_field_add(tip, file->name, {}, UI_TIP_STYLE_HEADER, UI_TIP_LC_MAIN);
  UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);

  if (!(file->typeflag & FILE_TYPE_BLENDERLIB)) {

    char full_path[FILE_MAX_LIBEXTRA];
    filelist_file_get_full_path(files, file, full_path);

    if (params->recursion_level > 0) {
      char root[FILE_MAX];
      BLI_path_split_dir_part(full_path, root, FILE_MAX);
      UI_tooltip_text_field_add(tip, root, {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
    }

    if (file->redirection_path) {
      UI_tooltip_text_field_add(tip,
                                fmt::format("{}: {}", N_("Link target"), file->redirection_path),
                                {},
                                UI_TIP_STYLE_NORMAL,
                                UI_TIP_LC_NORMAL);
    }
    if (file->attributes & FILE_ATTR_OFFLINE) {
      UI_tooltip_text_field_add(
          tip, N_("This file is offline"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
    }
    if (file->attributes & FILE_ATTR_READONLY) {
      UI_tooltip_text_field_add(
          tip, N_("This file is read-only"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
    }
    if (file->attributes & (FILE_ATTR_SYSTEM | FILE_ATTR_RESTRICTED)) {
      UI_tooltip_text_field_add(
          tip, N_("This is a restricted system file"), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_ALERT);
    }

    if (file->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) {
      char version_st[128] = {0};
      if (!thumb) {
        /* Load the thumbnail from cache if existing, but don't create if not. */
        thumb = IMB_thumb_read(full_path, THB_LARGE);
      }
      if (thumb) {
        /* Look for version in existing thumbnail if available. */
        IMB_metadata_get_field(
            thumb->metadata, "Thumb::Blender::Version", version_st, sizeof(version_st));
      }

      if (!version_st[0] && !(file->attributes & FILE_ATTR_OFFLINE)) {
        /* Load Blender version directly from the file. */
        short version = BLO_version_from_file(full_path);
        if (version != 0) {
          SNPRINTF(version_st, "%d.%01d", version / 100, version % 100);
        }
      }

      if (version_st[0]) {
        UI_tooltip_text_field_add(
            tip, fmt::format("Blender {}", version_st), {}, UI_TIP_STYLE_NORMAL, UI_TIP_LC_NORMAL);
        UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
      }
    }
    else if (file->typeflag & FILE_TYPE_IMAGE) {
      if (!thumb) {
        /* Load the thumbnail from cache if existing, create if not. */
        thumb = IMB_thumb_manage(full_path, THB_LARGE, THB_SOURCE_IMAGE);
      }
      if (thumb) {
        char value1[128];
        char value2[128];
        if (IMB_metadata_get_field(
                thumb->metadata, "Thumb::Image::Width", value1, sizeof(value1)) &&
            IMB_metadata_get_field(
                thumb->metadata, "Thumb::Image::Height", value2, sizeof(value2)))
        {
          UI_tooltip_text_field_add(tip,
                                    fmt::format("{} \u00D7 {}", value1, value2),
                                    {},
                                    UI_TIP_STYLE_NORMAL,
                                    UI_TIP_LC_NORMAL);
          UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
        }
      }
    }
    else if (file->typeflag & FILE_TYPE_MOVIE) {
      if (!thumb) {
        /* This could possibly take a while. */
        thumb = IMB_thumb_manage(full_path, THB_LARGE, THB_SOURCE_MOVIE);
      }
      if (thumb) {
        char value1[128];
        char value2[128];
        char value3[128];
        if (IMB_metadata_get_field(
                thumb->metadata, "Thumb::Video::Width", value1, sizeof(value1)) &&
            IMB_metadata_get_field(
                thumb->metadata, "Thumb::Video::Height", value2, sizeof(value2)))
        {
          UI_tooltip_text_field_add(tip,
                                    fmt::format("{} \u00D7 {}", value1, value2),
                                    {},
                                    UI_TIP_STYLE_NORMAL,
                                    UI_TIP_LC_NORMAL);
        }
        if (IMB_metadata_get_field(
                thumb->metadata, "Thumb::Video::Frames", value1, sizeof(value1)) &&
            IMB_metadata_get_field(thumb->metadata, "Thumb::Video::FPS", value2, sizeof(value2)) &&
            IMB_metadata_get_field(
                thumb->metadata, "Thumb::Video::Duration", value3, sizeof(value3)))
        {
          UI_tooltip_text_field_add(
              tip,
              fmt::format("{} {} @ {} {}", value1, N_("Frames"), value2, N_("FPS")),
              {},
              UI_TIP_STYLE_NORMAL,
              UI_TIP_LC_NORMAL);
          UI_tooltip_text_field_add(tip,
                                    fmt::format("{} {}", value3, N_("seconds")),
                                    {},
                                    UI_TIP_STYLE_NORMAL,
                                    UI_TIP_LC_NORMAL);
          UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
        }
      }
    }

    char date_st[FILELIST_DIRENTRY_DATE_LEN], time_st[FILELIST_DIRENTRY_TIME_LEN];
    bool is_today, is_yesterday;
    std::string day_string = ("");
    BLI_filelist_entry_datetime_to_string(
        nullptr, file->time, false, time_st, date_st, &is_today, &is_yesterday);
    if (is_today || is_yesterday) {
      day_string = (is_today ? N_("Today") : N_("Yesterday")) + std::string(" ");
    }
    UI_tooltip_text_field_add(tip,
                              fmt::format("{}: {}{}{}",
                                          N_("Modified"),
                                          day_string,
                                          (is_today || is_yesterday) ? "" : date_st,
                                          (is_today || is_yesterday) ? time_st : ""),
                              {},
                              UI_TIP_STYLE_NORMAL,
                              UI_TIP_LC_NORMAL);

    if (!(file->typeflag & FILE_TYPE_DIR) && file->size > 0) {
      char size[16];
      BLI_filelist_entry_size_to_string(nullptr, file->size, false, size);
      if (file->size < 10000) {
        char size_full[BLI_STR_FORMAT_UINT64_GROUPED_SIZE];
        BLI_str_format_uint64_grouped(size_full, file->size);
        UI_tooltip_text_field_add(
            tip,
            fmt::format("{}: {} ({} {})", N_("Size"), size, size_full, N_("bytes")),
            {},
            UI_TIP_STYLE_NORMAL,
            UI_TIP_LC_NORMAL);
      }
      else {
        UI_tooltip_text_field_add(tip,
                                  fmt::format("{}: {}", N_("Size"), size),
                                  {},
                                  UI_TIP_STYLE_NORMAL,
                                  UI_TIP_LC_NORMAL);
      }
    }
  }

  if (thumb && params->display != FILE_IMGDISPLAY) {
    UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);
    UI_tooltip_text_field_add(tip, {}, {}, UI_TIP_STYLE_SPACER, UI_TIP_LC_NORMAL);

    uiTooltipImage image_data;
    float scale = (96.0f * UI_SCALE_FAC) / float(std::max(thumb->x, thumb->y));
    image_data.ibuf = thumb;
    image_data.width = short(float(thumb->x) * scale);
    image_data.height = short(float(thumb->y) * scale);
    image_data.border = true;
    image_data.background = uiTooltipImageBackground::Checkerboard_Themed;
    image_data.premultiplied = true;
    UI_tooltip_image_field_add(tip, image_data);
  }

  if (thumb && free_imbuf) {
    IMB_freeImBuf(thumb);
  }
}

static std::string file_draw_asset_tooltip_func(bContext * /*C*/, void *argN, const char * /*tip*/)
{
  const auto *asset = static_cast<blender::asset_system::AssetRepresentation *>(argN);
  std::string complete_string = asset->get_name();
  const AssetMetaData &meta_data = asset->get_metadata();
  if (meta_data.description) {
    complete_string += '\n';
    complete_string += meta_data.description;
  }
  return complete_string;
}

static void draw_tile_background(const rcti *draw_rect, int colorid, int shade)
{
  float color[4];
  rctf draw_rect_fl;
  BLI_rctf_rcti_copy(&draw_rect_fl, draw_rect);

  UI_GetThemeColorShade4fv(colorid, shade, color);
  UI_draw_roundbox_corner_set(UI_CNR_ALL);
  UI_draw_roundbox_aa(&draw_rect_fl, true, 5.0f, color);
}

static void file_but_enable_drag(uiBut *but,
                                 const SpaceFile *sfile,
                                 const FileDirEntry *file,
                                 const char *path,
                                 const ImBuf *preview_image,
                                 int icon,
                                 float scale)
{
  ID *id;

  if ((id = filelist_file_get_id(file))) {
    UI_but_drag_set_id(but, id);
    if (preview_image) {
      UI_but_drag_attach_image(but, preview_image, scale);
    }
  }
  else if (sfile->browse_mode == FILE_BROWSE_MODE_ASSETS &&
           (file->typeflag & FILE_TYPE_ASSET) != 0)
  {
    const int import_method = ED_fileselect_asset_import_method_get(sfile, file);
    BLI_assert(import_method > -1);

    BLI_assert(ED_fileselect_is_asset_browser(sfile) && file->asset);
    const FileAssetSelectParams *params = ED_fileselect_get_asset_params(sfile); /*BFA*/
    bool drop_collections_as_instances = params->drop_collections_as_instances; /*BFA*/
    bool drop_collections_at_origin = params->drop_collections_at_origin; /*BFA*/
    UI_but_drag_set_asset(but, file->asset, import_method, icon, preview_image, scale, drop_collections_as_instances, drop_collections_at_origin); /*BFA*/
  }
  else if (preview_image) {
    UI_but_drag_set_image(but, path, icon, preview_image, scale);
  }
  else {
    /* path is no more static, cannot give it directly to but... */
    UI_but_drag_set_path(but, path);
  }
}

static uiBut *file_add_icon_but(const SpaceFile *sfile,
                                uiBlock *block,
                                const char * /*path*/,
                                const FileDirEntry *file,
                                const rcti *tile_draw_rect,
                                int icon,
                                int width,
                                int height,
                                bool dimmed)
{
  uiBut *but;

  const int x = tile_draw_rect->xmin;
  const int y = tile_draw_rect->ymax - sfile->layout->tile_border_y - height;

  but = uiDefIconBut(
      block, UI_BTYPE_LABEL, 0, icon, x, y, width, height, nullptr, 0.0f, 0.0f, nullptr);
  UI_but_label_alpha_factor_set(but, dimmed ? 0.3f : 1.0f);
  if (file->asset) {
    UI_but_func_tooltip_set(but, file_draw_asset_tooltip_func, file->asset, nullptr);
  }
  else {
    UI_but_func_tooltip_custom_set(
        but, file_draw_tooltip_custom_func, file_tooltip_data_create(sfile, file), MEM_freeN);
  }

  return but;
}

static void file_draw_string(int sx,
                             int sy,
                             const char *string,
                             float width,
                             int height,
                             eFontStyle_Align align,
                             const uchar col[4])
{
  uiFontStyle fs;
  rcti rect;
  char filename[FILE_MAXFILE];

  if (string[0] == '\0' || width < 1) {
    return;
  }

  const uiStyle *style = UI_style_get();
  fs = style->widget;

  STRNCPY(filename, string);
  UI_text_clip_middle_ex(&fs, filename, width, UI_ICON_SIZE, sizeof(filename), '\0');

  /* no text clipping needed, UI_fontstyle_draw does it but is a bit too strict
   * (for buttons it works) */
  rect.xmin = sx;
  rect.xmax = sx + round_fl_to_int(width);
  rect.ymin = sy - height;
  rect.ymax = sy;

  uiFontStyleDraw_Params font_style_params{};
  font_style_params.align = align;

  UI_fontstyle_draw(&fs, &rect, filename, sizeof(filename), col, &font_style_params);
}

/**
 * \param r_sx, r_sy: The lower right corner of the last line drawn, plus the height of the last
 *                    line. This is the cursor position on completion to allow drawing more text
 *                    behind that.
 */
static void file_draw_string_multiline(int sx,
                                       int sy,
                                       const char *string,
                                       int wrap_width,
                                       int line_height,
                                       const uchar text_col[4],
                                       int *r_sx,
                                       int *r_sy)
{
  rcti rect;

  if (string[0] == '\0' || wrap_width < 1) {
    return;
  }

  const uiStyle *style = UI_style_get();
  int font_id = style->widget.uifont_id;
  int len = strlen(string);

  rcti textbox;
  BLF_wordwrap(font_id, wrap_width);
  BLF_enable(font_id, BLF_WORD_WRAP);
  BLF_boundbox(font_id, string, len, &textbox);
  BLF_disable(font_id, BLF_WORD_WRAP);

  /* no text clipping needed, UI_fontstyle_draw does it but is a bit too strict
   * (for buttons it works) */
  rect.xmin = sx;
  rect.xmax = sx + wrap_width;
  /* Need to increase the clipping rect by one more line, since the #UI_fontstyle_draw_ex() will
   * actually start drawing at (ymax - line-height). */
  rect.ymin = sy - BLI_rcti_size_y(&textbox) - line_height;
  rect.ymax = sy;

  uiFontStyleDraw_Params font_style_params{};
  font_style_params.align = UI_STYLE_TEXT_LEFT;
  font_style_params.word_wrap = true;

  ResultBLF result;
  UI_fontstyle_draw_ex(
      &style->widget, &rect, string, len, text_col, &font_style_params, nullptr, nullptr, &result);
  if (r_sx) {
    *r_sx = result.width;
  }
  if (r_sy) {
    *r_sy = rect.ymin + line_height;
  }
}

void file_calc_previews(const bContext *C, ARegion *region)
{
  SpaceFile *sfile = CTX_wm_space_file(C);
  View2D *v2d = &region->v2d;

  ED_fileselect_init_layout(sfile, region);
  UI_view2d_totRect_set(v2d, sfile->layout->width, sfile->layout->height);
}

static void file_add_preview_drag_but(const SpaceFile *sfile,
                                      uiBlock *block,
                                      FileLayout *layout,
                                      const FileDirEntry *file,
                                      const char *path,
                                      const rcti *tile_draw_rect,
                                      const ImBuf *preview_image,
                                      const int icon,
                                      const float scale)
{
  /* Invisible button for dragging. */
  rcti drag_rect = *tile_draw_rect;
  /* A bit smaller than the full tile, to increase the gap between items that users can drag from
   * for box select. */
  BLI_rcti_pad(&drag_rect, -layout->tile_border_x, -layout->tile_border_y);

  uiBut *but = uiDefBut(block,
                        UI_BTYPE_LABEL,
                        0,
                        "",
                        drag_rect.xmin,
                        drag_rect.ymin,
                        BLI_rcti_size_x(&drag_rect),
                        BLI_rcti_size_y(&drag_rect),
                        nullptr,
                        0.0,
                        0.0,
                        nullptr);
  file_but_enable_drag(but, sfile, file, path, preview_image, icon, scale);

  if (file->asset) {
    UI_but_func_tooltip_set(but, file_draw_asset_tooltip_func, file->asset, nullptr);
  }
  else {
    UI_but_func_tooltip_custom_set(
        but, file_draw_tooltip_custom_func, file_tooltip_data_create(sfile, file), MEM_freeN);
  }
}

static void file_draw_preview(const FileList *files,
                              const FileDirEntry *file,
                              const rcti *tile_draw_rect,
                              const float icon_aspect,
                              const ImBuf *imb,
                              const int icon,
                              FileLayout *layout,
                              const bool is_icon,
                              const bool dimmed,
                              const bool is_link,
                              float *r_scale)
{
  float fx, fy;
  float dx, dy;
  int xco, yco;
  float ui_imbx, ui_imby;
  float scaledx, scaledy;
  float scale;
  int ex, ey;
  bool show_outline = !is_icon && (file->typeflag & (FILE_TYPE_IMAGE | FILE_TYPE_OBJECT_IO |
                                                     FILE_TYPE_MOVIE | FILE_TYPE_BLENDER));
  const bool is_offline = (file->attributes & FILE_ATTR_OFFLINE);
  const bool is_loading = filelist_file_is_preview_pending(files, file);

  BLI_assert(imb != nullptr);

  ui_imbx = imb->x * UI_SCALE_FAC;
  ui_imby = imb->y * UI_SCALE_FAC;
  /* Unlike thumbnails, icons are not scaled up. */
  if (((ui_imbx > layout->prv_w) || (ui_imby > layout->prv_h)) ||
      (!is_icon && ((ui_imbx < layout->prv_w) || (ui_imby < layout->prv_h))))
  {
    if (imb->x > imb->y) {
      scaledx = float(layout->prv_w);
      scaledy = (float(imb->y) / float(imb->x)) * layout->prv_w;
      scale = scaledx / imb->x;
    }
    else {
      scaledy = float(layout->prv_h);
      scaledx = (float(imb->x) / float(imb->y)) * layout->prv_h;
      scale = scaledy / imb->y;
    }
  }
  else {
    scaledx = ui_imbx;
    scaledy = ui_imby;
    scale = UI_SCALE_FAC;
  }

  ex = int(scaledx);
  ey = int(scaledy);
  fx = (float(layout->prv_w) - float(ex)) / 2.0f;
  fy = (float(layout->prv_h) - float(ey)) / 2.0f;
  dx = (fx + 0.5f + layout->prv_border_x);
  dy = (fy + 0.5f - layout->prv_border_y);
  xco = tile_draw_rect->xmin + int(dx);
  yco = tile_draw_rect->ymax - layout->prv_h + int(dy);

  GPU_blend(GPU_BLEND_ALPHA);

  /* the large image */

  float document_img_col[4] = {1.0f, 1.0f, 1.0f, 1.0f};
  if (is_icon) {
    if (file->typeflag & FILE_TYPE_DIR) {
      UI_GetThemeColor4fv(TH_ICON_FOLDER, document_img_col);
    }
    else {
      UI_GetThemeColor4fv(TH_TEXT, document_img_col);
    }
  }
  else if (file->typeflag & FILE_TYPE_FTFONT) {
    UI_GetThemeColor4fv(TH_TEXT, document_img_col);
  }

  if (dimmed) {
    document_img_col[3] *= 0.3f;
  }

  if (!is_icon && ELEM(file->typeflag, FILE_TYPE_IMAGE, FILE_TYPE_OBJECT_IO)) {
    /* Draw checker pattern behind image previews in case they have transparency. */
    imm_draw_box_checker_2d(float(xco), float(yco), float(xco + ex), float(yco + ey));
  }

  if (!is_icon && file->typeflag & FILE_TYPE_BLENDERLIB) {
    /* Datablock preview images use premultiplied alpha. */
    GPU_blend(GPU_BLEND_ALPHA_PREMULT);
  }

  if (!is_loading) {
    /* Don't show outer document image if loading - too flashy. */
    if (is_icon) {
      /* Draw large folder or document icon. */
      const int icon_large = (file->typeflag & FILE_TYPE_DIR) ? ICON_FILE_FOLDER_LARGE :
                                                                ICON_FILE_LARGE;
      uchar icon_col[4];
      rgba_float_to_uchar(icon_col, document_img_col);
      float icon_x = float(xco) + (file->typeflag & FILE_TYPE_DIR ? 0.0f : ex * -0.142f);
      float icon_y = float(yco) + (file->typeflag & FILE_TYPE_DIR ? ex * -0.11f : 0.0f);
      UI_icon_draw_ex(icon_x,
                      icon_y,
                      icon_large,
                      icon_aspect / 4.0f / UI_SCALE_FAC,
                      document_img_col[3],
                      0.0f,
                      icon_col,
                      false,
                      UI_NO_ICON_OVERLAY_TEXT);
    }
    else {
      IMMDrawPixelsTexState state = immDrawPixelsTexSetup(GPU_SHADER_3D_IMAGE_COLOR);
      immDrawPixelsTexTiled_scaling(&state,
                                    float(xco),
                                    float(yco),
                                    imb->x,
                                    imb->y,
                                    GPU_RGBA8,
                                    true,
                                    imb->byte_buffer.data,
                                    scale,
                                    scale,
                                    1.0f,
                                    1.0f,
                                    document_img_col);
    }
  }

  if (icon && is_icon) {
    /* Small icon in the middle of large image, scaled to fit container and UI scale */
    float icon_x, icon_y;
    float icon_opacity = 0.8f; /*bfa - changed from 0.3 to 0.8*/
    uchar icon_color[4] = {0, 0, 0, 255};
    if (rgb_to_grayscale(document_img_col) < 0.5f) {
      icon_color[0] = 255;
      icon_color[1] = 255;
      icon_color[2] = 255;
    }

    if (is_loading) {
      /* Contrast with background since we are not showing the large document image. */
      UI_GetThemeColor4ubv(TH_TEXT, icon_color);
    }

    icon_x = xco + (file->typeflag & FILE_TYPE_DIR ? ex * 0.31f : ex * 0.178f);
    /* BFA - WIP changed mini icon position y from (ey / 2.0f) to (ey / 2.2f)*/
    icon_y = yco + (file->typeflag & FILE_TYPE_DIR ? ex * 0.11f : ex * 0.15f);
    UI_icon_draw_ex(icon_x,
                    icon_y,
                    is_loading ? ICON_TEMP : icon,
                    icon_aspect / UI_SCALE_FAC / (file->typeflag & FILE_TYPE_DIR ? 1.5f : 2.0f),
                    icon_opacity,
                    0.0f,
                    icon_color,
                    false,
                    UI_NO_ICON_OVERLAY_TEXT);
  }

  if (is_link || is_offline) {
    /* Icon at bottom to indicate it is a shortcut, link, alias, or offline. */
    const int arrow = is_link ? ICON_LOOP_FORWARDS : ICON_INTERNET;
    if (!is_icon) {
      /* At very bottom-left if preview style. */
      const uchar light[4] = {255, 255, 255, 255};
      const float icon_x = float(xco) + (2.0f * UI_SCALE_FAC);
      const float icon_y = float(yco) + (2.0f * UI_SCALE_FAC);
      UI_icon_draw_ex(icon_x,
                      icon_y,
                      arrow,
                      1.0f / UI_SCALE_FAC,
                      0.6f,
                      0.0f,
                      light,
                      true,
                      UI_NO_ICON_OVERLAY_TEXT);
    }
    else {
      /* Link to folder or non-previewed file. */
      uchar icon_color[4];
      UI_GetThemeColor4ubv(TH_BACK, icon_color);
      const float icon_x = xco + ((file->typeflag & FILE_TYPE_DIR) ? 0.14f : 0.23f) * scaledx;
      const float icon_y = yco + ((file->typeflag & FILE_TYPE_DIR) ? 0.24f : 0.14f) * scaledy;
      UI_icon_draw_ex(icon_x,
                      icon_y,
                      arrow,
                      icon_aspect / UI_SCALE_FAC * 1.8f,
                      0.3f,
                      0.0f,
                      icon_color,
                      false,
                      UI_NO_ICON_OVERLAY_TEXT);
    }
  }
  else if (icon && icon_aspect < 2.0f &&
           ((!is_icon && !(file->typeflag & FILE_TYPE_FTFONT)) || is_loading))
  {
    /* Smaller, fainter icon at bottom-left for preview image thumbnail, but not for fonts. */
    const uchar light[4] = {255, 255, 255, 255};
    const float icon_x = float(tile_draw_rect->xmin) + (3.0f * UI_SCALE_FAC);
    const float icon_y = float(tile_draw_rect->ymin) + (17.0f * UI_SCALE_FAC);
    UI_icon_draw_ex(icon_x,
                    icon_y,
                    icon,
                    1.0f / UI_SCALE_FAC,
                    0.6f,
                    0.0f,
                    light,
                    true,
                    UI_NO_ICON_OVERLAY_TEXT);
  }

  const bool is_current_main_data = filelist_file_get_id(file) != nullptr;
  if (is_current_main_data) {
    /* Smaller, fainter icon at the top-right indicating that the file represents data from the
     * current file (from current #Main in fact). */
    float icon_x, icon_y;
    const uchar light[4] = {255, 255, 255, 255};
    icon_x = float(tile_draw_rect->xmax) - (16.0f * UI_SCALE_FAC);
    icon_y = float(tile_draw_rect->ymax) - (20.0f * UI_SCALE_FAC);
    UI_icon_draw_ex(icon_x,
                    icon_y,
                    ICON_BLENDER,
                    1.0f / UI_SCALE_FAC,
                    0.6f,
                    0.0f,
                    light,
                    true,
                    UI_NO_ICON_OVERLAY_TEXT);
  }

  /* Contrasting outline around some preview types. */
  if (show_outline) {
    GPU_blend(GPU_BLEND_ALPHA);

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    float border_color[4] = {1.0f, 1.0f, 1.0f, 0.15f};
    float bgcolor[4];
    UI_GetThemeColor4fv(TH_BACK, bgcolor);
    if (rgb_to_grayscale(bgcolor) > 0.5f) {
      border_color[0] = 0.0f;
      border_color[1] = 0.0f;
      border_color[2] = 0.0f;
    }
    immUniformColor4fv(border_color);
    imm_draw_box_wire_2d(pos, float(xco), float(yco), float(xco + ex + 1), float(yco + ey + 1));
    immUnbindProgram();
  }

  GPU_blend(GPU_BLEND_NONE);

  if (r_scale) {
    *r_scale = scale;
  }
}

static void renamebutton_cb(bContext *C, void * /*arg1*/, char *oldname)
{
  char newname[FILE_MAX + 12];
  char orgname[FILE_MAX + 12];
  char filename[FILE_MAX + 12];
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  SpaceFile *sfile = (SpaceFile *)CTX_wm_space_data(C);
  ARegion *region = CTX_wm_region(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);

  BLI_path_join(orgname, sizeof(orgname), params->dir, oldname);
  STRNCPY(filename, params->renamefile);
  BLI_path_make_safe_filename(filename);
  BLI_path_join(newname, sizeof(newname), params->dir, filename);

  if (!STREQ(orgname, newname)) {
    errno = 0;
    if ((BLI_rename(orgname, newname) != 0) || !BLI_exists(newname)) {
      WM_reportf(RPT_ERROR, "Could not rename: %s", errno ? strerror(errno) : "unknown error");
      WM_report_banner_show(wm, win);
      /* Renaming failed, reset the name for further renaming handling. */
      STRNCPY(params->renamefile, oldname);
    }
    else {
      /* If rename is successful, set renamefile to newly renamed entry.
       * This is used later to select and scroll to the file.
       */
      STRNCPY(params->renamefile, filename);
    }

    /* Ensure we select and scroll to the renamed file.
     * This is done even if the rename fails as we want to make sure that the file we tried to
     * rename is still selected and in view. (it can move if something added files/folders to the
     * directory while we were renaming.
     */
    file_params_invoke_rename_postscroll(wm, win, sfile);
    /* to make sure we show what is on disk */
    ED_fileselect_clear(wm, sfile);
    ED_region_tag_redraw(region);
  }
}

static void draw_background(FileLayout *layout, View2D *v2d)
{
  const int item_height = layout->tile_h + (2 * layout->tile_border_y);
  int i;
  int sy;

  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  float col_alternating[4];
  UI_GetThemeColor4fv(TH_ROW_ALTERNATE, col_alternating);
  immUniformThemeColorBlend(TH_BACK, TH_ROW_ALTERNATE, col_alternating[3]);

  /* alternating flat shade background */
  for (i = 2; (i <= layout->rows + 1); i += 2) {
    sy = int(v2d->cur.ymax) - layout->offset_top - i * item_height - layout->tile_border_y;

    /* Offset pattern slightly to add scroll effect. */
    sy += round_fl_to_int(item_height * (v2d->tot.ymax - v2d->cur.ymax) / item_height);

    immRectf(pos,
             v2d->cur.xmin,
             float(sy),
             v2d->cur.xmax,
             float(sy + layout->tile_h + 2 * layout->tile_border_y));
  }

  immUnbindProgram();
}

static void draw_dividers(FileLayout *layout, View2D *v2d)
{
  /* vertical column dividers */

  const int step = (layout->tile_w + 2 * layout->tile_border_x);

  uint vertex_len = 0;
  int sx = int(v2d->tot.xmin);
  while (sx < v2d->cur.xmax) {
    sx += step;
    vertex_len += 4; /* vertex_count = 2 points per line * 2 lines per divider */
  }

  if (vertex_len > 0) {
    int v1[2], v2[2];
    uchar col_hi[3], col_lo[3];

    UI_GetThemeColorShade3ubv(TH_BACK, 30, col_hi);
    UI_GetThemeColorShade3ubv(TH_BACK, -30, col_lo);

    v1[1] = v2d->cur.ymax - layout->tile_border_y;
    v2[1] = v2d->cur.ymin;

    GPUVertFormat *format = immVertexFormat();
    uint pos = GPU_vertformat_attr_add(format, "pos", GPU_COMP_I32, 2, GPU_FETCH_INT_TO_FLOAT);
    uint color = GPU_vertformat_attr_add(
        format, "color", GPU_COMP_U8, 3, GPU_FETCH_INT_TO_FLOAT_UNIT);

    immBindBuiltinProgram(GPU_SHADER_3D_FLAT_COLOR);
    immBegin(GPU_PRIM_LINES, vertex_len);

    sx = int(v2d->tot.xmin);
    while (sx < v2d->cur.xmax) {
      sx += step;

      v1[0] = v2[0] = sx;
      immAttrSkip(color);
      immVertex2iv(pos, v1);
      immAttr3ubv(color, col_lo);
      immVertex2iv(pos, v2);

      v1[0] = v2[0] = sx + 1;
      immAttrSkip(color);
      immVertex2iv(pos, v1);
      immAttr3ubv(color, col_hi);
      immVertex2iv(pos, v2);
    }

    immEnd();
    immUnbindProgram();
  }
}

static void draw_columnheader_background(const FileLayout *layout, const View2D *v2d)
{
  uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

  immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
  immUniformThemeColorShade(TH_BACK, 11);

  immRectf(pos,
           v2d->cur.xmin,
           v2d->cur.ymax - layout->attribute_column_header_h,
           v2d->cur.xmax,
           v2d->cur.ymax);

  immUnbindProgram();
}

static void draw_columnheader_columns(const FileSelectParams *params,
                                      FileLayout *layout,
                                      const View2D *v2d,
                                      const uchar text_col[4])
{
  const float divider_pad = 0.2 * layout->attribute_column_header_h;
  int sx = v2d->cur.xmin, sy = v2d->cur.ymax;

  for (int column_type = 0; column_type < ATTRIBUTE_COLUMN_MAX; column_type++) {
    if (!file_attribute_column_type_enabled(params, FileAttributeColumnType(column_type), layout))
    {
      continue;
    }
    const FileAttributeColumn *column = &layout->attribute_columns[column_type];

    /* Active sort type triangle */
    if (params->sort == column->sort_type) {
      float tri_color[4];

      rgba_uchar_to_float(tri_color, text_col);
      UI_draw_icon_tri(sx + column->width - (0.3f * U.widget_unit) -
                           ATTRIBUTE_COLUMN_PADDING / 2.0f,
                       sy + (0.1f * U.widget_unit) - (layout->attribute_column_header_h / 2),
                       (params->flag & FILE_SORT_INVERT) ? 't' : 'v',
                       tri_color);
    }

    file_draw_string(sx + ATTRIBUTE_COLUMN_PADDING,
                     sy - layout->tile_border_y,
                     IFACE_(column->name),
                     column->width - 2 * ATTRIBUTE_COLUMN_PADDING,
                     layout->attribute_column_header_h - layout->tile_border_y,
                     UI_STYLE_TEXT_LEFT,
                     text_col);

    /* Separator line */
    if (column_type != COLUMN_NAME) {
      uint pos = GPU_vertformat_attr_add(
          immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);

      immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
      immUniformThemeColorShade(TH_BACK, -10);
      immBegin(GPU_PRIM_LINES, 2);
      immVertex2f(pos, sx - 1, sy - divider_pad);
      immVertex2f(pos, sx - 1, sy - layout->attribute_column_header_h + divider_pad);
      immEnd();
      immUnbindProgram();
    }

    sx += column->width;
  }

  /* Vertical separator lines line */
  {
    uint pos = GPU_vertformat_attr_add(immVertexFormat(), "pos", GPU_COMP_F32, 2, GPU_FETCH_FLOAT);
    immBindBuiltinProgram(GPU_SHADER_3D_UNIFORM_COLOR);
    immUniformThemeColorShade(TH_BACK, -10);
    immBegin(GPU_PRIM_LINES, 4);
    immVertex2f(pos, v2d->cur.xmin, sy);
    immVertex2f(pos, v2d->cur.xmax, sy);
    immVertex2f(pos, v2d->cur.xmin, sy - layout->attribute_column_header_h);
    immVertex2f(pos, v2d->cur.xmax, sy - layout->attribute_column_header_h);
    immEnd();
    immUnbindProgram();
  }
}

/**
 * Updates the stat string stored in file->entry if necessary.
 */
static const char *filelist_get_details_column_string(
    FileAttributeColumnType column,
    /* Generated string will be cached in the file, so non-const. */
    FileDirEntry *file,
    const bool compact,
    const bool update_stat_strings)
{
  switch (column) {
    case COLUMN_DATETIME:
      if (!(file->typeflag & FILE_TYPE_BLENDERLIB) && !FILENAME_IS_CURRPAR(file->relpath)) {
        if (file->draw_data.datetime_str[0] == '\0' || update_stat_strings) {
          char date[FILELIST_DIRENTRY_DATE_LEN], time[FILELIST_DIRENTRY_TIME_LEN];
          bool is_today, is_yesterday;

          BLI_filelist_entry_datetime_to_string(
              nullptr, file->time, compact, time, date, &is_today, &is_yesterday);

          if (!compact && (is_today || is_yesterday)) {
            STRNCPY(date, is_today ? IFACE_("Today") : IFACE_("Yesterday"));
          }
          SNPRINTF(file->draw_data.datetime_str, compact ? "%s" : "%s %s", date, time);
        }

        return file->draw_data.datetime_str;
      }
      break;
    case COLUMN_SIZE:
      if ((file->typeflag & (FILE_TYPE_BLENDER | FILE_TYPE_BLENDER_BACKUP)) ||
          !(file->typeflag & (FILE_TYPE_DIR | FILE_TYPE_BLENDERLIB)))
      {
        if (file->draw_data.size_str[0] == '\0' || update_stat_strings) {
          BLI_filelist_entry_size_to_string(
              nullptr, file->size, compact, file->draw_data.size_str);
        }

        return file->draw_data.size_str;
      }
      break;
    default:
      break;
  }

  return nullptr;
}

static void draw_details_columns(const FileSelectParams *params,
                                 const FileLayout *layout,
                                 FileDirEntry *file,
                                 const rcti *tile_draw_rect,
                                 const uchar text_col[4])
{
  const bool compact = FILE_LAYOUT_COMPACT(layout);
  const bool update_stat_strings = layout->width != layout->curr_size;
  int sx = tile_draw_rect->xmin - layout->tile_border_x - (UI_UNIT_X * 0.1f);

  for (int column_type = 0; column_type < ATTRIBUTE_COLUMN_MAX; column_type++) {
    const FileAttributeColumn *column = &layout->attribute_columns[column_type];

    /* Name column is not a detail column (should already be drawn), always skip here. */
    if (column_type == COLUMN_NAME) {
      sx += column->width;
      continue;
    }
    if (!file_attribute_column_type_enabled(params, FileAttributeColumnType(column_type), layout))
    {
      continue;
    }

    const char *str = filelist_get_details_column_string(
        FileAttributeColumnType(column_type), file, compact, update_stat_strings);

    if (str) {
      file_draw_string(sx + ATTRIBUTE_COLUMN_PADDING,
                       tile_draw_rect->ymax - layout->tile_border_y,
                       IFACE_(str),
                       column->width - 2 * ATTRIBUTE_COLUMN_PADDING,
                       layout->tile_h,
                       eFontStyle_Align(column->text_align),
                       text_col);
    }

    sx += column->width;
  }
}

static rcti tile_draw_rect_get(const View2D *v2d,
                               const FileLayout *layout,
                               const eFileDisplayType display,
                               const int file_idx,
                               const int padx)
{
  int tile_pos_x, tile_pos_y;
  ED_fileselect_layout_tilepos(layout, file_idx, &tile_pos_x, &tile_pos_y);
  tile_pos_x += int(v2d->tot.xmin);
  tile_pos_y = int(v2d->tot.ymax - tile_pos_y);

  rcti rect;
  rect.xmin = tile_pos_x + padx;
  rect.xmax = rect.xmin + (ELEM(display, FILE_VERTICALDISPLAY, FILE_HORIZONTALDISPLAY) ?
                               layout->tile_w - (2 * padx) :
                               layout->tile_w);
  rect.ymax = tile_pos_y;
  rect.ymin = rect.ymax - layout->tile_h - layout->tile_border_y;

  return rect;
}

void file_draw_list(const bContext *C, ARegion *region)
{
  wmWindowManager *wm = CTX_wm_manager(C);
  wmWindow *win = CTX_wm_window(C);
  SpaceFile *sfile = CTX_wm_space_file(C);
  FileSelectParams *params = ED_fileselect_get_active_params(sfile);
  FileLayout *layout = ED_fileselect_get_layout(sfile, region);
  View2D *v2d = &region->v2d;
  FileList *files = sfile->files;
  FileDirEntry *file;
  uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
  int numfiles;
  int numfiles_layout;
  int offset;
  int column_width, textheight;
  int i;
  bool is_icon;
  eFontStyle_Align align;
  bool do_drag;
  uchar text_col[4];
  const bool draw_columnheader = (params->display == FILE_VERTICALDISPLAY);
  const float thumb_icon_aspect = std::min(64.0f / float(params->thumbnail_size), 4.0f);

  numfiles = filelist_files_ensure(files);

  if (params->display != FILE_IMGDISPLAY) {
    draw_background(layout, v2d);
    draw_dividers(layout, v2d);
  }

  offset = ED_fileselect_layout_offset(
      layout, int(region->v2d.cur.xmin), int(-region->v2d.cur.ymax));
  if (offset < 0) {
    offset = 0;
  }

  numfiles_layout = ED_fileselect_layout_numfiles(layout, region);

  /* adjust, so the next row is already drawn when scrolling */
  if (layout->flag & FILE_LAYOUT_HOR) {
    numfiles_layout += layout->rows;
  }
  else {
    numfiles_layout += layout->flow_columns;
  }

  filelist_file_cache_slidingwindow_set(files, numfiles_layout);

  column_width = (FILE_IMGDISPLAY == params->display) ?
                     layout->tile_w :
                     round_fl_to_int(layout->attribute_columns[COLUMN_NAME].width);
  textheight = int(layout->textheight * 3.0 / 2.0 + 0.5);

  align = (FILE_IMGDISPLAY == params->display) ? UI_STYLE_TEXT_CENTER : UI_STYLE_TEXT_LEFT;

  if (numfiles > 0) {
    const bool success = filelist_file_cache_block(
        files, min_ii(offset + (numfiles_layout / 2), numfiles - 1));
    BLI_assert(success);
    UNUSED_VARS_NDEBUG(success);

    filelist_cache_previews_update(files);

    /* Handle preview timer here,
     * since it's filelist_file_cache_block() and filelist_cache_previews_update()
     * which controls previews task. */
    {
      const bool previews_running = filelist_cache_previews_running(files) &&
                                    !filelist_cache_previews_done(files);
      //          printf("%s: preview task: %d\n", __func__, previews_running);
      if (previews_running && !sfile->previews_timer) {
        sfile->previews_timer = WM_event_timer_add_notifier(
            wm, win, NC_SPACE | ND_SPACE_FILE_PREVIEW, 0.01);
      }
      if (!previews_running && sfile->previews_timer) {
        /* Preview is not running, no need to keep generating update events! */
        //              printf("%s: Inactive preview task, sleeping!\n", __func__);
        WM_event_timer_remove_notifier(wm, win, sfile->previews_timer);
        sfile->previews_timer = nullptr;
      }
    }
  }

  BLF_batch_draw_begin();

  UI_GetThemeColor4ubv(TH_TEXT, text_col);

  for (i = offset; (i < numfiles) && (i < offset + numfiles_layout); i++) {
    eDirEntry_SelectFlag file_selflag;
    const int padx = 0.1f * UI_UNIT_X;
    int icon_ofs = 0;

    const rcti tile_draw_rect = tile_draw_rect_get(
        v2d, layout, eFileDisplayType(params->display), i, padx);

    file = filelist_file(files, i);
    file_selflag = filelist_entry_select_get(sfile->files, file, CHECK_ALL);

    char path[FILE_MAX_LIBEXTRA];
    filelist_file_get_full_path(files, file, path);

    if (!(file_selflag & FILE_SEL_EDITING)) {
      if ((params->highlight_file == i) || (file_selflag & FILE_SEL_HIGHLIGHTED) ||
          (file_selflag & FILE_SEL_SELECTED))
      {
        int colorid = (file_selflag & FILE_SEL_SELECTED) ? TH_HILITE : TH_BACK;
        int shade = (params->highlight_file == i) || (file_selflag & FILE_SEL_HIGHLIGHTED) ? 35 :
                                                                                             0;
        BLI_assert(i == 0 || !FILENAME_IS_CURRPAR(file->relpath));

        rcti tile_bg_rect = tile_draw_rect;
        /* One pixel downwards, places it more in the center. */
        BLI_rcti_translate(&tile_bg_rect, 0, -U.pixelsize);
        draw_tile_background(&tile_bg_rect, colorid, shade);
      }
    }
    UI_draw_roundbox_corner_set(UI_CNR_NONE);

    /* don't drag parent or refresh items */
    do_drag = !FILENAME_IS_CURRPAR(file->relpath);
    const bool is_hidden = (file->attributes & FILE_ATTR_HIDDEN);
    const bool is_link = (file->attributes & FILE_ATTR_ANY_LINK);

    if (FILE_IMGDISPLAY == params->display) {
      const int icon = filelist_geticon(files, i, false);
      is_icon = false;
      const ImBuf *imb = filelist_getimage(files, i);
      if (!imb) {
        imb = filelist_geticon_image(files, i);
        is_icon = true;
      }

      float scale = 0;
      file_draw_preview(files,
                        file,
                        &tile_draw_rect,
                        thumb_icon_aspect,
                        imb,
                        icon,
                        layout,
                        is_icon,
                        is_hidden,
                        is_link,
                        /* Returns the scale which is needed below. */
                        &scale);
      if (do_drag) {
        file_add_preview_drag_but(
            sfile, block, layout, file, path, &tile_draw_rect, imb, icon, scale);
      }
    }
    else {
      const int icon = filelist_geticon(files, i, true);

      icon_ofs += ICON_DEFAULT_WIDTH_SCALE + 0.2f * UI_UNIT_X;

      /* Add dummy draggable button covering the icon and the label. */
      if (do_drag) {
        const uiStyle *style = UI_style_get();
        const int str_width = UI_fontstyle_string_width(&style->widget, file->name);
        const int drag_width = std::min(str_width + icon_ofs,
                                        int(column_width - ATTRIBUTE_COLUMN_PADDING));
        if (drag_width > 0) {
          uiBut *drag_but = uiDefBut(block,
                                     UI_BTYPE_LABEL,
                                     0,
                                     "",
                                     tile_draw_rect.xmin,
                                     tile_draw_rect.ymin - 1,
                                     drag_width,
                                     layout->tile_h + layout->tile_border_y * 2,
                                     nullptr,
                                     0,
                                     0,
                                     nullptr);
          UI_but_dragflag_enable(drag_but, UI_BUT_DRAG_FULL_BUT);
          file_but_enable_drag(drag_but, sfile, file, path, nullptr, icon, UI_SCALE_FAC);
          UI_but_func_tooltip_custom_set(drag_but,
                                         file_draw_tooltip_custom_func,
                                         file_tooltip_data_create(sfile, file),
                                         MEM_freeN);
        }
      }

      /* Add this after the fake draggable button, so the icon button tooltip is displayed. */
      uiBut *icon_but = file_add_icon_but(sfile,
                                          block,
                                          path,
                                          file,
                                          &tile_draw_rect,
                                          icon,
                                          ICON_DEFAULT_WIDTH_SCALE,
                                          ICON_DEFAULT_HEIGHT_SCALE,
                                          is_hidden);
      if (do_drag) {
        /* For some reason the dragging is unreliable for the icon button if we don't explicitly
         * enable dragging, even though the dummy drag button above covers the same area. */
        file_but_enable_drag(icon_but, sfile, file, path, nullptr, icon, UI_SCALE_FAC);
      }
    }

    if (file_selflag & FILE_SEL_EDITING) {
      const short width = (params->display == FILE_IMGDISPLAY) ?
                              column_width :
                              layout->attribute_columns[COLUMN_NAME].width -
                                  ATTRIBUTE_COLUMN_PADDING;

      uiBut *but = uiDefBut(block,
                            UI_BTYPE_TEXT,
                            1,
                            "",
                            tile_draw_rect.xmin + icon_ofs,
                            tile_draw_rect.ymin + layout->tile_border_y - 0.15f * UI_UNIT_X,
                            width - icon_ofs,
                            textheight,
                            params->renamefile,
                            1.0f,
                            float(sizeof(params->renamefile)),
                            "");
      UI_but_func_rename_set(but, renamebutton_cb, file);
      UI_but_flag_enable(but, UI_BUT_NO_UTF8); /* allow non utf8 names */
      UI_but_flag_disable(but, UI_BUT_UNDO);
      if (false == UI_but_active_only(C, region, block, but)) {
        /* Note that this is the only place where we can also handle a cancelled renaming. */

        file_params_rename_end(wm, win, sfile, file);

        /* After the rename button is removed, we need to make sure the view is redrawn once more,
         * in case selection changed. Usually UI code would trigger that redraw, but the rename
         * operator may have been called from a different region.
         * Tagging regions for redrawing while drawing is rightfully prevented. However, this
         * active button removing basically introduces handling logic to drawing code. So a
         * notifier should be an acceptable workaround. */
        WM_event_add_notifier_ex(wm, win, NC_SPACE | ND_SPACE_FILE_PARAMS, nullptr);

        file_selflag = filelist_entry_select_get(sfile->files, file, CHECK_ALL);
      }
    }

    /* file_selflag might have been modified by branch above. */
    if ((file_selflag & FILE_SEL_EDITING) == 0) {
      const int txpos = (params->display == FILE_IMGDISPLAY) ? tile_draw_rect.xmin :
                                                               tile_draw_rect.xmin + 1 + icon_ofs;
      const int typos = (params->display == FILE_IMGDISPLAY) ?
                            tile_draw_rect.ymin + layout->tile_border_y + layout->textheight :
                            tile_draw_rect.ymax - layout->tile_border_y;
      const int twidth = (params->display == FILE_IMGDISPLAY) ?
                             column_width :
                             column_width - 1 - icon_ofs - padx - layout->tile_border_x;
      file_draw_string(txpos, typos, file->name, float(twidth), textheight, align, text_col);
    }

    if (params->display != FILE_IMGDISPLAY) {
      draw_details_columns(params, layout, file, &tile_draw_rect, text_col);
    }
  }

  if (numfiles < 1) {
    const rcti tile_draw_rect = tile_draw_rect_get(
        v2d, layout, eFileDisplayType(params->display), 0, 0);
    const uiStyle *style = UI_style_get();

    const bool is_filtered = params->filter_search[0] != '\0';

    uchar text_col_mod[4];
    copy_v4_v4_uchar(text_col_mod, text_col);
    if (!is_filtered) {
      text_col_mod[3] /= 2;
    }

    const char *message = [&]() {
      if (!filelist_is_ready(files)) {
        return IFACE_("Loading...");
      }
      if (is_filtered) {
        return IFACE_("No results match the search filter");
      }
      return IFACE_("No items");
    }();

    UI_fontstyle_draw_simple(&style->widget,
                             tile_draw_rect.xmin + UI_UNIT_X,
                             tile_draw_rect.ymax - UI_UNIT_Y,
                             message,
                             text_col_mod);
  }

  BLF_batch_draw_end();

  UI_block_end(C, block);
  UI_block_draw(C, block);

  /* Draw last, on top of file list. */
  if (draw_columnheader) {
    draw_columnheader_background(layout, v2d);
    draw_columnheader_columns(params, layout, v2d, text_col);
  }

  if (numfiles != -1) {
    /* Only save current size if there is something to show. */
    layout->curr_size = layout->width;
  }
}

static void file_draw_invalid_asset_library_hint(const bContext *C,
                                                 const SpaceFile *sfile,
                                                 ARegion *region,
                                                 FileAssetSelectParams *asset_params)
{
  char library_ui_path[FILE_MAX_LIBEXTRA];
  file_path_to_ui_path(asset_params->base_params.dir, library_ui_path, sizeof(library_ui_path));

  uchar text_col[4];
  UI_GetThemeColor4ubv(TH_TEXT, text_col);

  const View2D *v2d = &region->v2d;
  const int pad = sfile->layout->tile_border_x;
  const int width = BLI_rctf_size_x(&v2d->tot) - (2 * pad);
  const int line_height = sfile->layout->textheight;
  int sx = v2d->tot.xmin + pad;
  /* For some reason no padding needed. */
  int sy = v2d->tot.ymax;

  {
    const char *message = RPT_("Path to asset library does not exist:");
    file_draw_string_multiline(sx, sy, message, width, line_height, text_col, nullptr, &sy);

    sy -= line_height;
    file_draw_string(sx, sy, library_ui_path, width, line_height, UI_STYLE_TEXT_LEFT, text_col);
  }

  /* Separate a bit further. */
  sy -= line_height * 2.2f;

  {
    UI_icon_draw(sx, sy - UI_UNIT_Y, ICON_INFO);

    const char *suggestion = RPT_(
        "Asset Libraries are local directories that can contain .blend files with assets inside.\n"
        "Manage Asset Libraries from the File Paths section in Preferences");
    file_draw_string_multiline(
        sx + UI_UNIT_X, sy, suggestion, width - UI_UNIT_X, line_height, text_col, nullptr, &sy);

    uiBlock *block = UI_block_begin(C, region, __func__, UI_EMBOSS);
    wmOperatorType *ot = WM_operatortype_find("SCREEN_OT_userpref_show", false);
    uiBut *but = uiDefIconTextButO_ptr(block,
                                       UI_BTYPE_BUT,
                                       ot,
                                       WM_OP_INVOKE_DEFAULT,
                                       ICON_PREFERENCES,
                                       WM_operatortype_name(ot, nullptr),
                                       sx + UI_UNIT_X,
                                       sy - line_height - UI_UNIT_Y * 1.2f,
                                       UI_UNIT_X * 8,
                                       UI_UNIT_Y,
                                       nullptr);
    PointerRNA *but_opptr = UI_but_operator_ptr_ensure(but);
    RNA_enum_set(but_opptr, "section", USER_SECTION_FILE_PATHS);

    UI_block_end(C, block);
    UI_block_draw(C, block);
  }
}

static void file_draw_invalid_library_hint(const bContext * /*C*/,
                                           const SpaceFile *sfile,
                                           ARegion *region,
                                           const char *blendfile_path,
                                           ReportList *reports)
{
  uchar text_col[4];
  UI_GetThemeColor4ubv(TH_TEXT, text_col);

  const View2D *v2d = &region->v2d;
  const int pad = sfile->layout->tile_border_x;
  const int width = BLI_rctf_size_x(&v2d->tot) - (2 * pad);
  const int line_height = sfile->layout->textheight;
  int sx = v2d->tot.xmin + pad;
  /* For some reason no padding needed. */
  int sy = v2d->tot.ymax;

  {
    const char *message = RPT_("Unreadable Blender library file:");
    file_draw_string_multiline(sx, sy, message, width, line_height, text_col, nullptr, &sy);

    sy -= line_height;
    file_draw_string(sx, sy, blendfile_path, width, line_height, UI_STYLE_TEXT_LEFT, text_col);
  }

  /* Separate a bit further. */
  sy -= line_height * 2.2f;

  LISTBASE_FOREACH (Report *, report, &reports->list) {
    const short report_type = report->type;
    if (report_type <= RPT_INFO) {
      continue;
    }

    int icon = ICON_INFO;
    if (report_type > RPT_WARNING) {
      icon = ICON_ERROR;
    }
    UI_icon_draw(sx, sy - UI_UNIT_Y, icon);

    file_draw_string_multiline(sx + UI_UNIT_X,
                               sy,
                               RPT_(report->message),
                               width - UI_UNIT_X,
                               line_height,
                               text_col,
                               nullptr,
                               &sy);
    sy -= line_height;
  }
}

bool file_draw_hint_if_invalid(const bContext *C, const SpaceFile *sfile, ARegion *region)
{
  char blendfile_path[FILE_MAX_LIBEXTRA];
  const bool is_asset_browser = ED_fileselect_is_asset_browser(sfile);
  const bool is_library_browser = !is_asset_browser &&
                                  filelist_islibrary(sfile->files, blendfile_path, nullptr);

  if (is_asset_browser) {
    FileAssetSelectParams *asset_params = ED_fileselect_get_asset_params(sfile);

    /* Check if the asset library exists. */
    if (!((asset_params->asset_library_ref.type == ASSET_LIBRARY_LOCAL) ||
          filelist_is_dir(sfile->files, asset_params->base_params.dir)))
    {
      file_draw_invalid_asset_library_hint(C, sfile, region, asset_params);
      return true;
    }
  }

  /* Check if the blendfile library is valid (has entries). */
  if (is_library_browser) {
    if (!filelist_is_ready(sfile->files)) {
      return false;
    }

    const int numfiles = filelist_files_num_entries(sfile->files);
    if (numfiles > 0) {
      return false;
    }

    /* This could all be part of the file-list loading:
     *   - When loading fails this could be saved in the file-list, e.g. when
     *     `BLO_blendhandle_from_file()` returns null in `filelist_readjob_list_lib()`, a
     *     `FL_HAS_INVALID_LIBRARY` file-list flag could be set.
     *   - Reports from it could also be stored in `FileList` rather than being ignored
     *     (`RPT_STORE` must be set!).
     *   - Then we could just check for `is_library_browser` and the `FL_HAS_INVALID_LIBRARY` flag
     *     here, and draw the hint with the reports in the file-list. (We would not draw a hint for
     *     recursive loading, even if the file-list has the "has invalid library" flag set, which
     *     seems like the wanted behavior.)
     *   - The call to BKE_blendfile_is_readable() would not be needed then.
     */
    if (!sfile->runtime->is_blendfile_status_set) {
      BKE_reports_clear(&sfile->runtime->is_blendfile_readable_reports);
      sfile->runtime->is_blendfile_readable = BKE_blendfile_is_readable(
          blendfile_path, &sfile->runtime->is_blendfile_readable_reports);
      sfile->runtime->is_blendfile_status_set = true;
    }
    if (!sfile->runtime->is_blendfile_readable) {
      file_draw_invalid_library_hint(
          C, sfile, region, blendfile_path, &sfile->runtime->is_blendfile_readable_reports);
      return true;
    }
  }

  return false;
}
