# Tutorial checklist and score

The checklist below defines a good tutorial. Each item is scored for this tutorial (pages 1 to 9 and the comparison pages under blender/) as pass (1), partial (0.5), or fail (0), with the evidence.

## Checklist of a good tutorial

1. One goal per page and stated up front.
2. Every code block runs as written, copy-paste, with the expected output shown.
3. Each step adds one idea and builds on the last.
4. A rendered image for every result.
5. States prerequisites and install in one command.
6. Explains failures and how to fix them.
7. Ends with a next step and links to reference.
8. Plain text with no decorative markup.
9. Tested on a clean machine by someone who did not write it.
10. Comparison with the reference tool is honest about gaps.

## Score: 8.5 of 10

| Item | Score | Evidence |
|---|---|---|
| 1. One goal per page | pass | Every page, including each of the 44 comparison pages, opens with a "Goal:" line naming one outcome. |
| 2. Code runs as written, output shown | partial | Pages 1 to 8: each script was run from an empty directory three times (by the author, then twice by a separate tester with an empty tool cache), downloaded with the page's `curl -O` line; the console output matched the page's expected block character for character and every image was byte-identical to the page's. `scripts/tutorial-sync.py --check` (run by `test.sh`) fails if a page's code differs from the file that was run. Partial because the comparison pages' "Run both" commands were reproduced for only 3 of 44 examples (material-principled, camera-look-at, procedural-materials), need a Blender 5.x executable on PATH, and the 21 examples that denoise crash with the v0.3.4 release on CPUs without SVE; the lmblender examples also print nothing on success, so those pages show an image but no console output. |
| 3. One idea per step, building on the last | partial | Pages 1 to 6 each reuse the previous scene and add one topic. Partial because some pages add two closely related ideas (page 2 adds `translate` and the camera; page 3 adds materials and lights together), and page 7 starts a new emissive scene rather than extending page 6. |
| 4. Image for every result | pass | Pages 1 to 8 show the image their script writes, including the fixed script on page 8; page 6 shows all three frames. Comparison pages show side-by-side images for all 39 examples that write a PNG and say so for the 5 that do not. |
| 5. Prerequisites and one-command install | pass | The index lists Node.js 24.15+, the single `npm install -g @lmctl-ai/lmctl@latest` command, a version check, and the supported platforms; lmblender itself is downloaded by the `_dsl_ install=` line, verified from an empty cache. |
| 6. Failures and fixes | pass | Page 8 runs a failing script, shows the exact JSON error, explains each field, and shows the fix and its output and image. Its table covers the common error codes and the arm64 denoiser crash. RUN_HOST, OUTPUT_PATH, and the crash were reproduced; the other rows are taken from LLM_GUIDE.md and were not reproduced. |
| 7. Next step and reference links | pass | Every page ends with a "Next:" link; the index and page 9 link to LLM_GUIDE.md, ANIMATION.md, PRESETS.md, the examples, the LMScript docs, and releases. The tester resolved every relative link against the GitHub tree. |
| 8. Plain text, no decorative markup | pass | Headings, short lists, tables, and code blocks only; no emoji, badges, bold, or HTML other than invisible sync markers. |
| 9. Clean machine, tested by someone else | partial | A separate agent that did not write the pages followed them twice with a fresh HOME and empty lmscript tool cache, reading the pages from GitHub; its first round found nine problems, all now fixed. Partial because it ran on the same physical Linux arm64 host as the author, with Node.js and lmscript already installed, it is not a human reader, and macOS, Windows, and x64 were not tested. |
| 10. Honest comparison | pass | Page 9 lists five lmblender defects with mechanisms and measured numbers (denoiser SIGILL, dark-tone LUT, sRGB textures, squeezed fallback camera, silent example errors), the Blender-side substitutions, what was not compared, that the hero image is one of the closest pairs, and that 21 timings come from a local build rather than the release. The independent tester's review of page 9 led to removing duplicated image pairs and separating release from local-build timings. |

## What would raise the score

- Run pages 1 to 8 on macOS, Windows, and Linux x64, ideally by a person on a machine without Node.js installed.
- Make every comparison "Run both" command print a success line, and reproduce all 44 on a CPU where the release denoiser works.
- Split page 3 into materials and lights, and make page 7 extend the page 6 scene.
