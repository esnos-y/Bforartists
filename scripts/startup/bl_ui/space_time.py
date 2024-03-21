# SPDX-FileCopyrightText: 2009-2023 Blender Authors
#
# SPDX-License-Identifier: GPL-2.0-or-later

import bpy
from bpy.types import Menu, Panel
from bpy.app.translations import contexts as i18n_contexts


# Header buttons for timeline header (play, etc.)
class TIME_HT_editor_buttons:

    @staticmethod
    def draw_header(context, layout):
        scene = context.scene
        tool_settings = context.tool_settings
        screen = context.screen
        anim = bpy.ops.anim #BFA

        layout.separator_spacer()

        row = layout.row(align=True)
### BFA - moved to another area.
#        row.prop(tool_settings, "use_keyframe_insert_auto", text="", toggle=True)
#        sub = row.row(align=True)
#        sub.active = tool_settings.use_keyframe_insert_auto
#        sub.popover(
#            panel="TIME_PT_auto_keyframing",
#            text="",
#        )
#        row = layout.row(align=True)

        row.operator("screen.frame_jump", text="", icon='REW').end = False
        row.operator("screen.keyframe_jump", text="", icon='PREV_KEYFRAME').next = False
        if not screen.is_animation_playing:
            # if using JACK and A/V sync:
            #   hide the play-reversed button
            #   since JACK transport doesn't support reversed playback
            if scene.sync_mode == 'AUDIO_SYNC' and context.preferences.system.audio_device == 'JACK':
                row.scale_x = 2
                row.operator("screen.animation_play", text="", icon='PLAY')
                row.scale_x = 1
            else:
                row.operator("screen.animation_play", text="", icon='PLAY_REVERSE').reverse = True
                row.operator("screen.animation_play", text="", icon='PLAY')
        else:
            row.scale_x = 2
            row.operator("screen.animation_play", text="", icon='PAUSE')
            row.scale_x = 1
        row.operator("screen.keyframe_jump", text="", icon='NEXT_KEYFRAME').next = True
        row.operator("screen.frame_jump", text="", icon='FF').end = True
        row.operator("screen.animation_cancel", text = "", icon = 'LOOP_BACK').restore_frame = True

        row = layout.row()
        if scene.show_subframe:
            row.scale_x = 1.15
            row.prop(scene, "frame_float", text="")
        else:
            row.scale_x = 0.95
            row.prop(scene, "frame_current", text="")

        row = layout.row(align=True)
        row.prop(scene, "use_preview_range", text="", toggle=True)
        row.operator("anim.start_frame_set", text="", icon = 'SET_POSITION')
        sub = row.row(align=True)
        sub.scale_x = 0.8
        if not scene.use_preview_range:
            sub.prop(scene, "frame_start", text="Start")
            sub.prop(scene, "frame_end", text="End")

        else:
            sub.prop(scene, "frame_preview_start", text="Start")
            sub.prop(scene, "frame_preview_end", text="End")

        row.operator("anim.end_frame_set", text="", icon = 'SET_POSITION')

        row.separator()

        row.operator("anim.keyframe_insert", text="", icon='KEYFRAMES_INSERT') # BFA - updated icon
        row.operator("anim.keyframe_delete_v3d", text="", icon='KEYFRAMES_REMOVE') # BFA - updated to work like it would in the 3D View (as expected)

        layout.separator_spacer()

        row = layout.row(align=True)
        sub = row.row(align=True)
        if tool_settings.use_keyframe_insert_auto:
            sub.popover(panel="TIME_PT_auto_keyframing", text="")
        row.prop(tool_settings, "use_keyframe_insert_auto", text="", toggle=True)
        row.prop_search(scene.keying_sets_all, "active", scene, "keying_sets_all", text="")

        row.popover(panel="TIME_PT_playback", text="Playback")
        row.popover(panel="TIME_PT_keyframing_settings", text="Keying")

        row.popover(panel = "TIME_PT_view_view_options", text = "")

class TIME_MT_editor_menus(Menu):
    bl_idname = "TIME_MT_editor_menus"
    bl_label = ""

    def draw(self, context):
        layout = self.layout
        horizontal = (layout.direction == 'VERTICAL')
        st = context.space_data
        if horizontal:
            row = layout.row()
            sub = row.row(align=True)
        else:
            sub = layout

## BFA - moved to header options
#        sub.popover(
#            panel="TIME_PT_playback",
#            text="Playback",
#        )
#        sub.popover(
#            panel="TIME_PT_keyframing_settings",
#            text="Keying",
#            text_ctxt=i18n_contexts.id_windowmanager,
#        )

