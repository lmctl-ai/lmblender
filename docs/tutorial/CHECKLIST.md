# Tutorial checklist and score

The checklist below defines a good tutorial. Each item is scored for this tutorial (pages 1 to 11 and the comparison pages under blender/) as pass (1), partial (0.5), or fail (0), with the evidence. The scores are the author's; the independent test runs they cite are summarized at the end.

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
| 2. Code runs as written, output shown | pass | `scripts/tutorial-run.py` copies each page's script into an empty directory, runs `lmscript FILE.lms`, and requires the console output to equal the page's "Expected output" block exactly and every image to match the page's image within 3/255. It passes for all 10 pages on five fresh runners (below). `scripts/tutorial-sync.py --check` in `test.sh` keeps page code identical to the files that are run. The two "Run both" commands on all 44 comparison pages were run from a fresh clone of main with an lmblender built from this repository and Blender 5.2: on all 44 pages both commands ran and lmblender's output equaled the page's expected block. |
| 3. One idea per step, building on the last | pass | Pages 2 to 9 each start from the previous page's script and add one thing: objects, camera, materials (with more samples), lights, textures, an imported mesh, keyframes, a view transform. Page 10 is page 9 with one deliberate error. Each page says what it adds before the code; an independent reader diffed consecutive scripts and found the descriptions accurate after the fixes listed below. |
| 4. Image for every result | pass | Pages 1 to 10 show every image their script writes (all three animation frames on page 8, both transforms on page 9, the fixed render on page 10). Comparison pages show side-by-side images for the 39 examples that write a PNG and say so for the 5 that do not. |
| 5. Prerequisites and one-command install | pass | The index lists Node.js 24.15+, the single `npm install -g @lmctl-ai/lmctl@latest` command, a version check, and the supported platforms; lmblender itself is downloaded by the `_dsl_ install=` line. |
| 6. Failures and fixes | pass | Page 10 runs a failing script, shows the exact JSON error, explains each field, and shows the fix with its output and image. Its table covers error codes, the GitHub API rate limit, lmscript's 60 second `system()` limit, and the arm64 denoiser crash; the rate limit, the time limit, RUN_HOST, OUTPUT_PATH, and the crash were reproduced. |
| 7. Next step and reference links | pass | Every page ends with a "Next:" link; the index and page 11 link to LLM_GUIDE.md, ANIMATION.md, PRESETS.md, the examples, the LMScript docs, and releases. The independent reader resolved every relative link on the index, pages 1 to 11, and this file against the GitHub tree. |
| 8. Plain text, no decorative markup | pass | Headings, short lists, tables, and code blocks only; no emoji, badges, bold, or HTML other than invisible sync markers. |
| 9. Clean machine, tested by someone else | partial | Fresh hosted runners on five platforms ran every page with lmscript newly installed from npm (below). A separate agent that did not write the pages followed them three times from GitHub with a fresh HOME and empty tool cache, reporting problems each round that were then fixed. Partial because no human reader has followed the pages, and the independent reader's own runs were on a Linux arm64 host with Node.js already installed. |
| 10. Honest comparison | pass | Page 11 states how the ports were made, the median and worst differences, per-example tables (first image each, with all images on the example pages), the five lmblender problems the comparison found with before and after numbers, what still differs and why, what was not compared, that timings are rough single runs, and that a release after v0.3.4 is needed for readers to get the fixes; until then its per-example commands reproduce the old numbers with the release binary. |

## Clean-runner results

The `tutorial` workflow (run 39895483969 on commit 2e81e45, lmscript 0.2.388 from npm, lmblender v0.3.4 downloaded by the scripts) passed every page on every runner: console output identical to the page, and images within the tolerance.

| Runner | Pages passed | Largest image difference /255 | Slowest page |
|---|---|---|---|
| Linux x64 (ubuntu-24.04) | 10 of 10 | 0.00 | 7.0 s |
| Linux arm64 (ubuntu-24.04-arm) | 10 of 10 | 0.00 | 4.9 s |
| macOS arm64 (macos-15) | 10 of 10 | 0.39 | 5.2 s |
| macOS x64 (macos-15-intel) | 10 of 10 | 0.04 | 16.9 s |
| Windows x64 (windows-2022) | 10 of 10 | 0.08 | 7.6 s |

Later edits to pages 9 and 10 keep page 8's keyframes in the script; still renders ignore keys, and the local check re-confirmed identical images. The workflow runs again on every push that changes the tutorial.

## Independent reader findings

- Round 1 (first version): nine problems, including dead links, a stray blank output line, a misleading fix step, and wrong claims about checker scale, AgX, and noise. All fixed.
- Round 2: duplicated image pairs and table numbers that disagreed with the example pages. Fixed.
- Round 3 (current pages): every page ran as written with byte-identical images. Page 9 did not build on page 8, pages 4, 6, and 8 to 10 had small undocumented changes, page 11's hero pair was not near the median, its table did not say it shows one image per example, its timing claims were stronger than single runs support, and this file still said the clean-runner results were pending. All fixed in the current pages.
