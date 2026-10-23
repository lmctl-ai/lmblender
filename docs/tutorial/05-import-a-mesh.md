# 5. Import a mesh

Goal: render geometry from a file instead of built-in shapes.

The script first writes a small OBJ pyramid with lmscript's `write_file`, so it needs no download. Inside the `_dsl_` block it imports the file, checks the triangle count, assigns a material, and reuses `studio_setup` from page 1.

```sh
curl -O https://raw.githubusercontent.com/lmctl-ai/lmblender/main/docs/tutorial/code/05-import-a-mesh.lms
```

<!-- code: code/05-import-a-mesh.lms -->
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
let pyramid = scene.import_obj("pyramid.obj");
scene.expect_int(pyramid.triangle_count(), 6);
pyramid.set_material(scene.material(0.85, 0.6, 0.2, 0.0, 0.35, 0.0, 0.0, 0.0, 0.0, 0.0));
scene.studio_setup();
scene.render_png("import-a-mesh.png", 32, 640, 480);
end _dsl_
file = "import-a-mesh.dslb";
savedsl(p, file);
r = system ${dsl_cli(p)} --dsl ${file};
delete_file(file);
if (r.code != 0) { println(trim(r.stdout)); if (trim(r.stderr) != "") { println(trim(r.stderr)); } exit(1); }
println("wrote import-a-mesh.png");
```

```sh
lmscript 05-import-a-mesh.lms
```

Expected output:

```text
wrote import-a-mesh.png
```

The script also leaves `pyramid.obj` in the directory.

![A gold pyramid on a studio floor](images/import-a-mesh.png)

- `import_obj`, `import_stl`, `import_ply`, and `import_gltf` (for `.gltf` and `.glb`) each return a mesh you can move, scale, and give a material.
- Coordinates are used as written: Z up, scene units. Rotate or scale Y-up or millimeter models yourself before `studio_setup`.
- `scene.expect_int(actual, expected)` stops the render with an error if the two numbers differ. Use it to catch a wrong or truncated input file before you spend time rendering.
- OBJ files with an MTL library keep their colors and maps; assigning a material overrides them.

Next: [6. Animation](06-animation.md).
