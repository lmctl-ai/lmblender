# Video scene samples

The four lmscript files build one-second 24 fps PNG sequences (24 frames each) in `build/video/`:

| Example | Scene | Auditable checks |
| --- | --- | --- |
| `level1-teams.lms` | Coder, Research, and Planner orb teams with different usage arcs and a camera peek | 24 640×360 frames; measurable camera change; all frames have luminance deviation >8/255 |
| `level2-agents.lms` | Colored agents around a lead, capsule avatar, routed arrows, moving message tokens | Same frame checks; message token moves >60 px between first and last frames |
| `level3-flow.lms` | Human → Lead → Team → Result with title plates and travelling message | Same frame checks; >80 colored pixels in each of four plate regions |
| `level4-pipeline.lms` | Input → Build → Review → Ship with a moving token and green check | Same frame checks; final check has >100 green pixels; token moves >60 px |

Run `./examples/video/run.sh` from the repo root. It compiles the scripts with `lmscript`, renders at draft quality, executes `tests/video_quality.py`, and encodes H.264 MP4s with ffmpeg when available. Four reviewed one-second sample clips are committed in `samples/`. The PNGs remain usable as a sequence if ffmpeg is absent. Adjust resolution and sample preset in the scripts for final delivery.

The built-in geometry helpers are `glow_orb`, `capsule`, `tube_between`, `arrow_between`, `ring_arc`, `checkmark`, and `label`. `label` uses extruded 5×7 uppercase ASCII letters and spaces; plates are ordinary cubes with text meshes placed on their front face. The orb's emission and ring gauge provide a simple graphic glow language; this renderer does not add a post-process bloom halo.
