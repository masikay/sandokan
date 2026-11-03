"""Batch-convert FBX files to GLB for Raylib, using Blender in background mode.

Usage:
    blender -b -P tools/fbx_to_glb.py -- <input_dir> <output_dir> [--texture atlas.png]

Every .fbx under <input_dir> (recursively) becomes a .glb in <output_dir>, keeping the
relative folder layout. With --texture, that image is applied as the base color of every
material, which is what Synty packs need: their FBX files carry no texture links, because
Unity assigns the shared palette atlas separately.
"""

import argparse
import sys
from pathlib import Path

import bpy


def parse_args():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    parser = argparse.ArgumentParser()
    parser.add_argument("input_dir", type=Path)
    parser.add_argument("output_dir", type=Path)
    parser.add_argument("--texture", type=Path, default=None)
    return parser.parse_args(argv)


def apply_texture(image_path):
    image = bpy.data.images.load(str(image_path), check_existing=True)
    for material in bpy.data.materials:
        material.use_nodes = True
        nodes = material.node_tree.nodes
        bsdf = nodes.get("Principled BSDF")
        if bsdf is None:
            continue
        tex = nodes.new("ShaderNodeTexImage")
        tex.image = image
        tex.interpolation = "Closest"  # keep the flat palette colors crisp
        material.node_tree.links.new(tex.outputs["Color"], bsdf.inputs["Base Color"])


def drop_lower_lods():
    """Synty FBX files carry LOD0..LOD3 as separate objects; drawn together they overlap."""
    for obj in list(bpy.data.objects):
        name = obj.name
        if "_LOD" in name and not name.rsplit("_LOD", 1)[1].startswith("0"):
            bpy.data.objects.remove(obj, do_unlink=True)


def bake_transforms():
    """Bake rotation/scale/hierarchy into the mesh data.

    Synty FBX files keep the mesh authored in a different axis convention and fix it with a
    +90 degree X rotation and a 0.01 scale on the node. Raylib does not apply that node
    transform, so models showed up lying on their side. Baking it into the vertices makes the
    exported GLB identical regardless of how the loader treats node transforms.
    """
    bpy.ops.object.select_all(action="SELECT")
    meshes = [o for o in bpy.context.selected_objects if o.type == "MESH"]
    bpy.context.view_layer.objects.active = meshes[0] if meshes else None
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.select_all(action="DESELECT")
    for obj in meshes:
        obj.select_set(True)
    if meshes:
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    # empties that only existed to hold the old transform are no longer needed
    for obj in list(bpy.data.objects):
        if obj.type == "EMPTY":
            bpy.data.objects.remove(obj, do_unlink=True)


def strip_vertex_colors():
    """Synty uses vertex colors as wind/tint masks for its Unity shaders (black trunk bases,
    RGB-coded leaf tips). Raylib multiplies vertex colors into the texture, so left in they
    turn plants dark and purple."""
    for obj in bpy.data.objects:
        if obj.type != "MESH":
            continue
        attributes = obj.data.color_attributes
        while len(attributes) > 0:
            attributes.remove(attributes[0])


def convert(fbx_path, glb_path, texture):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(fbx_path))
    drop_lower_lods()
    bake_transforms()
    strip_vertex_colors()
    if texture is not None:
        apply_texture(texture)
    glb_path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.gltf(filepath=str(glb_path), export_format="GLB")


def main():
    args = parse_args()
    fbx_files = sorted(args.input_dir.rglob("*.fbx"))
    if not fbx_files:
        print(f"No .fbx files found under {args.input_dir}")
        return

    failed = []
    for fbx in fbx_files:
        glb = args.output_dir / fbx.relative_to(args.input_dir).with_suffix(".glb")
        try:
            convert(fbx, glb, args.texture)
            print(f"OK    {fbx} -> {glb}")
        except Exception as error:
            failed.append(fbx)
            print(f"FAIL  {fbx}: {error}")

    print(f"Converted {len(fbx_files) - len(failed)}/{len(fbx_files)} files")
    if failed:
        sys.exit(1)


main()
