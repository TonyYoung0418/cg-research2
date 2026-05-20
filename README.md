# Mirror Chess Room Path Tracer

This repository contains my COMP8610 Research Project 2 renderer. It is a
C++17 Monte Carlo path tracer that generates a staged indoor scene, loads the
generated OBJ geometry, and renders a mirror illusion in which the room and the
mirror reflection are intentionally inconsistent.

The final scene is a living-room/study composition with a large floor mirror,
imported furniture, chess pieces on the table, textured wood flooring, area
lights, and a procedural environment. The main visual idea is inspired by
Shruti Agarwal's Dartmouth College 2018 Rendering Competition entry for the
theme "Something seems a little off".

No off-the-shelf renderer such as Blender, Cycles, Unity, Unreal, or Mitsuba is
used for the final image synthesis. The C++ program performs the scene loading,
BVH construction, ray intersections, shading, shadow testing, mirror reflection,
and Monte Carlo sampling itself.

## Final Renders

The submitted render set was generated with `spp=768`.

- `renders/view_main.png`: main view, 1024x768.
- `renders/view_side.png`: side view, 800x600.
- `renders/view_door.png`: doorway view, 800x600.
- `renders/view_extra.png`: additional room view, 800x600.

The matching PPM files are also kept in `renders/` because the renderer writes
PPM directly before conversion to PNG.

## Project Structure

```text
.
├── CMakeLists.txt
├── README.md
├── src/
│   ├── main.cpp
│   ├── scene_builder.cpp
│   └── scene_builder.hpp
├── scenes/
│   ├── generated_scene.obj
│   └── generated_scene.mtl
├── textures/
│   └── old_wood_floor.ppm
├── renders/
│   ├── view_main.png / view_main.ppm
│   ├── view_side.png / view_side.ppm
│   ├── view_door.png / view_door.ppm
│   └── view_extra.png / view_extra.ppm
├── Free_Stuff_1_-__Chess_Set/OBJ/
└── 57-estancia_comedor_obj/        # local source asset, not committed
```

## Main Features

- Monte Carlo path tracing with multiple samples per pixel.
- Triangle mesh loading from OBJ.
- BVH acceleration structure for triangle intersection.
- Diffuse, metal, glass, emissive, mirror, and textured floor materials.
- Direct lighting from sampled area lights.
- Shadow rays that can be enabled or disabled from the command line.
- User-controlled camera position, look-at target, field of view, image size,
  sample count, light position, light size, shadow toggle, object transform,
  material roughness, and environment mode.
- Procedural scene generation to `scenes/generated_scene.obj`.
- Imported room furniture from a large external OBJ, filtered and repositioned
  by `src/scene_builder.cpp`.
- Imported chess mesh assets from the TurboSquid chess set.

## Mirror Illusion Implementation

The mirror is not a post-processing trick. It is a real mirror surface in the
scene using the `mirror` material.

In `src/main.cpp`, when a ray hits the mirror material, the ray direction is
reflected and the ray visibility mode changes from `kPrimaryRay` to
`kMirrorRay`. Meshes whose object names begin with `CameraOnly` are visible only
to primary camera rays. Meshes whose object names begin with `ReflectionOnly`
are visible only to mirror-reflected rays.

This makes it possible for the direct view and reflected view to disagree:

- The room sofa is camera-only in the direct view.
- A shifted lounge-chair copy is reflection-only in the mirror.
- The visible chess queen becomes a king in the mirror.
- The visible knight becomes a bishop in the mirror.

The final image is therefore rendered by normal ray tracing, but with deliberate
visibility rules for selected geometry.

## External Assets

The renderer and scene construction code are written for this project. External
assets are used only as input geometry or texture sources.

- Chess meshes: TurboSquid "Free Stuff 1 - Chess Set" OBJ assets. Only the
  chess OBJ files used by the scene builder are kept in the repository.
- Room furniture source mesh: `57-estancia_comedor_obj/room.obj`. This is a
  large local asset used by the scene builder to extract selected indoor
  furniture. It is not committed because the original OBJ is larger than
  GitHub's normal file size limit.
- Generated room mesh: `scenes/generated_scene.obj`. This file is committed so
  the final scene state can be inspected even without opening the original
  large room asset.
