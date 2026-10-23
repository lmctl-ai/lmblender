# Open items

1. DONE in v0.3.4: macOS arm64/x64 and Windows x64 ship. Remaining: real-hardware smoke tests outside CI; Kimi seed needs login.
2. x64 Embree speedup not measured (arm64: ~5-10%).
3. Hair/curves, toon outline, sweep helpers; hostile-input fuzz under ASan.
4. Python-free Blender build (blendercli side) not started; low priority.
5. MIT relicensing of lmblender-authored combined work only if Cycles/Blender GPL terms ever allow (they do not today).
6. Lamps invisible to BSDF rays (use_mis false): decide whether to set use_mis=true by default (changes renders) or add an opt-in; DRAW_NEEDS entry.
7. material IOR parameter and label digits/punctuation (DRAW_NEEDS).
