# 3. Camera

Goal: choose the camera position yourself.

The script is page 2 plus one line: `camera_look_at` after `studio_setup`.

Download the full script, or copy it from below:

```sh
curl -O https://raw.githubusercontent.com/lmctl-ai/lmblender/main/docs/tutorial/code/03-camera.lms
```

<!-- code: code/03-camera.lms -->
```lmscript
p = _lmcode_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";
let ball = scene.uv_sphere(1.0, 48, 24, true);
ball.translate(0.0, 0.0, 1.0);
let box = scene.cube(1.6);
box.translate(-2.8, 0.0, 0.8);
let ring = scene.torus(0.8, 0.25, 48, 16, true);
ring.translate(2.8, 0.0, 0.25);
scene.studio_setup();
scene.camera_look_at(0.0, -9.0, 3.5, 0.0, 0.0, 0.8, 40.0);
scene.render_png("camera.png", 16, 640, 480);
end _lmcode_
file = "camera.dslb";
savedsl(p, file);
argv = dsl_cli(p);
push(argv, "--dsl");
push(argv, file);
r = system(argv, 3600000);
println(trim(r.stdout));
delete_file(file);
if (r.code != 0) { println(trim(f"lmblender exit {r.code}: {r.stderr}")); exit(1); }
println("wrote camera.png");
```

Run it:

```sh
lmscript 03-camera.lms
```

Expected output:

```text
{"ok": true, "result": null}
wrote camera.png
```

![The same three objects seen from the front and slightly above](images/camera.png)

- `camera_look_at(px, py, pz, tx, ty, tz, fov)` puts the camera at the first point, aims it at the second, and uses a vertical field of view in degrees. Here the camera is 9 units in front of the objects and 3.5 units up.
- A camera call made after `studio_setup` replaces its automatic framing; the floor and lights stay.

Always set a camera, or call `studio_setup` or `fit_camera`. Without one, lmblender v0.3.4 uses a fallback view that is squeezed horizontally in wide images (fixed in the source for the next release; see [page 11](11-compare-with-blender.md)).

Next: [4. Materials](04-materials.md).