#        # Add a separator to keep the popover button from aligning with the menu button.
#        sub.separator(factor=0.4)
        if horizontal:
            sub = row.row(align=True)

        sub.menu("TIME_MT_view")
        if st.show_markers:
            sub.menu("TIME_MT_marker")
            sub.menu("DOPESHEET_MT_select")#BFA


class TIME_MT_marker(Menu):
    bl_label = "Marker"

    def draw(self, context):
        layout = self.layout

        marker_menu_generic(layout, context)

class TIME_MT_view(Menu):
    bl_label = "View"

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        st = context.space_data

        layout.prop(st, "show_region_ui")#BFA
        layout.prop(st, "show_region_hud")
        layout.prop(st, "show_region_channels")

		# BFA - moved below
        layout.separator()
		# BFA - these properties were moved to the header options properties
        layout.operator_context = 'INVOKE_REGION_WIN'
        layout.operator("view2d.zoom_in", icon = "ZOOM_IN")
        layout.operator("view2d.zoom_out", icon = "ZOOM_OUT")
        layout.operator("view2d.zoom_border", text = "Zoom Border", icon = "ZOOM_BORDER")

        layout.separator()

        # NOTE: "action" now, since timeline is in the dopesheet editor, instead of as own editor
        layout.operator("action.view_all", icon = "VIEWALL")
        layout.operator("action.view_frame", icon = "VIEW_FRAME" )

        layout.separator()

        layout.prop(st, "show_markers")

        layout.separator()

        layout.prop(st, "show_locked_time")
        layout.prop(st, "show_seconds")

        layout.separator()

        layout.prop(st, "show_region_hud")

## BFA - moved to the options panel exclusively.
#class TIME_MT_cache(Menu):
#    bl_label = "Cache"

#    def draw(self, context):
#        layout = self.layout

#        st = context.space_data

#        layout.prop(st, "show_cache")

#        layout.separator()

#        col = layout.column()
#        col.enabled = st.show_cache
#        col.prop(st, "cache_softbody")
#        col.prop(st, "cache_particles")
#        col.prop(st, "cache_cloth")
#        col.prop(st, "cache_simulation_nodes")
#        col.prop(st, "cache_smoke")
#        col.prop(st, "cache_dynamicpaint")
#        col.prop(st, "cache_rigidbody")


def marker_menu_generic(layout, context):

    # layout.operator_context = 'EXEC_REGION_WIN'

    layout.column()
    layout.operator("marker.add", text = "Add Marker", icon = "MARKER")
    layout.operator("marker.duplicate", text="Duplicate Marker", icon = "DUPLICATE")

    if len(bpy.data.scenes) > 10:
        layout.operator_context = 'INVOKE_DEFAULT'
        layout.operator("marker.make_links_scene", text="Duplicate Marker to Scene...", icon='OUTLINER_OB_EMPTY')
    else:
        layout.operator_menu_enum("marker.make_links_scene", "scene", text="Duplicate Marker to Scene")

    layout.operator("marker.delete", text="Delete Marker", icon = "DELETE")

    layout.separator()

    props = layout.operator("wm.call_panel", text="Rename Marker", icon = "RENAME")
    props.name = "TOPBAR_PT_name_marker"
    props.keep_open = False
    layout.operator("marker.move", text="Move Marker", icon = "TRANSFORM_MOVE")

    layout.separator()

    layout.menu('NLA_MT_marker_select')

    layout.separator()

    layout.operator("screen.marker_jump", text="Jump to Next Marker", icon = "NEXT_KEYFRAME").next = True
    layout.operator("screen.marker_jump", text="Jump to Previous Marker", icon = "PREV_KEYFRAME").next = False


###################################


class TimelinePanelButtons:
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'UI'

    @staticmethod
    def has_timeline(context):
        return context.space_data.mode == 'TIMELINE'


class TIME_PT_playback(TimelinePanelButtons, Panel):
    bl_label = "Playback"
    bl_region_type = 'HEADER'
    bl_ui_units_x = 13

    def draw(self, context):
        layout = self.layout
        layout.use_property_split = False
        layout.use_property_decorate = False

        screen = context.screen
        scene = context.scene

        col = layout.column(align = True)
        col.label(text = "Audio")
        row = col.row()
        row.separator()
        row.use_property_split = True
        row.prop(scene, "sync_mode", text="Sync Mode")
        row = col.row()
        row.separator()
        row.prop(scene, "use_audio_scrub", text="Scrubbing")
        row = col.row()
        row.separator()
        row.prop(scene, "use_audio", text="Play Audio")

        col = layout.column(align = True)
        col.label(text = "Playback")
        row = col.row()
        row.separator()
        row.prop(scene, "lock_frame_selection_to_range", text="Limit to Frame Range")
        row = col.row()
        row.separator()
        row.prop(screen, "use_follow", text="Follow Current Frame")

        col = layout.column(align = True)
        col.label(text = "Play In")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_top_left_3d_editor", text="Active Editor")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_3d_editors", text="3D Viewport")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_animation_editors", text="Animation Editors")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_image_editors", text="Image Editor")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_properties_editors", text="Properties Editor")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_clip_editors", text="Movie Clip Editor")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_node_editors", text="Node Editors")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_sequence_editors", text="Video Sequencer")
        row = col.row()
        row.separator()
        row.prop(screen, "use_play_spreadsheet_editors", text="Spreadsheet")

        col = layout.column()
        col.prop(scene, "show_subframe", text="Show Subframes")


