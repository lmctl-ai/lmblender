# 6. Animation

Goal: move an object over time and render each frame.

The sphere, ground, light, and camera follow pages 2 and 3, with a smaller sphere and a smaller image so three frames render quickly. Two keyframes move the sphere from left to right.

```sh
curl -O https://raw.githubusercontent.com/lmctl-ai/lmblender/main/docs/tutorial/code/06-animation.lms
```

<!-- code: code/06-animation.lms -->
```lmscript
p = _dsl_ cli="lmblender" install="github:lmctl-ai/lmblender@latest";
let ball = scene.uv_sphere(0.6, 32, 16, true);
ball.set_material(scene.material(0.08, 0.25, 0.85, 0.0, 0.3, 0.0, 0.0, 0.0, 0.0, 0.0));
ball.keyframe_transform(0, -2.0, 0.0, 0.6, "ease");
ball.keyframe_transform(2, 2.0, 0.0, 0.6, "ease");
scene.ground_plane(0.0, 20.0, 0.7, 0.7, 0.72);
scene.area_light(-3.0, -4.0, 5.0, 0.0, 0.0, 1.0, 1.0, 0.92, 0.82, 600.0, 3.0);
scene.camera_look_at(0.0, -8.0, 3.0, 0.0, 0.0, 0.6, 40.0);
scene.render_sequence("animation-{frame}.png", 0, 2, 16, 320, 240);
end _dsl_
file = "animation.dslb";
savedsl(p, file);
r = system ${dsl_cli(p)} --dsl ${file};
delete_file(file);
if (r.code != 0) { println(trim(r.stdout)); println(trim(r.stderr)); exit(1); }
println("wrote animation-0.png, animation-1.png, animation-2.png");
```

```sh
lmscript 06-animation.lms
```

Expected output:

```text
wrote animation-0.png, animation-1.png, animation-2.png
```

Frame 0, frame 1, and frame 2:

![Frame 0: sphere on the left](images/animation-0.png)
![Frame 1: sphere in the middle](images/animation-1.png)
![Frame 2: sphere on the right](images/animation-2.png)

- `keyframe_transform(frame, x, y, z, interpolation)` sets the object's position at a frame. Positions are absolute, not offsets. `"ease"` starts and ends slowly; `"linear"` moves at constant speed.
- `render_sequence(pattern, first, last, samples, width, height)` renders every frame from first to last inclusive. `{frame}` in the path becomes the frame number.
- `render_apng(path, first, last, samples, width, height, fps)` writes one animated PNG instead.
- Cameras, visibility, material color, emission, and point light strength can also be keyed. See [ANIMATION.md](../ANIMATION.md).

Next: [7. View transforms](07-view-transforms.md).
