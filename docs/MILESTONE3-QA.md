# Milestone 3 image review

Re-rendered 2026-10-20 after correcting direct Cycles light shader assignments and lmcad/renderer axis handling. The earlier generic compose “PASS 5/5” is withdrawn. Both compose images now have image-specific numeric checks in `tests/compose_mug_quality.py` and `tests/compose_plate_quality.py`, run by `examples/compose/run.sh`.

| Image / example | Falsifiable criteria | Numeric result | Independent visual review |
|---|---|---|---|
| `build/compose-mug.png` / `examples/compose/mug-render.lms` | blue body mask; STL and rendered silhouette aspect; principal axis <=10°; mean luminance >=50 and p95-p5 >=35; cavity/rim contrast; floor lit/shadow >1.25 | rendered/CAD aspect 0.853/1.200; upright error 6.8°; mean 88.9/255, p5-p95 35.7-143.5; sigma 30.3; cavity 43.6/68.3; floor lit/shadow 192.2/103.7 | Opus PASS 6/6. Its full silhouette ratio was 1.16 height/width, just above the 1.15 minimum. Opening, broad lighting and shadow pass. Residual facets/shoulder lump noted. |
| `build/compose-plate.png` / `examples/compose/plate-render.lms` | copper mask >5,000 pixels; uncropped; eight neutral backdrop components visible through plate; lit wall >1.2x wall shadow | copper pixels 11,125; 8 openings; wall lit/shadow 113.4/62.1 | Opus PASS, 4/5. All 8 holes show the lit neutral wall. Material still reads as dark rust-orange more than polished copper; pink light noise above the plate remains minor. |

The mug is explicitly rotated from lmcad's Y-up `revolve_y` coordinates to renderer +Z-up. Generic STL carries no axis metadata, so arbitrary imports are not rotated automatically. `scene.fit_camera(fov)` defers fitting until render dimensions are known, uses transformed bounds and the output aspect. The composition applies a documented 1.2x horizontal profile adjustment to the specific lmcad mug because its source height/diameter ratio looked can-like in the render; the source mesh is not globally rescaled. It uses a diffuse XY floor, broad area key/fill, low-strength sun and world ambient. The plate uses a lit vertical wall behind its holes.

| `build/procedural-materials.png` / `examples/procedural-materials.lms` | checker color mask and noise color bins | final: checker red pixels 2,355; noise blue pixels 6,621; 260 coarse noise-color bins | Opus reviewed the prior render: PASS, 4/5. We lowered noise material brightness to retain highlight detail and visually re-inspected the final render; tetra fixture is still narrow and floor grain remains. |
| `build/perspective-reference.png` + `build/orthographic-camera.png` / `examples/orthographic-camera.lms` | uncropped orange object mask; compare rendered object height | initial: perspective 115 px; orthographic 224 px; ratio 1.95 | Opus reviewed the initial views: PASS, 4/5. Both cameras were moved to a three-quarter angle; final images were visually re-inspected after rerender. |
| `build/image-texture.png` / `examples/image-texture.lms` | framed cube; alternating red/yellow cells on top and front; each color >200 px on each face; floor and shadow visible | red 4,160 px; yellow 3,837 px; top [1,461 red, 1,323 yellow]; front [2,787 red, 2,649 yellow] | Opus PASS 4/4. Per-corner cube UVs fixed the prior front-face smear. Opus notes front cells are vertically stretched and the right face is dark; neither hides checker alternation on the required faces. |

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
