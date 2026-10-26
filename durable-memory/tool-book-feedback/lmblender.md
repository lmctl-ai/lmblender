# lmblender tool book feedback

Session: lmblender book stages 2 to 7, 2026-10-22 and 2026-10-23, one Claude Opus 5.5 Lead on a four-core Neoverse-N1 (AWS Graviton2) Linux arm64 host. Feedback for the publish-tool-book recipe at https://lmctl.com/skills/publish-tool-book-skill.html, for lmblender, for lmscript, and for lmctl autorun.

Scope actually done: read the recipe and tool docs; ported all 44 top-level examples to Blender 5.2 Python and compared the renders; wrote an eleven-page tutorial with a checklist; fixed the lmblender bugs the comparison found; added README art with light and dark variants; checked the README in a browser. Not done yet: book.json, get_started.html, the solo and trio quickrun scripts, the lmctl.com build and deploy, and the provider autorun runs (recipe steps 4, 5, 7, 8, and 9). The lmctl autorun feedback below is therefore from reading the recipe and from how stage prompts arrived, not from running quickruns.

## What worked

1. Porting the examples to the reference tool through a module that copies the host's semantics. The bpy module reproduces the host's C++ geometry builders vertex for vertex, its light and camera matrices, its texture coordinates (standalone Cycles fills generated coordinates from object space, so the port uses the Object texture coordinate), and its keyframe interpolation. With that, differences in the images mean real differences in the renderer, not in the port. Median difference over 51 image pairs went from 5.45 to 0.52 of 255 once the bugs below were fixed.

2. A numeric side-by-side comparison found five real lmblender bugs that its own regression suite had been calibrated around: sRGB textures used as linear, a display lookup table that crushed dark tones by up to 20 of 255, a squeezed fallback camera, an arm64 denoiser built with SVE instructions, and example scripts that hid host errors.

3. Keeping every tutorial script in a file, syncing the page code blocks from those files, and checking the sync in test.sh. Pages never drifted from what was run.

4. A page runner (scripts/tutorial-run.py) that copies each page's script into an empty directory, runs it, and compares console output exactly and images within a tolerance, plus a workflow that runs it on fresh Linux x64, Linux arm64, macOS arm64, macOS x64, and Windows x64 runners. All 50 page runs passed; the largest image difference across platforms was 0.39 of 255.

5. An independent tester agent with a fresh HOME and an empty lmscript tool cache, reading the pages from GitHub. Three rounds found 9, then 4, then 7 problems, including overstated claims in the author's own checklist. Each round was worth its cost.

6. Playwright at 1280 by 900 and 390 by 844 in light and dark. It confirmed the theme-aware hero swaps correctly and caught the quick start code block scrolling sideways on a phone.

7. Replacing a reference image from a different rig with a matched-rig reference. The BlenderCLI bridge test compared lmblender against a Blender render with different lights and view transform, so its color gate (44 of 255) only measured the rig difference. The same scene rendered by Blender through the port gives a tight gate (difference 0.86 of 255, gate 4).

## What failed or cost time

1. Installing Blender on Linux arm64. There is no official Linux arm64 tarball (mirrors list x64 only up to 5.2.3), download.blender.org answers scripted requests with a Cloudflare 403, and the bpy wheels on PyPI are x86_64 only. A Blender 5.2.2 source build already on the host saved the stage; Ubuntu's packaged Blender 4.0.2 lacks OpenImageDenoise and its glTF importer lacks numpy.

2. The v0.3.3 and v0.3.4 linux-arm64 releases stop with SIGILL whenever denoising is on, on CPUs without SVE such as Graviton2. ISPC tunes for the build machine's CPU by default and GitHub's arm64 runners have SVE. CI never noticed because it runs on the SVE machine that built it; it took a run on an older CPU to see it. 21 of 44 examples denoise.

3. Test gates calibrated to buggy output. Fixing the color bugs broke four checks that had been tuned to the old output: the bridge color gate, the denoise silhouette mask, the transparent-film aspect classifier, and an Embree-versus-BVH2 PNG fixture stored in the old display encoding. Each needed a fix and a fresh 25 to 40 minute test.sh run; there were seven full runs in stage 6.

4. lmscript's system() stops a program after 60 seconds by default and reports r.code as null. Under load, motion-blur and volume-fog renders crossed that line, and the examples printed "lmblender exit null". My own pair runner first misread that as the SIGILL crash.

5. lmscript install=github:...@latest asks the GitHub API on every run. After a few hundred runs in a day the unauthenticated limit returned HTTP 403 and scripts failed to start. Setting GITHUB_TOKEN fixed it.

6. Small lmscript surprises cost minutes each: comments are not allowed inside a _dsl_ block and the error says only "expected an expression, found '/'"; adding two lists produces a comma-joined string instead of a list; cmd is a reserved name; the command-literal form of system cannot take a timeout.

7. My first fix for the ISPC CPU tuning pinned a CPU on x64 too, where OIDN compiles several ISPC targets and ISPC rejects a CPU with multiple targets. The arm64 host could not test the x64 path; the cold CI build caught it. Any build-script change should wait for both CI architectures before being called done.

8. Self-scoring drifted from the evidence. My checklist claimed a page chain that page 9 broke, called a hero image "close to the median" at 1.8 times the median, and still said clean-runner results were pending after they had passed. Only the independent reader caught these.

9. Two binaries in one book. Tutorial images must come from the released binary readers download (v0.3.4), while the comparison numbers come from a build with unreleased fixes. Keeping that distinction clear on every page took several rewrites.

## Changes proposed for the book recipe

