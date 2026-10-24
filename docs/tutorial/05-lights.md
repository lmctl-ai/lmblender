# 5. Lights

Goal: light the scene yourself instead of using studio_setup.

The script is page 4 with `studio_setup()` replaced by three calls: a ground plane, a world color, and an area light. The camera from page 3 keeps the framing.

Download the full script, or copy it from below:

```sh
curl -O https://raw.githubusercontent.com/lmctl-ai/lmblender/main/docs/tutorial/code/05-lights.lms
```

<!-- code: code/05-lights.lms -->
```lmscript
p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";
let ball = scene.uv_sphere(1.0, 48, 24, true);
ball.translate(0.0, 0.0, 1.0);
let box = scene.cube(1.6);
box.translate(-2.8, 0.0, 0.8);
let ring = scene.torus(0.8, 0.25, 48, 16, true);
ring.translate(2.8, 0.0, 0.25);
ball.set_material(scene.material(0.08, 0.25, 0.85, 0.0, 0.3, 0.0, 0.0, 0.0, 0.0, 0.0));
box.set_material(scene.material(0.9, 0.15, 0.06, 0.0, 0.4, 0.0, 0.0, 0.0, 0.0, 0.0));
ring.set_material(scene.material(0.95, 0.7, 0.2, 1.0, 0.25, 0.0, 0.0, 0.0, 0.0, 0.0));
scene.ground_plane(0.0, 20.0, 0.7, 0.7, 0.72);
scene.world_color(0.6, 0.7, 0.9, 0.3);
scene.area_light(-3.0, -4.0, 5.0, 0.0, 0.0, 1.0, 1.0, 0.92, 0.82, 600.0, 3.0);
scene.camera_look_at(0.0, -9.0, 3.5, 0.0, 0.0, 0.8, 40.0);
scene.render_png("lights.png", 32, 640, 480);
end _dsl_
file = "lights.dslb";
savedsl(p, file);
argv = dsl_cli(p);
push(argv, "--dsl");
push(argv, file);
r = system(argv, 3600000);
println(trim(r.stdout));
delete_file(file);
if (r.code != 0) { println(trim(f"lmblender exit {r.code}: {r.stderr}")); exit(1); }
println("wrote lights.png");
```

Run it:

```sh
lmscript 05-lights.lms
```

Expected output:

```text
{"ok": true, "result": null}
wrote lights.png
```

![The three objects on a light floor, lit from the front left](images/lights.png)

- `ground_plane(z, half_size, r, g, b)` adds a matte floor.
- `world_color(r, g, b, strength)` sets the sky color, which also lights the scene softly.
- `area_light(x, y, z, tx, ty, tz, r, g, b, power, size)` adds a square light at the first point facing the second. Power is in watts, as in Blender.
- Once a scene has any light call, lmblender adds no light of its own.

The bright speckles on the floor around the torus are light bounced off the gold metal that 32 samples have not averaged out yet. More samples remove them at the cost of render time. `scene.render_settings(bounces, seed, true)` turns on the OIDN denoiser, but read the known issue on [page 10](10-when-it-fails.md) before you use it on Linux arm64.

Next: [6. Textures](06-textures.md).
