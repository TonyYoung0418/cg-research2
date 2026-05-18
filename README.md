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

## Build

```bash
cmake -S . -B build
cmake --build build
```

## Render the required two viewpoints

```bash
./build/MirrorReflectionRenderer --preset main --width 800 --height 600 --spp 64 --output renders/view_main.ppm
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

- `docs/report.md`: conference-style report draft. Replace the author names and
  contribution details before final submission, then export to PDF.
- `docs/presentation.md`: 10-slide seminar outline with result image slots filled.
- `docs/peer_review_template.md`: confidential peer review template.
- `renders/view_main.png` and `renders/view_side.png`: generated 800x600 result
  images from two viewpoints.
- `Free_Stuff_1_-__Chess_Set/OBJ/`: the subset of the downloaded chess OBJ
  assets used by the scene builder.
