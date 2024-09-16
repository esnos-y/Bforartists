/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_vector.hh"
#include "DNA_anim_types.h"
#include "RNA_types.hh"

struct ID;
struct Main;
struct Scene;

struct KeyingSet;

struct AnimationEvalContext;
struct FCurve;

struct ReportList;
struct bContext;

struct EnumPropertyItem;
struct PointerRNA;
struct PropertyRNA;

namespace blender::animrig {
enum class ModifyKeyReturn;
enum class ModifyKeyMode;
}  // namespace blender::animrig

/* -------------------------------------------------------------------- */
/** \name Key-Framing Management
 * \{ */

/**
 * \brief Lesser Key-framing API call.
 *
 * Update integer/discrete flags of the FCurve (used when creating/inserting keyframes,
 * but also through RNA when editing an ID prop, see #37103).
 */
void update_autoflags_fcurve(FCurve *fcu, bContext *C, ReportList *reports, PointerRNA *ptr);

/* -------- */

/**
 * Add the given number of keyframes to the FCurve. Their coordinates are
 * uninitialized, so the curve should not be used without further attention.
 *
 * The newly created keys are selected, existing keys are not touched.
 *
 * This can be used to allocate all the keys at once, and then update them
 * afterwards.
 */
void ED_keyframes_add(FCurve *fcu, int num_keys_to_add);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keying Sets
 * \{ */

/* Forward declaration. For this struct which is declared a bit later. */
struct ExtensionRNA;
struct KeyingSetInfo;

/** Polling Callback for KeyingSets. */
using cbKeyingSet_Poll = bool (*)(KeyingSetInfo *ksi, bContext *C);
/** Context Iterator Callback for KeyingSets. */
using cbKeyingSet_Iterator = void (*)(KeyingSetInfo *ksi, bContext *C, KeyingSet *ks);
/** Property Specifier Callback for KeyingSets (called from iterators) */
using cbKeyingSet_Generate = void (*)(KeyingSetInfo *ksi,
                                      bContext *C,
                                      KeyingSet *ks,
                                      PointerRNA *ptr);

/** Callback info for 'Procedural' KeyingSets to use. */
struct KeyingSetInfo {
  KeyingSetInfo *next, *prev;

  /* info */
  /** Identifier used for class name, which KeyingSet instances reference as "Type-info Name". */
  char idname[64];
  /** identifier so that user can hook this up to a KeyingSet (used as label). */
  char name[64];
  /** Short help/description. */
  char description[1024]; /* #RNA_DYN_DESCR_MAX */
  /** Keying settings. */
  short keyingflag;

  /* polling callbacks */
  /** callback for polling the context for whether the right data is available. */
  cbKeyingSet_Poll poll;

  /* generate callbacks */
  /**
   * Iterator to use to go through collections of data in context
   * - this callback is separate from the 'adding' stage, allowing
   *   BuiltIn KeyingSets to be manually specified to use.
   */
  cbKeyingSet_Iterator iter;
  /** Generator to use to add properties based on the data found by iterator. */
  cbKeyingSet_Generate generate;

  /** RNA integration. */
  ExtensionRNA rna_ext;
};

/* -------- */

/**
 * Add another data source for Relative Keying Sets to be evaluated with.
 */
void ANIM_relative_keyingset_add_source(blender::Vector<PointerRNA> &sources,
                                        ID *id,
                                        StructRNA *srna,
                                        void *data);
void ANIM_relative_keyingset_add_source(blender::Vector<PointerRNA> &sources, ID *id);

/**
 * Given a #KeyingSet and context info, validate Keying Set's paths.
 * This is only really necessary with relative/built-in KeyingSets
 * where their list of paths is dynamically generated based on the
 * current context info.
 *
 * \note Passing sources as pointer because it can be a nullptr.
 *
 * \return 0 if succeeded, otherwise an error code: #eModifyKey_Returns.
 */
blender::animrig::ModifyKeyReturn ANIM_validate_keyingset(bContext *C,
                                                          blender::Vector<PointerRNA> *sources,
                                                          KeyingSet *keyingset);

/**
 * Use the specified #KeyingSet and context info (if required)
 * to add/remove various Keyframes on the specified frame.
 *
 * Modify keyframes for the channels specified by the KeyingSet.
 * This takes into account many of the different combinations of using KeyingSets.
 *
 * \returns the number of channels that key-frames were added or
 * an #eModifyKey_Returns value (always a negative number).
 */
int ANIM_apply_keyingset(bContext *C,
                         blender::Vector<PointerRNA> *sources,
                         KeyingSet *keyingset,
                         blender::animrig::ModifyKeyMode mode,
                         float cfra);

/* -------- */

/**
 * Find builtin #KeyingSet by name.
 *
 * \return The first builtin #KeyingSet with the given name
 */
KeyingSet *ANIM_builtin_keyingset_get_named(const char name[]);

/**
 * Find KeyingSet type info given a name.
 */
KeyingSetInfo *ANIM_keyingset_info_find_name(const char name[]);

/**
 * Check if the ID appears in the paths specified by the #KeyingSet.
 */
bool ANIM_keyingset_find_id(KeyingSet *keyingset, ID *id);

/**
 * Add the given KeyingSetInfo to the list of type infos,
 * and create an appropriate builtin set too.
 */
void ANIM_keyingset_info_register(KeyingSetInfo *keyingset_info);
/**
 * Remove the given #KeyingSetInfo from the list of type infos,
 * and also remove the builtin set if appropriate.
 */
void ANIM_keyingset_info_unregister(Main *bmain, KeyingSetInfo *keyingset_info);

/* cleanup on exit */
/* --------------- */

void ANIM_keyingset_infos_exit();

/* -------- */

/**
 * Get the active Keying Set for the given scene.
 */
KeyingSet *ANIM_scene_get_active_keyingset(const Scene *scene);

/**
 * Get the index of the Keying Set provided, for the given Scene.
 */
int ANIM_scene_get_keyingset_index(Scene *scene, KeyingSet *keyingset);

/**
 * Get Keying Set to use for Auto-Key-Framing some transforms.
 */
KeyingSet *ANIM_get_keyingset_for_autokeying(const Scene *scene, const char *transformKSName);

void ANIM_keyingset_visit_for_search(
    const bContext *C,
    PointerRNA *ptr,
    PropertyRNA *prop,
    const char *edit_text,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn);

void ANIM_keyingset_visit_for_search_no_poll(
    const bContext *C,
    PointerRNA *ptr,
    PropertyRNA *prop,
    const char *edit_text,
    blender::FunctionRef<void(StringPropertySearchVisitParams)> visit_fn);
/**
 * Dynamically populate an enum of Keying Sets.
 */
const EnumPropertyItem *ANIM_keying_sets_enum_itemf(bContext *C,
                                                    PointerRNA *ptr,
                                                    PropertyRNA *prop,
                                                    bool *r_free);

/**
 * Get the keying set from enum values generated in #ANIM_keying_sets_enum_itemf.
 *
 * Type is the Keying Set the user specified to use when calling the operator:
 * \param type:
 * - == 0: use scene's active Keying Set.
 * -  > 0: use a user-defined Keying Set from the active scene.
 * -  < 0: use a builtin Keying Set.
 */
KeyingSet *ANIM_keyingset_get_from_enum_type(Scene *scene, int type);
KeyingSet *ANIM_keyingset_get_from_idname(Scene *scene, const char *idname);

/**
 * Check if #KeyingSet can be used in the current context.
 */
bool ANIM_keyingset_context_ok_poll(bContext *C, KeyingSet *keyingset);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Drivers
 * \{ */

/** Flags for use by driver creation calls */
enum eCreateDriverFlags {
  /** create drivers with a default variable for nicer UI */
  CREATEDRIVER_WITH_DEFAULT_DVAR = (1 << 0),
  /** Create drivers with Generator FModifier (for backwards compatibility). */
  CREATEDRIVER_WITH_FMODIFIER = (1 << 1),
};

/** Heuristic to use for connecting target properties to driven ones */
enum eCreateDriver_MappingTypes {
  /** 1 to Many - Use the specified index, and drive all elements with it */
  CREATEDRIVER_MAPPING_1_N = 0,
  /** 1 to 1 - Only for the specified index on each side */
  CREATEDRIVER_MAPPING_1_1 = 1,
  /** Many to Many - Match up the indices one by one (only for drivers on vectors/arrays) */
  CREATEDRIVER_MAPPING_N_N = 2,

  /** None (Single Prop):
   * Do not create driver with any targets; these will get added later instead */
  CREATEDRIVER_MAPPING_NONE = 3,
  /** None (All Properties):
   * Do not create driver with any targets; these will get added later instead */
  CREATEDRIVER_MAPPING_NONE_ALL = 4,
};

/**
 * Mapping Types enum for operators.
 * \note Used by #ANIM_OT_driver_button_add and #UI_OT_eyedropper_driver.
 */
extern const EnumPropertyItem prop_driver_create_mapping_types[];

/* -------- */

enum eDriverFCurveCreationMode {
  /** Don't add anything if not found. */
  DRIVER_FCURVE_LOOKUP_ONLY = 0,
  /** Add with keyframes, for visual tweaking. */
  DRIVER_FCURVE_KEYFRAMES = 1,
  /** Add with generator, for script backwards compatibility. */
  DRIVER_FCURVE_GENERATOR = 2,
  /** Add without data, for pasting. */
  DRIVER_FCURVE_EMPTY = 3
};

/**
 * Get (or add relevant data to be able to do so) F-Curve from the driver stack,
 * for the given Animation Data block. This assumes that all the destinations are valid.
 *
 * \note This low-level function shouldn't be used directly for most tools,
 * although there are special cases where this approach is preferable.
 */
FCurve *verify_driver_fcurve(ID *id,
                             const char rna_path[],
                             int array_index,
                             eDriverFCurveCreationMode creation_mode);

FCurve *alloc_driver_fcurve(const char rna_path[],
                            int array_index,
                            eDriverFCurveCreationMode creation_mode);

/* -------- */

/**
 * \brief Main Driver Management API calls
 *
 * Add a new driver for the specified property on the given ID block,
 * and make it be driven by the specified target.
 *
 * This is intended to be used in conjunction with a modal "eyedropper"
 * for picking the variable that is going to be used to drive this one.
 *
 * \param flag: eCreateDriverFlags
 * \param driver_type: eDriver_Types
 * \param mapping_type: eCreateDriver_MappingTypes
 */
int ANIM_add_driver_with_target(ReportList *reports,
                                ID *dst_id,
                                const char dst_path[],
                                int dst_index,
                                ID *src_id,
                                const char src_path[],
                                int src_index,
                                short flag,
                                int driver_type,
                                short mapping_type);

/* -------- */

/**
 * \brief Main Driver Management API calls.
 *
 * Add a new driver for the specified property on the given ID block
 */
int ANIM_add_driver(
    ReportList *reports, ID *id, const char rna_path[], int array_index, short flag, int type);

/**
 * \brief Main Driver Management API calls.
 *
 * Remove the driver for the specified property on the given ID block.
 *
 * \return Whether any driver was removed.
 */
bool ANIM_remove_driver(ID *id, const char rna_path[], int array_index);

/* -------- */

/**
 * Clear copy-paste buffer for drivers.
 * \note This function frees any MEM_calloc'ed copy/paste buffer data.
 */
void ANIM_drivers_copybuf_free();

/**
 * Clear copy-paste buffer for driver variable sets.
 * \note This function frees any MEM_calloc'ed copy/paste buffer data.
 */
void ANIM_driver_vars_copybuf_free();

/* -------- */

/**
 * Returns whether there is a driver in the copy/paste buffer to paste.
 */
bool ANIM_driver_can_paste();

/**
 * \brief Main Driver Management API calls.
 *
 * Make a copy of the driver for the specified property on the given ID block.
 */
bool ANIM_copy_driver(
    ReportList *reports, ID *id, const char rna_path[], int array_index, short flag);

/**
 * \brief Main Driver Management API calls.
 *
 * Add a new driver for the specified property on the given ID block or replace an existing one
 * with the driver + driver-curve data from the buffer.
 */
bool ANIM_paste_driver(
    ReportList *reports, ID *id, const char rna_path[], int array_index, short flag);

/* -------- */

/**
 * Checks if there are driver variables in the copy/paste buffer.
 */
bool ANIM_driver_vars_can_paste();

/**
 * Copy the given driver's variables to the buffer.
 */
bool ANIM_driver_vars_copy(ReportList *reports, FCurve *fcu);

/**
 * Paste the variables in the buffer to the given FCurve.
 */
bool ANIM_driver_vars_paste(ReportList *reports, FCurve *fcu, bool replace);

/* -------- */

/**
 * Create a driver & variable that reads the specified property,
 * and store it in the buffers for Paste Driver and Paste Variables.
 */
void ANIM_copy_as_driver(ID *target_id, const char *target_path, const char *var_name);

/** \} */

/* -------------------------------------------------------------------- */
/** \name Keyframe Checking
 * \{ */

/**
 * \brief Lesser Keyframe Checking API call.
 *
 * Checks if some F-Curve has a keyframe for a given frame.
 * \note Used for the buttons to check for keyframes.
 */
bool fcurve_frame_has_keyframe(const FCurve *fcu, float frame);

/**
 * \brief Lesser Keyframe Checking API call.
 *
 * - Returns whether the current value of a given property differs from the interpolated value.
 * - Used for button drawing.
 */
bool fcurve_is_changed(PointerRNA ptr,
                       PropertyRNA *prop,
                       FCurve *fcu,
                       const AnimationEvalContext *anim_eval_context);

/**
 * \brief Main Keyframe Checking API call.
 *
 * Checks whether a keyframe exists for the given ID-block one the given frame.
 * It is recommended to call this method over the other keyframe-checkers directly,
 * in case some detail of the implementation changes...
 * \param frame: The value of this is quite often result of #BKE_scene_ctime_get()
 */
bool id_frame_has_keyframe(ID *id, float frame);

/* Names for builtin keying sets so we don't confuse these with labels/text,
 * defined in python script: `keyingsets_builtins.py`. */

#define ANIM_KS_LOCATION_ID "Location"
#define ANIM_KS_ROTATION_ID "Rotation"
#define ANIM_KS_SCALING_ID "Scaling"
#define ANIM_KS_LOC_ROT_SCALE_ID "LocRotScale"
#define ANIM_KS_LOC_ROT_SCALE_CPROP_ID "LocRotScaleCProp"
#define ANIM_KS_AVAILABLE_ID "Available"
#define ANIM_KS_WHOLE_CHARACTER_ID "WholeCharacter"
#define ANIM_KS_WHOLE_CHARACTER_SELECTED_ID "WholeCharacterSelected"

/** \} */
