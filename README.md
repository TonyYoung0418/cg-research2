# Mirror Chess Renderer

This is a small C++17 Monte Carlo path tracer for COMP8610 Research Project 2.
It procedurally creates a 3D OBJ scene, loads that OBJ back into the renderer,
and renders a mirror-and-chess illusion inspired by Shruti Agarwal's Dartmouth
College 2018 Rendering Competition entry for the theme "Something seems a
little off". The chess pieces are imported from the downloaded TurboSquid "Free
Stuff 1 - Chess Set" OBJ assets.

The final scene intentionally makes the reflection inconsistent: the right-side
queen visible in the room is replaced by a king in the mirror, and the left-side
knight is replaced by a bishop. This is implemented with camera-only and
reflection-only mesh visibility rather than a post-process image trick.

The room background is procedurally modeled after an online abandoned-house
reference: rough plaster, old wood boards, a worn door, fireplace damage,
cracks, dust, and loose debris. It is built from geometry and materials rather
than using the reference photo as an image background; the window elements from
the reference are intentionally omitted.

The floor uses a CC0 old wood floor diffuse texture from Poly Haven, converted
to a local PPM file and sampled by the renderer as a repeating material map.

## External Assets And Citations

The core renderer, scene construction code, camera controls, material handling,
shadow rays, mirror visibility logic, texture sampling, and Monte Carlo path
tracing are implemented in this C++ project. External assets are used only as
scene inputs and are cited here for report traceability:

- Motivational image: Shruti Agarwal's Dartmouth College 2018 Rendering
  Competition entry for the theme "Something seems a little off".
- Chess meshes: TurboSquid "Free Stuff 1 - Chess Set" OBJ assets, imported as
  mesh geometry and assigned materials by the local scene builder.
- Floor texture: Poly Haven "Old Wood Floor" diffuse texture
  (`https://polyhaven.com/a/old_wood_floor`), CC0, converted to
  `textures/old_wood_floor.ppm` and sampled by the renderer as a repeating
  texture map.

No off-the-shelf renderer such as Blender, Unity, Unreal, or Cycles is used for
the final rendering. The renderer loads the generated OBJ scene and performs the
image synthesis itself.

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Render the required two viewpoints

```bash
./build/MirrorReflectionRenderer --preset main --width 1024 --height 768 --spp 160 --output renders/view_main.ppm
./build/MirrorReflectionRenderer --preset side --width 800 --height 600 --spp 64 --output renders/view_side.ppm
```

For fast previews:

```bash
./build/MirrorReflectionRenderer --preset main --width 320 --height 240 --spp 8 --output renders/preview.ppm
```

## Useful controls

- `--camera x y z` and `--look-at x y z`: camera position and orientation.
- `--object-offset x y z` and `--object-rotate-y degrees`: move/pose the imported chess pieces.
- `--floor-color r g b`, `--metal-roughness v`: material controls.
- `--shadows 0|1`: enable or disable shadow rays.
- `--light-pos x y z`, `--light-size v`, `--extra-light 0|1`: area-light controls.
- `--environment space|studio|sunset`: procedural environment selection.

The renderer writes `scenes/generated_scene.obj` and `scenes/generated_scene.mtl`
on every run, then renders from the loaded OBJ geometry.

## Prepared files

- `renders/view_main.png`: generated 1024x768 main result image.
- `renders/view_side.png`: generated 800x600 side-view result image.
- `Free_Stuff_1_-__Chess_Set/OBJ/`: the subset of the downloaded chess OBJ
  assets used by the scene builder.
