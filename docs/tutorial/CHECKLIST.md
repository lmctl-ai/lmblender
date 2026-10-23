# Tutorial checklist and score

The checklist below defines a good tutorial. Each item is scored for this tutorial (pages 1 to 11 and the comparison pages under blender/) as pass (1), partial (0.5), or fail (0), with the evidence.

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

## Score: 9.5 of 10

| Item | Score | Evidence |
|---|---|---|
| 1. One goal per page | pass | Every page, including the 44 comparison pages under blender/, has a "Goal:" line on line 3. |
| 2. Code runs as written, output shown | pass | `scripts/tutorial-run.py` copies each page's script into an empty directory, runs `lmscript FILE.lms`, and requires the console output to equal the page's "Expected output" block exactly and every image to match the page's image (mean difference under 3/255). On Linux arm64 with the v0.3.4 release, 10 of 10 pages pass with identical images. `scripts/tutorial-sync.py --check` in `test.sh` keeps page code identical to the files that are run. All 44 comparison pages now show the expected lmblender and Blender output, and their commands were run for all 44 examples from the repository layout. |
| 3. One idea per step, building on the last | pass | Pages 2 to 9 each start from the previous page's script and add one thing: objects, camera, materials, lights, textures, an imported mesh, keyframes, a view transform. Page 10 is page 9 with one deliberate error. Each page names what it adds before the code. |
| 4. Image for every result | pass | Pages 1 to 10 show every image their script writes (all three animation frames on page 8, both transforms on page 9, the fixed render on page 10). Comparison pages show side-by-side images for the 39 examples that write a PNG and say so for the 5 that do not. |
| 5. Prerequisites and one-command install | pass | The index lists Node.js 24.15+, the single `npm install -g @lmctl-ai/lmctl@latest` command, a version check, and the supported platforms; lmblender itself is downloaded by the `_dsl_ install=` line. |
| 6. Failures and fixes | pass | Page 10 runs a failing script, shows the exact JSON error, explains each field, and shows the fix with its output and image. Its table covers error codes, the GitHub API rate limit, lmscript's 60 second `system()` limit, and the arm64 denoiser crash; the rate limit, the time limit, RUN_HOST, OUTPUT_PATH, and the crash were reproduced. |
| 7. Next step and reference links | pass | Every page ends with a "Next:" link; the index and page 11 link to LLM_GUIDE.md, ANIMATION.md, PRESETS.md, the examples, the LMScript docs, and releases. |
| 8. Plain text, no decorative markup | pass | Headings, short lists, tables, and code blocks only; no emoji, badges, bold, or HTML other than invisible sync markers. |
| 9. Clean machine, tested by someone else | partial | A separate agent that did not write the pages followed the first version twice with a fresh HOME and empty tool cache and found nine problems, all fixed. The `tutorial` GitHub Actions workflow now runs every page on fresh Linux x64, Linux arm64, macOS arm64, macOS x64, and Windows x64 runners with lmscript installed from npm; its results are recorded below. Partial because no person has followed the current pages. |
| 10. Honest comparison | pass | Page 11 states how the ports were made, the median and worst differences, per-example tables, the five lmblender problems the comparison found with before and after numbers, what still differs and why, what was not compared, and that a release after v0.3.4 is needed for readers to get the fixes. |

## Clean-runner results

Pending the first run of the `tutorial` workflow on these pages.
