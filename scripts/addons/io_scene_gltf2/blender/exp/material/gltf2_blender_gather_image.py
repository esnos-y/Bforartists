# SPDX-FileCopyrightText: 2018-2021 The glTF-Blender-IO authors
#
# SPDX-License-Identifier: Apache-2.0

import bpy
import typing
import os

from ....io.com import gltf2_io
from ....io.com.gltf2_io_path import path_to_uri
from ....io.exp import gltf2_io_binary_data, gltf2_io_image_data
from ....io.com import gltf2_io_debug
from ....io.exp.gltf2_io_user_extensions import export_user_extensions
from ..gltf2_blender_gather_cache import cached
from .extensions.gltf2_blender_image import Channel, ExportImage, FillImage
from ..gltf2_blender_get import get_tex_from_socket

@cached
def gather_image(
        blender_shader_sockets: typing.Tuple[bpy.types.NodeSocket],
        default_sockets: typing.Tuple[bpy.types.NodeSocket],
        export_settings):
    if not __filter_image(blender_shader_sockets, export_settings):
        return None, None, None

    image_data = __get_image_data(blender_shader_sockets, default_sockets, export_settings)
    if image_data.empty():
        # The export image has no data
        return None, None, None

    mime_type = __gather_mime_type(blender_shader_sockets, image_data, export_settings)
    name = __gather_name(image_data, export_settings)

    factor = None

    if image_data.original is None:
        uri, factor_uri = __gather_uri(image_data, mime_type, name, export_settings)
    else:
        # Retrieve URI relative to exported glTF files
        uri = __gather_original_uri(image_data.original.filepath, export_settings)
        # In case we can't retrieve image (for example packed images, with original moved)
        # We don't create invalid image without uri
        factor_uri = None
        if uri is None: return None, None, None

    buffer_view, factor_buffer_view = __gather_buffer_view(image_data, mime_type, name, export_settings)

    factor = factor_uri if uri is not None else factor_buffer_view

    image = __make_image(
        buffer_view,
        __gather_extensions(blender_shader_sockets, export_settings),
        __gather_extras(blender_shader_sockets, export_settings),
        mime_type,
        name,
        uri,
        export_settings
    )

    export_user_extensions('gather_image_hook', export_settings, image, blender_shader_sockets)

    # We also return image_data, as it can be used to generate same file with another extension for webp management
    return image, image_data, factor

def __gather_original_uri(original_uri, export_settings):

    path_to_image = bpy.path.abspath(original_uri)
    if not os.path.exists(path_to_image): return None
    try:
        rel_path = os.path.relpath(
            path_to_image,
            start=export_settings['gltf_filedirectory'],
        )
    except ValueError:
        # eg. because no relative path between C:\ and D:\ on Windows
        return None
    return path_to_uri(rel_path)


@cached
def __make_image(buffer_view, extensions, extras, mime_type, name, uri, export_settings):
    return gltf2_io.Image(
        buffer_view=buffer_view,
        extensions=extensions,
        extras=extras,
        mime_type=mime_type,
        name=name,
        uri=uri
    )


def __filter_image(sockets, export_settings):
    if not sockets:
        return False
    return True


@cached
def __gather_buffer_view(image_data, mime_type, name, export_settings):
    if export_settings['gltf_format'] != 'GLTF_SEPARATE':
        data, factor = image_data.encode(mime_type, export_settings)
        return gltf2_io_binary_data.BinaryData(data=data), factor
    return None, None


def __gather_extensions(sockets, export_settings):
    return None


def __gather_extras(sockets, export_settings):
    return None


def __gather_mime_type(sockets, export_image, export_settings):
    # force png or webp if Alpha contained so we can export alpha
    for socket in sockets:
        if socket.name == "Alpha":
            if export_settings["gltf_image_format"] == "WEBP":
                return "image/webp"
            else:
                # If we keep image as is (no channel composition), we need to keep original format (for webp)
                image = export_image.blender_image()
                if image is not None and __is_blender_image_a_webp(image):
                    return "image/webp"
                return "image/png"

    if export_settings["gltf_image_format"] == "AUTO":
        if export_image.original is None: # We are going to create a new image
            image = export_image.blender_image()
        else:
            # Using original image
            image = export_image.original

        if image is not None and __is_blender_image_a_jpeg(image):
            return "image/jpeg"
        elif image is not None and __is_blender_image_a_webp(image):
            return "image/webp"
        return "image/png"

    elif export_settings["gltf_image_format"] == "WEBP":
        return "image/webp"
    elif export_settings["gltf_image_format"] == "JPEG":
        return "image/jpeg"


def __gather_name(export_image, export_settings):
    if export_image.original is None:
        # Find all Blender images used in the ExportImage
        imgs = []
        for fill in export_image.fills.values():
            if isinstance(fill, FillImage):
                img = fill.image
                if img not in imgs:
                    imgs.append(img)

        # If all the images have the same path, use the common filename
        filepaths = set(img.filepath for img in imgs)
        if len(filepaths) == 1:
            filename = os.path.basename(list(filepaths)[0])
            name, extension = os.path.splitext(filename)
            if extension.lower() in ['.png', '.jpg', '.jpeg']:
                if name:
                    return name

        # Combine the image names: img1-img2-img3
        names = []
        for img in imgs:
            name, extension = os.path.splitext(img.name)
            names.append(name)
        name = '-'.join(names)
        return name or 'Image'
    else:
        return export_image.original.name


