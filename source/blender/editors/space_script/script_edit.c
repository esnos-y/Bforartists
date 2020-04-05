/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spscript
 */

#include <stdio.h>
#include <string.h>

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_report.h"

#include "WM_api.h"
#include "WM_types.h"
#include "wm_event_system.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "ED_screen.h"

#include "script_intern.h"  // own include

#ifdef WITH_PYTHON
#  include "BPY_extern.h" /* BPY_script_exec */
#endif

static int run_pyfile_exec(bContext *C, wmOperator *op)
{
  char path[512];
  RNA_string_get(op->ptr, "filepath", path);
#ifdef WITH_PYTHON
  if (BPY_execute_filepath(C, path, op->reports)) {
    ARegion *region = CTX_wm_region(C);
    ED_region_tag_redraw(region);
    return OPERATOR_FINISHED;
  }
#else
  (void)C; /* unused */
#endif
  return OPERATOR_CANCELLED; /* FAIL */
}

void SCRIPT_OT_python_file_run(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Run Python File";
  ot->description = "Run Python file";
  ot->idname = "SCRIPT_OT_python_file_run";

  /* api callbacks */
  ot->exec = run_pyfile_exec;
  ot->poll = ED_operator_areaactive;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_INTERNAL;

  RNA_def_string_file_path(ot->srna, "filepath", NULL, FILE_MAX, "Path", "");
}

#ifdef WITH_PYTHON
static bool script_test_modal_operators(bContext *C)
{
  wmWindowManager *wm;
  wmWindow *win;

  wm = CTX_wm_manager(C);

  for (win = wm->windows.first; win; win = win->next) {
    LISTBASE_FOREACH (wmEventHandler *, handler_base, &win->modalhandlers) {
      if (handler_base->type == WM_HANDLER_TYPE_OP) {
        wmEventHandler_Op *handler = (wmEventHandler_Op *)handler_base;
        if (handler->op != NULL) {
          wmOperatorType *ot = handler->op->type;
          if (ot->rna_ext.srna) {
            return true;
          }
        }
      }
    }
  }

  return false;
}
#endif

static int script_reload_exec(bContext *C, wmOperator *op)
{

#ifdef WITH_PYTHON

  /* clear running operators */
  if (script_test_modal_operators(C)) {
    BKE_report(op->reports, RPT_ERROR, "Can't reload with running modal operators");
    return OPERATOR_CANCELLED;
  }

  WM_script_tag_reload();

  /* TODO, this crashes on netrender and keying sets, need to look into why
   * disable for now unless running in debug mode */
  WM_cursor_wait(1);
  BPY_execute_string(
      C, (const char *[]){"bpy", NULL}, "bpy.utils.load_scripts(reload_scripts=True)");
  WM_cursor_wait(0);
  WM_event_add_notifier(C, NC_WINDOW, NULL);
  return OPERATOR_FINISHED;
#else
  UNUSED_VARS(C, op);
  return OPERATOR_CANCELLED;
#endif
}

void SCRIPT_OT_reload(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Reload Scripts";
  ot->description = "Reload all Python Scripts, including the Bforartists UI";
  ot->idname = "SCRIPT_OT_reload";

  /* api callbacks */
  ot->exec = script_reload_exec;
}
