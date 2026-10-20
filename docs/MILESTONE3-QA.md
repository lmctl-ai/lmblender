# Milestone 3 image review

Re-rendered 2026-10-20 after correcting direct Cycles light shader assignments and lmcad/renderer axis handling. The earlier generic compose “PASS 5/5” is withdrawn. Both compose images now have image-specific numeric checks in `tests/compose_mug_quality.py` and `tests/compose_plate_quality.py`, run by `examples/compose/run.sh`.

| Image / example | Falsifiable criteria | Numeric result | Independent visual review |
|---|---|---|---|
| `build/compose-mug.png` / `examples/compose/mug-render.lms` | blue body mask, uncropped; principal axis <=10° from vertical; body sigma >=7/255; dark opening/rim contrast; lit floor >1.25x shadow sample | upright error 3.3°; body sigma 20.7/255; cavity 25.3 vs rim 35.5; floor lit/shadow 83.5/26.7 | Opus PASS, 4/5. Opening, attached handle, soft key pool, and cast shadow visible. Minor vertical facet banding, floor grain, and dark surround remain. |
| `build/compose-plate.png` / `examples/compose/plate-render.lms` | copper mask >5,000 pixels; uncropped; eight neutral backdrop components visible through plate; lit wall >1.2x wall shadow | copper pixels 11,125; 8 openings; wall lit/shadow 113.4/62.1 | Opus PASS, 4/5. All 8 holes show the lit neutral wall. Material still reads as dark rust-orange more than polished copper; pink light noise above the plate remains minor. |

The mug is explicitly rotated from lmcad's Y-up `revolve_y` coordinates to renderer +Z-up. Generic STL carries no axis metadata, so arbitrary imports are not rotated automatically. `scene.fit_camera(fov)` uses transformed mesh bounds and ignores studio planes; the composition uses a diffuse XY floor, area key/fill, and a point-fill default where no point light is supplied. The plate uses a lit vertical wall behind its holes so openings can be checked visually and numerically.

| `build/procedural-materials.png` / `examples/procedural-materials.lms` | checker color mask and noise color bins | final: checker red pixels 2,355; noise blue pixels 6,621; 260 coarse noise-color bins | Opus reviewed the prior render: PASS, 4/5. We lowered noise material brightness to retain highlight detail and visually re-inspected the final render; tetra fixture is still narrow and floor grain remains. |
| `build/perspective-reference.png` + `build/orthographic-camera.png` / `examples/orthographic-camera.lms` | uncropped orange object mask; compare rendered object height | initial: perspective 115 px; orthographic 224 px; ratio 1.95 | Opus reviewed the initial views: PASS, 4/5. Both cameras were moved to a three-quarter angle; final images were visually re-inspected after rerender. |

## Technical smoke images (not product-gallery claims)

The compact binding examples use a tetrahedron fixture to exercise execution and pixels, not to claim a realistic material or composition. I inspected each generated image directly:

| Image | What it demonstrates | Limitation |
|---|---|---|
| `build/imported-mesh.png` | visible gray tetrahedron silhouette | projected nearly edge-on; not a modeling showcase |
| `build/imported-mesh-xml.png` | same fixture via XML oracle | dark-background regression oracle only |
| `build/material-principled.png` | blue material against green world | simple tetrahedron, slight visible shading, not a material study |
| `build/camera-look-at.png` | visible object near the image center | same simple tetrahedron |
| `build/mesh-instances.png` | two distinct, separated silhouettes | not a composition example |
| `build/render-settings.png` | visible pixels under selected seed/bounce settings | technical smoke only |
| `build/cornell-view.png` | colored room planes and central box | XML test scene, not a full lighting gallery |
| `build/cornell-cleanenv.png`, `build/cornell-static.png`, `build/cornell-dynamic.png` | output/environment and linkage smoke files | not beauty renders; static/dynamic controls are minimal |

`./test.sh` checks decoded output, direct-vs-XML object-pixel MAD, embedded schema, and static linkage. The old generic visual score is not treated as evidence for scene-specific visual quality.