class TIME_PT_keyframing_settings(TimelinePanelButtons, Panel):
    bl_label = "Keyframing Settings"
    bl_options = {'HIDE_HEADER'}
    bl_region_type = 'HEADER'

    @classmethod
    def poll(cls, context):
        # only for timeline editor
        return cls.has_timeline(context)

    def draw(self, context):
        layout = self.layout

        scene = context.scene
        tool_settings = context.tool_settings
        prefs = context.preferences

        col = layout.column(align=True)
        col.label(text="New Keyframe Type")
        col.prop(tool_settings, "keyframe_type", text="")

        col = layout.column(align=True)
        col.label(text="Auto Keyframing")
        row = col.row()
        row.prop(tool_settings, "auto_keying_mode", text="")
        row.prop(tool_settings, "use_keyframe_insert_keyingset", text="")

        if not prefs.edit.use_keyframe_insert_available:
            layout.prop(tool_settings, "use_record_with_nla", text="Layered Recording")

        layout.prop(tool_settings, "use_keyframe_cycle_aware")


############# Panels in sidebar #########################


class TIME_PT_view_view_options(TimelinePanelButtons, Panel):
    bl_label = "View Options"
    bl_category = "View"
    bl_space_type = 'DOPESHEET_EDITOR'
    bl_region_type = 'HEADER'

    @classmethod
    def poll(cls, context):
        # only for timeline editor
        return cls.has_timeline(context)

    def draw(self, context):
        sc = context.scene
        layout = self.layout

        st = context.space_data
        scene = context.scene
        tool_settings = context.tool_settings

        col = layout.column()


        col.prop(st.dopesheet, "show_only_errors")

        col.separator()

        col.prop(scene, "show_keys_from_selected_only", text = "Only show selected")
        col.prop(st, "show_seconds")
        col.prop(st, "show_locked_time")

        col.prop(st, "show_markers")
        col.prop(tool_settings, "lock_markers")

        col.separator()

        row = layout.row()
        row.use_property_split = False
        row.prop(st, "show_cache")
        row.use_property_split = True

        if st.show_cache:
            row.label(icon='DISCLOSURE_TRI_DOWN')
            row = layout.row()

            row.separator() #indent
            col = row.column()
            col.prop(st, "cache_softbody")
            col.prop(st, "cache_particles")
            col.prop(st, "cache_cloth")
            col.prop(st, "cache_simulation_nodes")
            col.prop(st, "cache_smoke")
            col.prop(st, "cache_dynamicpaint")
            col.prop(st, "cache_rigidbody")
        else:
            row.label(icon='DISCLOSURE_TRI_RIGHT')


class TIME_PT_auto_keyframing(TimelinePanelButtons, Panel):
    bl_label = "Auto Keyframing"
    bl_options = {'HIDE_HEADER'}
    bl_region_type = 'HEADER'
    bl_ui_units_x = 9

    @classmethod
    def poll(cls, context):
        # Only for timeline editor.
        return cls.has_timeline(context)

    def draw(self, context):
        layout = self.layout

        tool_settings = context.tool_settings
        prefs = context.preferences

        layout.active = tool_settings.use_keyframe_insert_auto

        layout.prop(tool_settings, "auto_keying_mode", expand=True)

        col = layout.column(align=True)
        col.prop(tool_settings, "use_keyframe_insert_keyingset", text="Only Active Keying Set", toggle=False)
        if not prefs.edit.use_keyframe_insert_available:
            col.prop(tool_settings, "use_record_with_nla", text="Layered Recording")

        col.prop(tool_settings, "use_keyframe_cycle_aware")


###################################

classes = (
    TIME_MT_editor_menus,
    TIME_MT_marker,
    TIME_MT_view,
    #TIME_MT_cache, #BFA - not used, replaced by the PT_view_view_options
    TIME_PT_playback,
    TIME_PT_keyframing_settings,
    TIME_PT_view_view_options,
    TIME_PT_auto_keyframing,
)

if __name__ == "__main__":  # only for live edit.
    from bpy.utils import register_class
    for cls in classes:
        register_class(cls)
