#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
"""Check the embedded manifest and render using Unicode and long Windows paths."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET

from png_metrics import decode

exe = Path(sys.argv[1]).resolve()
assert sys.platform == "win32", "this smoke test requires Windows"
with tempfile.TemporaryDirectory(prefix="lmblender-") as temporary:
    work = Path(temporary) / "rendu-é-日本語"
    work.mkdir()
    manifest = work / "embedded.xml"
    subprocess.run(["mt.exe", f"-inputresource:{exe};#1", f"-out:{manifest}"], check=True)
    tree = ET.parse(manifest)
    assert tree.find(".//{http://schemas.microsoft.com/SMI/2019/WindowsSettings}activeCodePage").text == "UTF-8"
    assert tree.find(".//{http://schemas.microsoft.com/SMI/2016/WindowsSettings}longPathAware").text == "true"
    folder = work
    while len(str(folder)) < 280:
        folder /= "long-directory-0123456789"
    folder.mkdir(parents=True)
    output = folder / "résultat-日本語.png"
    assert len(str(output)) > 260
    source = work / "scène-日本語.lms"
    bytecode = work / "scène-日本語.dslb"
    source.write_text('p = _dsl_ cli="lmblender";\n'
                      'scene.world_color(0.5, 0.5, 0.5, 1.0);\n'
                      f'scene.render_png({json.dumps(output.as_posix(), ensure_ascii=False)}, 1, 16, 16);\n'
                      'end _dsl_\nsavedsl(p, ARGV[0]);\n', encoding="utf-8")
    env = dict(os.environ, PATH=str(exe.parent) + os.pathsep + os.environ["PATH"])
    subprocess.run([shutil.which("lmscript"), str(source), str(bytecode)], cwd=work, env=env, check=True)
    run = subprocess.run([str(exe), "--dsl", str(bytecode)], cwd=work, capture_output=True)
    stdout, stderr = run.stdout.decode("utf-8"), run.stderr.decode("utf-8")
    assert run.returncode == 0, (stdout, stderr)
    assert json.loads(stdout)["ok"], stdout
    assert output.is_file(), (str(output), stdout, stderr)
    assert decode(output)[:2] == (16, 16)
    print(f"Windows paths: embedded UTF-8/longPathAware manifest; Unicode cwd/DSL; {len(str(output))}-character PNG path passed")
