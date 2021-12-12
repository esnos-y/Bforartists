# ##### BEGIN GPL LICENSE BLOCK #####
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU General Public License
#  as published by the Free Software Foundation; either version 2
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software Foundation,
#  Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
#
# ##### END GPL LICENSE BLOCK #####

# <pep8 compliant>

# these must be all lower case for comparisons

dict_custom = {
    # Added to newer versions of the dictionary,
    # we can remove these when the updated word-lists have been applied to aspell-en.
    "accessor", "accessors",
    "completer", "completers",
    "enqueue", "enqueued", "enqueues",
    "intrinsics",
    "iterable",
    "parallelization",
    "parallelized",
    "pipelining",
    "polygonization",
    "prepend", "prepends",
    "rasterize",
    "reachability",
    "runtime", "runtimes",
    "serializable",
    "unary",
    "variadic",

    # Correct spelling, update the dictionary, here:
    # https://github.com/en-wl/wordlist
    "accessor",
    "adjoint", "adjugate",
    "alignable",
    "allocatable",
    "allocator", "allocators",
    "anisotropic",
    "anisotropy",
    "atomicity",
    "boolean",
    "breaked",
    "callables",
    "canonicalization", "canonicalized", "canonicalizing",
    "catadioptric",
    "checksums",
    "collinear",
    "comparator,", "comparators,",
    "confusticate", "confusticated",
    "constructability",
    "coplanarity",
    "counterforce",
    "criterium",
    "crosstalk",
    "customizable",
    "decorrelated",
    "decrement",
    "decrementing",
    "deduplicating", "deduplication",
    "degeneracies",
    "denoised", "denoiser", "denoising",
    "dereference", "dereferenced",
    "desaturate",
    "designator",
    "destructor", "destructors",
    "dialogs",
    "dihedral",
    "discoverability",
    "discretization",
    "downcasting",
    "durations",
    "eachother",
    "editability",
    "effector", "effectors",
    "embedder",
    "enqueueing",
    "equiangular",
    "finalizer",
    "flushable",
    "formatter",
    "haptics",
    "highlightable",
    "homogenous",
    "illuminant",
    "incrementation",
    "initializer", "initializers",
    "instantiable", "instantiation", "instantiations",
    "interferences",
    "interocular",
    "invariants",
    "invisibilities",
    "irradiance",
    "iteratively",
    "jitteryness",
    "linkable",
    "luminances",
    "merchantability",
    "minimalistic",
    "misconfiguration", "misconfigured",
    "monospaced",
    "mutators",
    "natively",
    "optionals",
    "orthogonalize",
    "parallelize",
    "parallelizing",
    "precompute",
    "renormalized",
    "sortable",
    "tokenizing",
    "transmissive",
    "unmaximized",
    "unpaused"
    "overridable",
    "paddings",
    "parameterization",
    "parentless",
    "passepartout",
    "pixelate", "pixelated", "pixelisation",
    "planarity",
    "polytope",
    "postprocessed",
    "pre-filtered",
    "precalculate",
    "precisions",
    "precomputations",
    "precomputed", "precomputing",
    "prefetch", "prefetching",
    "prefilter", "prefiltered", "prefiltering",
    "premutliplied", "pre-multiplied",
    "prepend", "prepending",
    "preventively",
    "probabilistically",
    "procedurally",
    "profiler",
    "quadratically",
    "rasterizer",
    "rasterizes",
    "rasterizing",
    "rebalancing",
    "recurse", "recurses",
    "recursed",
    "recursivity",
    "redistributions",
    "registerable",
    "remappings",
    "rendeder",
    "renderable",
    "renormalize",
    "reparameterization",
    "reparametization",
    "repurpose",
    "retiming",
    "reusability",
    "schemas",
    "sidedness",
    "skippable",
    "stitchable",
    "subclass", "subclasses", "subclassing",
    "subdirectory", "subdirectories",
    "tertiarily",
    "triangulations",
    "triangulator",
    "trilinear",
    "tunable",
    "unassign",
    "unbuffered",
    "unclamped",
    "uncomment",
    "unconfigured",
    "undisplaced",
    "uneditable",
    "unflagged",
    "unformatted",
    "unkeyframed",
    "unlinkable",
    "unparameterized",
    "unparsed",
    "unproject",
    "unregister", "unregisters",
    "unselected",
    "unsynchronized",
    "untag", "untagging",
    "untrusted",
    "unvisited",
    "vectorial",
    "vectorization", "vectorized",
    "virtualized",
    "visibilities",
    "volumetrics",
    "vortices",
    "voxelize",
    "zoomable",

    # python types
    "enum", "enums",
    "int", "ints",
    "str",
    "tuple", "tuples",

    # python functions
    "func",
    "repr",

    # Accepted concatenations.
    "addon", "addons",
    "autocomplete",
    "colospace",
    "datablock", "datablocks",
    "keyframe", "keyframing",
    "lookup", "lookups",
    "multithreaded", "multithreading",
    "namespace",
    "reparent",
    "tooltip",
    "unparent",

    # Accepted abbreviations.
    "config",
    "coord", "coords",
    "dir",
    "iter",
    "multi",
    "ortho",
    "recalc",
    "resync",
    "struct", "structs",
    "subdir",

    # general computer terms
    "XXX",
    "app",
    "ascii",
    "autocomplete",
    "autorepeat",
    "blit", "blitting",
    "boids",
    "booleans",
    "codepage",
    "contructor",
    "decimator",
    "diff",
    "diffs",
    "endian",
    "endianness",
    "env",
    "euler", "eulers",
    "foo",
    "hashable",
    "http",
    "jitter", "jittering",
    "keymap",
    "lerp",
    "metadata",
    "mutex",
    "opengl",
    "preprocessor",
    "quantized",
    "searchable",
    "segfault",
    "stdin",
    "stdin",
    "stdout",
    "sudo",
    "threadsafe",
    "touchpad", "touchpads",
    "trackpad", "trackpads",
    "unicode",
    "usr",
    "vert", "verts",
    "voxel", "voxels",
    "wiki",

    # specific computer terms/brands
    "ack",
    "amiga",
    "cmake",
    "ffmpeg",
    "freebsd",
    "linux",
    "manpage",
    "mozilla",
    "nvidia",
    "openexr"
    "posix",
    "qtcreator",
    "unix",
    "valgrind",
    "xinerama",

    # general computer graphics terms
    "atomics",
    "barycentric",
    "bezier",
    "bicubic",
    "bitangent",
    "centroid",
    "colinear",
    "compositing",
    "coplanar",
    "crypto",
    "deinterlace",
    "emissive",
    "fresnel",
    "gaussian",
    "kerning",
    "lacunarity",
    "lossless",
    "lossy",
    "mipmap", "mipmaps", "mipmapped", "mipmapping",
    "musgrave",
    "n-gon", "n-gons",
    "normals",
    "nurbs",
    "octree",
    "quaternions",
    "radiosity",
    "reflectance",
    "shader",
    "shaders",
    "specular",

    # Blender specific terms.
    "animsys",
    "animviz",
    "bmain",
    "bmesh",
    "bpy",
    "depsgraph",
    "doctree",
    "editmode",
    "eekadoodle",
    "fcurve",
    "look-dev",
    "mathutils",
    "obdata",

    # Should have apostrophe but ignore for now unless we want to get really picky!
    "indices",
    "vertices",
}

