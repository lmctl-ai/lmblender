# 9. View transforms

Goal: keep very bright light readable by choosing how scene light maps to pixels.

The script is page 8 plus one glowing orb above the scene, with `render_sequence` replaced by two still renders: once with the Standard view transform and once with AgX. The keyframes from page 8 stay in the script; still renders ignore keys and use each object's authored position.

Download the full script, or copy it from below:

```sh
curl -O https://raw.githubusercontent.com/lmctl-ai/lmblender/main/docs/tutorial/code/09-view-transforms.lms
```

<!-- code: code/09-view-transforms.lms -->
```lmscript
obj = """o pyramid
v -1 -1 0
v 1 -1 0
v 1 1 0
v -1 1 0
v 0 0 1.5
f 1 3 2
f 1 4 3
f 1 2 5
f 2 3 5
f 3 4 5
f 4 1 5
""";
write_file("pyramid.obj", obj);
p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";
let ball = scene.uv_sphere(1.0, 48, 24, true);
ball.translate(0.0, 0.0, 1.0);
let box = scene.cube(1.6);
box.translate(-2.8, 0.0, 0.8);
let ring = scene.torus(0.8, 0.25, 48, 16, true);
ring.translate(2.8, 0.0, 0.25);
ball.set_material(scene.checker_material(0.08, 0.25, 0.85, 0.9, 0.92, 0.95, 3.0, 0.3));
let stone = scene.noise_material(0.55, 0.12, 0.05, 0.95, 0.55, 0.35, 3.0, 0.6);
stone.bump(0.4, 0.1, 12.0);
box.set_material(stone);
ring.set_material(scene.material(0.95, 0.7, 0.2, 1.0, 0.25, 0.0, 0.0, 0.0, 0.0, 0.0));
ball.keyframe_transform(0, -1.2, -1.5, 1.0, "ease");
ball.keyframe_transform(2, 1.2, -1.5, 1.0, "ease");
let pyramid = scene.import_obj("pyramid.obj");
scene.expect_int(pyramid.triangle_count(), 6);
pyramid.translate(1.5, 2.6, 0.0);
pyramid.set_material(scene.material(0.85, 0.6, 0.2, 0.0, 0.35, 0.0, 0.0, 0.0, 0.0, 0.0));
let lamp = scene.glow_orb(0.0, 1.2, 3.0, 0.5, 1.0, 0.55, 0.2, 8.0);
scene.checker_ground(20.0, 1.0);
scene.world_color(0.6, 0.7, 0.9, 0.3);
scene.area_light(-3.0, -4.0, 5.0, 0.0, 0.0, 1.0, 1.0, 0.92, 0.82, 600.0, 3.0);
scene.camera_look_at(0.0, -9.0, 3.5, 0.0, 0.0, 0.8, 40.0);
scene.set_view_transform("Standard");
scene.render_png("view-standard.png", 32, 320, 240);
scene.set_view_transform("AgX");
scene.render_png("view-agx.png", 32, 320, 240);
end _dsl_
file = "view-transforms.dslb";
savedsl(p, file);
argv = dsl_cli(p);
push(argv, "--dsl");
push(argv, file);
r = system(argv, 3600000);
println(trim(r.stdout));
delete_file(file);
if (r.code != 0) { println(trim(f"lmblender exit {r.code}: {r.stderr}")); exit(1); }
println("wrote view-standard.png, view-agx.png");
```

Run it:

```sh
lmscript 09-view-transforms.lms
```

Expected output:

```text
{"ok": true, "result": null}
wrote view-standard.png, view-agx.png
```

![Standard: the glowing orb clips to flat white](images/view-standard.png)

![AgX: the orb keeps a pale warm tint and the whole image is softer](images/view-agx.png)

- `glow_orb(x, y, z, radius, r, g, b, strength)` adds a smooth emissive sphere. At strength 8 it is far brighter than white.
- `Standard` (the default) clips anything brighter than white, so the orb is a flat white disk. `AgX` compresses highlights, so the orb keeps a pale warm tint, and the rest of the image becomes slightly softer and less saturated.
- `set_view_transform` accepts `Standard`, `Filmic`, and `AgX`. It affects PNG output only; EXR files from `render_exr` keep the raw linear values.
- A script can render several images. Every render uses the settings in effect when it is called.

Next: [10. When it fails](10-when-it-fails.md).
