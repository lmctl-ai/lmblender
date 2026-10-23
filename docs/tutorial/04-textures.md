# 4. Textures

Goal: replace flat colors with procedural textures and add surface bump.

The lights, world, and camera follow page 3. The floor becomes a checker ground, one sphere gets a checker material, and the other gets a noise material with bump.

```sh
curl -O https://raw.githubusercontent.com/lmctl-ai/lmblender/main/docs/tutorial/code/04-textures.lms
```

<!-- code: code/04-textures.lms -->
```lmscript
p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";
let ball = scene.uv_sphere(1.0, 48, 24, true);
ball.translate(-1.3, 0.0, 1.0);
ball.set_material(scene.checker_material(0.9, 0.15, 0.06, 0.95, 0.85, 0.6, 4.0, 0.4));
let rock = scene.uv_sphere(1.0, 48, 24, true);
rock.translate(1.3, 0.0, 1.0);
let stone = scene.noise_material(0.2, 0.18, 0.15, 0.75, 0.7, 0.62, 3.0, 0.7);
stone.bump(0.4, 0.1, 12.0);
rock.set_material(stone);
scene.checker_ground(10.0, 4.0);
scene.world_color(0.6, 0.7, 0.9, 0.3);
scene.area_light(-3.0, -4.0, 5.0, 0.0, 0.0, 1.0, 1.0, 0.92, 0.82, 600.0, 3.0);
scene.camera_look_at(0.0, -7.0, 2.8, 0.0, 0.0, 0.9, 40.0);
scene.render_png("textures.png", 32, 640, 480);
end _dsl_
file = "textures.dslb";
savedsl(p, file);
r = system ${dsl_cli(p)} --dsl ${file};
delete_file(file);
if (r.code != 0) { println(trim(r.stdout)); println(trim(r.stderr)); exit(1); }
println("wrote textures.png");
```

```sh
lmscript 04-textures.lms
```

Expected output:

```text
wrote textures.png
```

![Checkered sphere and stone-like sphere on a checker floor](images/textures.png)

- `checker_material(r1, g1, b1, r2, g2, b2, scale, roughness)` alternates two colors. `scale` sets how many squares fit across the object.
- `noise_material(...)` uses the same arguments and blends the two colors with Cycles noise.
- `material.bump(strength, distance, scale)` adds a noise height pattern to the surface normal. Call it on the material before assigning it.
- `checker_ground(half_size, tile_scale)` is a ready-made dark and light checker floor.

Textures are placed in each mesh's own coordinates, so the pattern moves with the object.

To use an image file instead, call `scene.image_material("file.png", roughness)`. PNG and JPEG images are currently read without sRGB decoding, so they render lighter than in Blender; see [page 9](09-compare-with-blender.md).

Next: [5. Import a mesh](05-import-a-mesh.md).