# incorrect spelling but ignore anyway
dict_ignore = {
    "a-z",
    "animatable",
    "arg", "args",
    "bool",
    "constness",
    "dirpath",
    "dupli",
    "eg",
    "filename", "filenames",
    "filepath",
    "filepaths",
    "hardcoded",
    "id-block",
    "inlined",
    "loc",
    "namespace",
    "node-trees",
    "ok",
    "ok-ish",
    "param",
    "polyline", "polylines",
    "premultiply", "premultiplied",
    "pylint",
    "quad",
    "readonly",
    "submodule", "submodules",
    "tooltips",
    "tri",
    "ui",
    "unfuzzy",
    "utils",
    "uv",
    "vec",
    "wireframe",
    "x-axis",
    "y-axis",
    "z-axis",

    # acronyms
    "api",
    "cpu",
    "gl",
    "gpl",
    "gpu",
    "gzip",
    "hg",
    "ik",
    "lhs",
    "nan",
    "nla",
    "ppc",
    "rgb",
    "rhs",
    "rna",
    "smpte",
    "svn",
    "utf",

    # extensions
    "py",
    "rst",
    "xml",
    "xpm",

    # tags
    "fixme",
    "todo",

    # sphinx/rst
    "rtype",

    # slang
    "automagically",
    "hacky",
    "hrmf",

    # names
    "campbell",
    "jahka",
    "mikkelsen", "morten",

    # Company names.
    "Logitech",
    "Wacom",

    # Project Names.
    "Wayland",

    # clang-tidy (for convenience).
    "bugprone-suspicious-enum-usage",
    "bugprone-use-after-move",
}

# Allow: `un-word`, `re-word` ... etc, in this case only check `word`.
dict_ignore_hyphenated_prefix = {
    "de",
    "mis",
    "non",
    "post",
    "pre",
    "re",
    "un",
}

dict_ignore_hyphenated_suffix = {
    "ish",
    "ness",
}

files_ignore = {
    "source/tools/utils_doc/rna_manual_reference_updater.py",  # Contains language ID references.
}
