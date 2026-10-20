# lmcad to lmblender turntable QA

Command run: `./examples/compose/run.sh` (exports the lmcad mug, perforated plate, and U-channel bracket STLs, then renders composition and turntable images). The full command completed successfully on 2026-10-20 after the Standard/Filmic/AgX output transform was enabled.

## Numeric checks

`tests/turntable_quality.py build` checks each 128x96 RGBA image, subject-mask coverage, a four-pixel frame margin, and RGB MAD between each adjacent pair including the wraparound pair.

| Set | Views | Mask coverage | Adjacent RGB MAD min / mean |
|---|---:|---:|---:|
| Mug | 8 | 0.227–0.282 | 9.97 / 12.67 |
| Bracket | 8 | 0.050–0.225 | 10.37 / 13.34 |

The mug mask uses blue ceramic pixels (B−R > 60); the bracket mask uses bright low-chroma steel pixels in the center region. Each mask remains at least four pixels from all image boundaries. Distinct-view MAD exceeds 9.9/255 for both subjects.

The composition runner also passed the existing checks: mug upright error 7.3°, image/CAD aspect 0.870/1.200, luminance p5–p95 92.5–190.9, cavity interior/rim 103.4/130.3, and lit-floor/shadow 220.6/163.7; plate check found all eight openings with wall lit/shadow luminance 177.6/134.6; bracket front/side/rear steel masks were 28,841/22,125/20,607 pixels.

## Visual inspection

All eight mug views show an upright vessel with its rim/opening and handle visible; the silhouette stays inside the frame. The handle moves around the profile as the camera orbits. All eight bracket views show the U-channel from changing sides; no frame cuts off the object. The bracket is intentionally small relative to the 128x96 frame to retain floor and shadow context, while its numeric coverage remains within the tested 5.0–22.5% range.

These are eight separate still images rather than a video. STL remains a tessellated, material-free exchange format; lmblender assigns ceramic/steel materials after import. The mug recipe maps lmcad's +Y-up export to +Z-up and uses a gallery-only 1.2x radial profile adjustment. Its world fill was lowered to 0.65 so the cavity remains distinct under the baked display transform.