1. Step 2, release verification: run the heaviest released code path (denoising, large renders) on the oldest supported CPU of each architecture, not only on CI runners, and scan release binaries for instruction-set leaks (for example SVE on arm64, AVX-512 on x64 outside runtime dispatch). Record the CPU model with the result.

2. Add an optional reference-tool comparison step for tools that reimplement an existing tool: port the examples, compare with a matched rig, publish per-example numbers and an honest gap list, and treat any regression gate tuned against the tool's own output as suspect.

3. Step 3, self-contained examples: require each example to print the host's JSON error on failure and a result line on success, to set an explicit system() timeout when it renders, and to avoid inputs that only a test harness generates. The 180 second render guidance should mention that lmscript's system() default is 60 seconds.

4. Step 6, README quick start: "no full URLs in narrow code blocks" cannot hold for a curl download line. Suggest linking the script in prose and keeping the code block to two short lines. The visual check should test each code block for internal horizontal scrolling at 390 pixels, not only page overflow.

5. Add tutorial guidance, which the recipe does not cover: one scene that grows page by page; scripts in files with a sync check; a runner that compares output and images; a workflow on clean runners for every platform; at least two independent reader rounds; checklist evidence that cites files and runs; no self-scored pass without an independent read.

6. Say which binary renders book images when fixes are not yet released: published pages show the released binary, comparison numbers name the build they come from, and publication waits for a release that contains the fixes, which the Lead authorizes.

7. Generalize the lmhtml rate-limit note: set GITHUB_TOKEN for lmscript installs in CI, in long QA sessions, and in the published troubleshooting text.

## Enhancement requests: lmblender

1. Publish a release after v0.3.4 so readers get the fixes in commits 34bd7cd, f6cfdc4, and 090d532 (sRGB textures, display LUT, fallback camera, arm64 denoiser, examples). Needs Lead approval.

2. Clean up temporary motion-blur shutter frames (files named like NAME.png.shutter-PID-N.png) when a render is cancelled or killed; a killed run leaves them in the output directory.

3. Let a still render pick an animation frame (for example a frame argument or render_frame call). Today render_png ignores keys and uses the authored state, which surprised both the port and the motion-blur example, whose control image showed the sphere half under the floor.

4. Warn, or note in the result JSON, when a scene sets no camera, and document the fallback view.

5. Add a per-image color space override (color versus data) for textures; the fix tags PNG and JPEG color images automatically, but users cannot override it.

6. Use background importance sampling for sky_texture and environment_texture. lmblender shows sun-disc sparkles on glossy floors that Blender does not; this is the largest remaining difference (5.5 of 255).

7. Document that import_gltf returns the first generated mesh's counts, so vertex and triangle checks differ from Blender's importer.

8. Run scripts/check-no-sve.sh on release assets in the release job, and add a matching x64 check for instructions above the baseline outside runtime dispatch.

## Enhancement requests: lmscript

1. Let the command-literal form of system take a timeout, or raise the default for _dsl_ host runs, and make a timeout visible: print or return timed_out in the error a script is most likely to show, instead of a bare null code.

2. Allow // comments inside _dsl_ blocks, or make the error say comments are not allowed there.

3. Make list plus list concatenate lists, or make it an error; today it silently produces a joined string.

4. Cache the resolved release for install=github:OWNER/REPO@latest for a while, use GITHUB_TOKEN automatically when it is set, and fall back to the cached binary when the GitHub API answers 403.

5. Add a helper that runs a DSL host with bytecode and options and returns the parsed JSON result, so every script does not repeat savedsl, argv building, system, and error printing.

6. List reserved identifiers (cmd was a surprise) in lmscript help.

## Enhancement requests: lmctl autorun

These come from the recipe and from how stage prompts reached this session; no quickrun was run in this book yet, so treat them as untested.

1. A seed-only check that also exercises the tool install path: lmscript version, GITHUB_TOKEN presence, and a dry install of the tool release, so rate limits and missing tokens show up before a provider session starts.

2. Pass GITHUB_TOKEN through to provider sessions by default, and surface HTTP 403 from installs as a named failure in the transcript.

3. Long stages leave background work running (CI runs, renders, monitors) when the next stage prompt arrives, and a previous session's monitors were marked stopped without a result. A stage prompt could carry or request a short status of still-running tasks, so the next stage starts from a known state.

## Additional recipe feedback, 2026-10-23

The release gate in step 2 is too strict for a documented multi-line version command. The v0.3.6 Linux arm64 binary reports its exact release as the first line, lmblender 0.3.6 (DSL bytecode v5), followed by the Cycles and OpenImageDenoise versions. The embedded schema matches the repository schema file byte for byte, and the binary signatures match the lmscript signatures generated from that schema. Requiring the entire output to be exactly NAME X.Y.Z rejects this valid release. Require the documented output to identify the exact tag version, and allow documented build or dependency details.

The release was freshly installed into an empty LMSCRIPT_TOOL_CACHE from the v0.3.6 tag; its reported SHA256 was ac8d91147d845c487352d66dafe4ccdcc240ee9a62828941384553b033f22526. The release is built from commit bdddf4a02487e566ca9675bc2cc43529a783e192. The checked-out main revision is 302c526fadb0328a625a06ad793e64b305ba179a and has successful CI and platform workflow runs. The 12-story examples were added after the v0.3.6 tag, so this book tests those source examples against the pinned released host rather than claiming they are present in the tag source tree.

The story examples use the command-literal form of system, which has a 60-second default timeout. The first 900 by 500 backlit-skin render exceeded that default, causing the script's assertion to fail before the renderer finished. Use an argument list with an explicit timeout for examples that render, while keeping the whole render under the 180-second book limit. The published copies use a 240-second host process timeout and the generator enforces the 180-second render limit.
