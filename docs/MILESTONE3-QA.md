# Milestone 3 render QA

Run on 2026-10-20 after `./test.sh`; static host reports `ldd: not a dynamic executable`.

| DSL example | Auditable image check | Numeric result | Image QA |
|---|---|---|---|
| `examples/render-mesh.lms` | PNG pixel variation and direct-vs-XML object pixels | 256x192; direct/XML object MAD 1.11/255; full-frame MAD 14.94/255 due the XML fixture's unlinked black world | inspected |
| `examples/material-principled.lms` | Blue foreground pixels and green top-left world sample | mean object RGB `(40.4, 100.5, 247.5)`; world sample `(10, 54, 15)` | PASS: centered blue tetrahedron against green world |
| `examples/camera-look-at.lms` | Foreground centroid within 12% of image center | `(120.7, 114.0)` in 256x192 | PASS: full tetrahedron near center, uncropped |
| `examples/mesh-instances.lms` | Connected-component segmentation; require two components >=10 pixels | component sizes `[915, 781]`; 256x192 | PASS: two separate uncropped tetrahedra with a clear gap |
| `examples/render-exr.lms` | OpenEXR magic, dataWindow and payload | 128x96, 5,453 bytes | header validated |
| `examples/render-timeout.lms` | One-second cancellation, structured error, no published partial output | `RENDER_TIMEOUT`; no PNG published | cancellation check passed |
| `examples/render-settings.lms` | Visible pixels at a non-default seed/bounce setting | 192x144; luminance stddev 35.82/255 | PASS, score 5/5: one centered uncropped tetrahedron with clear contrast |
| `examples/compose/mug-render.lms` | Pixel variation plus checklist for ceramic body, attached handle, framing and contrast | 384x288; luminance stddev 32.23/255 | PASS, score 5/5 |
| `examples/compose/plate-render.lms` | Pixel variation plus checklist for perforated plate, open holes and framing | 384x288; luminance stddev 33.42/255 | PASS, score 5/5; eight holes visible |

The requested QA reviewer inspected the three material, camera, and instance PNGs directly and returned PASS for each checklist. Other numerical image checks are run by `test.sh`.

The lmcad composition runner `examples/compose/run.sh` successfully ran both lmcad STL exporters and lmblender render scripts sequentially. Source models were the existing lmcad mug and plate-with-holes examples.
