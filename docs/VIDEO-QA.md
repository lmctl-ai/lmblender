# Homepage video scene QA

Rendered with `examples/video/run.sh` at 640×360, 24 fps, 24 frames per clip. The runner deletes old frames first, so its gate only sees output from the current run. `tests/video_quality.py` decoded all 96 PNGs and checked each frame for dimensions and luminance deviation.

| Clip | Numeric result | Independent visual checklist |
| --- | --- | --- |
| Level 1 teams | Camera peek RGB delta 15.67/255; close frame is centered on Coder and Research with ring/orb margins. | QA PASS, 5/5: both teams and labels visible; rings and satellites fully framed; peek closer than wide view; teams distinct. |
| Level 2 agents | Red-orange message centroid traveled 212.4 px from first to last frame. | QA PASS, 5/5: agents, routed arrows, labels, and token visible and framed; no overlap hides the flow. |
| Level 3 flow | Message centroid traveled 329.7 px; colored card-region counts were HUMAN 951, LEAD 3232, TEAM 3301, RESULT 918 pixels. | QA PASS, 5/5: four cards, arrows, labels, and token visible; no overlap obscures the flow. |
| Level 4 pipeline | Token centroid traveled 325.5 px; final green check region covered 1,033 pixels. | QA PASS, 5/5: four stage labels, token, and final check visible and fully framed. |

I inspected representative frames from all four clips. The first review caught a clipped Level 1 ring and a hidden Level 4 token; those scene positions were fixed and the numeric and visual checks were rerun. The compositions intentionally use a compact flat graphic style, not photorealism. The font is extruded 5×7 ASCII geometry; bloom is not implemented, so orb glow comes from emissive surfaces and the gauge/ring geometry.