- Floor texture: Poly Haven "Old Wood Floor", CC0, converted to
  `textures/old_wood_floor.ppm` and sampled as a repeating diffuse texture.

The furniture importer intentionally removes the original walls, windows,
window frames, outdoor backdrop, wall-mounted shelf, wall picture, dining set,
extra chairs, and small cabinet from the borrowed room OBJ. The final layout
keeps only the selected furniture needed for the mirror-room composition.

## Build

Use CMake from the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j4
```

The executable will be:

```bash
./build/MirrorReflectionRenderer
```

## Reproduce the Final Renders

The final render commands use `spp=768`.

```bash
mkdir -p renders

./build/MirrorReflectionRenderer --preset main --width 1024 --height 768 --spp 768 --output renders/view_main.ppm
sips -s format png renders/view_main.ppm --out renders/view_main.png

./build/MirrorReflectionRenderer --preset side --width 800 --height 600 --spp 768 --output renders/view_side.ppm
sips -s format png renders/view_side.ppm --out renders/view_side.png

./build/MirrorReflectionRenderer --camera 4.95 1.15 -2.55 --look-at 0.05 0.95 2.10 --fov 58 --width 800 --height 600 --spp 768 --output renders/view_door.ppm
sips -s format png renders/view_door.ppm --out renders/view_door.png

./build/MirrorReflectionRenderer --camera 3.85 1.45 5.75 --look-at -0.18 0.62 2.35 --fov 56 --width 800 --height 600 --spp 768 --output renders/view_extra.ppm
sips -s format png renders/view_extra.ppm --out renders/view_extra.png
```

On non-macOS systems, replace `sips` with another PPM-to-PNG converter, such as
ImageMagick:

```bash
magick renders/view_main.ppm renders/view_main.png
```

## Fast Preview Example

For quick testing, lower the resolution and sample count:

```bash
./build/MirrorReflectionRenderer --preset main --width 320 --height 240 --spp 8 --output renders/preview.ppm
```

## Command-Line Controls

The renderer supports the following user controls and parameterized inputs:

- `--camera x y z`: camera position.
- `--look-at x y z`: camera orientation target.
- `--fov degrees`: camera field of view.
- `--width n --height n`: output resolution.
- `--spp n`: samples per pixel.
- `--max-depth n`: maximum ray bounce depth.
- `--threads n`: render thread count.
- `--object-offset x y z`: transform imported chess objects.
- `--object-rotate-y degrees`: rotate imported chess objects.
- `--floor-color r g b`: floor material color fallback.
- `--metal-roughness v`: metal material roughness.
- `--shadows 0|1`: disable or enable shadow rays.
- `--light-pos x y z`: main area-light position.
- `--light-size v`: main area-light size.
- `--extra-light 0|1`: disable or enable the additional ceiling fill light.
- `--environment space|studio|sunset`: select the procedural background model.

These controls cover the required camera, object pose, material, shadow, light,
and environment parameters for the assignment.

## Scene Generation Notes

Each renderer run first calls `scene_builder::writeSceneObj(...)`, which writes:

- `scenes/generated_scene.obj`
- `scenes/generated_scene.mtl`

The renderer then reloads this generated OBJ and renders from that mesh data.
Because of this, rerendering from source requires the local
`57-estancia_comedor_obj/` folder to exist beside the project files. If that
folder is missing, the committed `scenes/generated_scene.obj` still documents
the final generated scene, but the automatic regeneration step will fail until
the source room asset is restored.

## Implementation Files

- `src/main.cpp`: command-line parsing, material setup, OBJ loading, BVH,
  path tracing, mirror visibility, lighting, texture sampling, and image output.
- `src/scene_builder.cpp`: procedural room shell, mirror frame, lights, imported
  room furniture filtering, chess-piece placement, and generated OBJ/MTL output.
- `src/scene_builder.hpp`: build options used by the renderer and scene builder.

## Notes For Marking

The current final scene includes:

- A complete indoor room shell with floor, ceiling, walls, baseboards, door,
  fireplace area, area lights, and a large floor mirror.
- Imported furniture arranged around the mirror.
- Removed extra dining chairs and unwanted background furniture.
- A mirror-only object substitution effect.
- Four high-sample final renders at `spp=768`.

The generated scene, final renders, renderer source code, texture, and required
chess assets are included in the repository. The large original room OBJ source
asset remains local and is documented above.