@cached
def __gather_uri(image_data, mime_type, name, export_settings):
    if export_settings['gltf_format'] == 'GLTF_SEPARATE':
        # as usual we just store the data in place instead of already resolving the references
        data, factor = image_data.encode(mime_type, export_settings)
        return gltf2_io_image_data.ImageData(
            data=data,
            mime_type=mime_type,
            name=name
        ), factor

    return None, None


def __get_image_data(sockets, default_sockets, export_settings) -> ExportImage:
    # For shared resources, such as images, we just store the portion of data that is needed in the glTF property
    # in a helper class. During generation of the glTF in the exporter these will then be combined to actual binary
    # resources.
    results = [get_tex_from_socket(socket) for socket in sockets]

    # Check if we need a simple mapping or more complex calculation
    # There is currently no complex calculation for any textures
    return __get_image_data_mapping(sockets, default_sockets, results, export_settings)

def __get_image_data_mapping(sockets, default_sockets, results, export_settings) -> ExportImage:
    """
    Simple mapping
    Will fit for most of exported textures : RoughnessMetallic, Basecolor, normal, ...
    """
    composed_image = ExportImage()

    default_metallic = None
    default_roughness = None
    if "Metallic" in [s.name for s in default_sockets]:
        default_metallic = [s for s in default_sockets if s.name == "Metallic"][0].default_value
    if "Roughness" in [s.name for s in default_sockets]:
        default_roughness = [s for s in default_sockets if s.name == "Roughness"][0].default_value

    for result, socket in zip(results, sockets):
        # Assume that user know what he does, and that channels/images are already combined correctly for pbr
        # If not, we are going to keep only the first texture found
        # Example : If user set up 2 or 3 different textures for Metallic / Roughness / Occlusion
        # Only 1 will be used at export
        # This Warning is displayed in UI of this option
        if export_settings['gltf_keep_original_textures']:
            composed_image = ExportImage.from_original(result.shader_node.image)

        else:
            # rudimentarily try follow the node tree to find the correct image data.
            src_chan = Channel.R
            for elem in result.path:
                if isinstance(elem.from_node, bpy.types.ShaderNodeSeparateColor):
                    src_chan = {
                        'Red': Channel.R,
                        'Green': Channel.G,
                        'Blue': Channel.B,
                    }[elem.from_socket.name]
                if elem.from_socket.name == 'Alpha':
                    src_chan = Channel.A

            dst_chan = None

            # some sockets need channel rewriting (gltf pbr defines fixed channels for some attributes)
            if socket.name == 'Metallic':
                dst_chan = Channel.B
            elif socket.name == 'Roughness':
                dst_chan = Channel.G
            elif socket.name == 'Occlusion':
                dst_chan = Channel.R
            elif socket.name == 'Alpha':
                dst_chan = Channel.A
            elif socket.name == 'Coat Weight':
                dst_chan = Channel.R
            elif socket.name == 'Coat Roughness':
                dst_chan = Channel.G
            elif socket.name == 'Thickness': # For KHR_materials_volume
                dst_chan = Channel.G
            elif socket.name == "Specular IOR Level": # For KHR_material_specular
                dst_chan = Channel.A
            elif socket.name == "Sheen Roughness": # For KHR_materials_sheen
                dst_chan = Channel.A

            if dst_chan is not None:
                composed_image.fill_image(result.shader_node.image, dst_chan, src_chan)

                # Since metal/roughness are always used together, make sure
                # the other channel is filled.
                if socket.name == 'Metallic' and not composed_image.is_filled(Channel.G):
                    if default_roughness is not None:
                        composed_image.fill_with(Channel.G, default_roughness)
                    else:
                        composed_image.fill_white(Channel.G)
                elif socket.name == 'Roughness' and not composed_image.is_filled(Channel.B):
                    if default_metallic is not None:
                        composed_image.fill_with(Channel.B, default_metallic)
                    else:
                        composed_image.fill_white(Channel.B)
            else:
                # copy full image...eventually following sockets might overwrite things
                composed_image = ExportImage.from_blender_image(result.shader_node.image)

    # Check that we don't have some empty channels (based on weird images without any size for example)
    keys = list(composed_image.fills.keys()) # do not loop on dict, we may have to delete an element
    for k in [k for k in keys if isinstance(composed_image.fills[k], FillImage)]:
        if composed_image.fills[k].image.size[0] == 0 or composed_image.fills[k].image.size[1] == 0:
            gltf2_io_debug.print_console("WARNING",
                                         "Image '{}' has no size and cannot be exported.".format(
                                             composed_image.fills[k].image))
            del composed_image.fills[k]

    return composed_image


def __is_blender_image_a_jpeg(image: bpy.types.Image) -> bool:
    if image.source != 'FILE':
        return False
    if image.filepath_raw == '' and image.packed_file:
        return image.packed_file.data[:3] == b'\xff\xd8\xff'
    else:
        path = image.filepath_raw.lower()
        return path.endswith('.jpg') or path.endswith('.jpeg') or path.endswith('.jpe')

def __is_blender_image_a_webp(image: bpy.types.Image) -> bool:
    if image.source != 'FILE':
        return False
    if image.filepath_raw == '' and image.packed_file:
        return image.packed_file.data[8:12] == b'WEBP'
    else:
        path = image.filepath_raw.lower()
        return path.endswith('.webp')
