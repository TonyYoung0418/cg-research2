# Slide 1: Outer Limits Observation Deck

Authors: [Team Members]

Motivational reference: Dartmouth Rendering Competition 2019, Dario Seyb's
black-hole bridge scene.

# Slide 2: Goal

Create a photo-realistic science-fiction observation deck with a black hole,
glowing accretion disk, glass, metal, soft shadows, and two verified viewpoints.

# Slide 3: Pipeline

1. C++ scene generator writes OBJ/MTL geometry.
2. Renderer loads the OBJ meshes.
3. Monte Carlo path tracing estimates direct and indirect illumination.
4. PPM images are saved for report figures.

# Slide 4: Scene Construction

The scene includes a room, window frame, glass pane, console, metallic probe,
chairs, ceiling area lights, and an emissive accretion disk outside the deck.

# Slide 5: Rendering Algorithm

The renderer uses ray-triangle intersections, BVH acceleration, Russian roulette
path continuation, diffuse/metal/glass materials, and sampled emissive triangles.

# Slide 6: User Controls

Camera, object offset/pose, material color/roughness, shadow toggle, light
position/size/count, and procedural environment selection are controlled from the
command line.

# Slide 7: Result View 1

![Main rendered viewpoint](../renders/view_main.png)

# Slide 8: Result View 2

![Side rendered viewpoint](../renders/view_side.png)

# Slide 9: Discussion

Strengths: fully self-contained renderer, soft shadows, multiple materials,
procedural geometry, and repeatable experiments.

Limitations: no spectral lensing simulation, no denoiser, simple procedural
background instead of a measured HDR environment map.

# Slide 10: Conclusion

The project demonstrates a complete rendering pipeline from authored 3D scene to
photo-realistic path-traced outputs and report-ready multi-view results.
