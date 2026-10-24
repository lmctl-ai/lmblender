#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2025-2026 lmblender contributors
# SPDX-License-Identifier: MIT
set -uo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
failures=0
qa_provider=${LMBLENDER_QA_PROVIDER:-agy}
qa_model=${LMBLENDER_QA_MODEL:-gemini-3.8-flash-high}
mkdir -p build/story-qa
: > build/story-qa/results.txt
qa() {
  local name=$1 path=$2 checks=$3 response verdict
  response=$(lmctl prompt --provider "$qa_provider" --model "$qa_model" "Inspect the local image $path directly. Checklist for $name: $checks Reply PASS only if every item visibly holds, else FAIL with item numbers and reason. Give a quality score 1-5.")
  printf '%s\n' "$response" > "build/story-qa/$name.txt"
  verdict=$(printf '%s\n' "$response" | grep -Eim1 '^[[:space:]]*(\*\*)?(PASS|FAIL)(\*\*)?([[:space:]:]|$)' || true)
  if [[ $verdict =~ ^[[:space:]]*(\*\*)?PASS(\*\*)?([[:space:]:]|$) ]]; then
    printf '%s: PASS\n' "$name" | tee -a build/story-qa/results.txt
  else
    printf '%s: FAIL\n' "$name" | tee -a build/story-qa/results.txt
    failures=$((failures + 1))
  fi
}
qa caustic-ball-lens "$PWD/build/caustic-ball-lens.png" '1) two glass spheres are fully visible above a floor; 2) one makes a smaller brighter focused spot on the floor while the other makes a broader dimmer patch; 3) the spheres read as transparent glass, not opaque balls.'
qa sunset-long-shadows "$PWD/build/sunset-long-shadows.png" '1) five posts are visible and fully framed; 2) low warm sunlight produces long distinct shadows on the ground; 3) the shadows point broadly toward the camera.'
qa god-rays-window "$PWD/build/god-rays-window.png" '1) a six-pane window shape is recognizable; 2) multiple light shafts pass through haze toward the floor; 3) floor light patches and the room remain visible, not an isolated glow.'
qa cornell-box "$PWD/build/cornell-box.png" '1) an enclosed box has distinct red and green side walls; 2) two white blocks are visible; 3) a ceiling light illuminates them and color bounce is visible without major clipping.'
qa backlit-skin "$PWD/build/backlit-skin.png" '1) four slabs are visible side by side; 2) backlighting makes them progress from lighter peach to deeper red; 3) all four are fully framed and can be compared.'
qa gear-pair-render "$PWD/docs/images/gear-pair-render.png" '1) two gears with visible teeth mesh together; 2) one reads as steel and the other as brass; 3) both are fully framed on a desk-like surface.'
qa caffeine-molecule "$PWD/build/caffeine-molecule.png" '1) one connected ball-and-stick molecule is visible; 2) atom colors include red oxygen and blue nitrogen; 3) bonds and most atoms are distinguishable without cropping.'
qa dna-double-helix "$PWD/build/dna-double-helix.png" '1) two winding backbone strands form a clear double helix; 2) colored rungs connect the strands; 3) the helical structure is fully framed and readable.'
qa salt-crystal "$PWD/build/salt-crystal.png" '1) a regular three-dimensional lattice of two alternating ion colors is visible; 2) the green and violet ions are distinct; 3) the lattice is fully framed and not a single flat grid.'
qa saturn-rings "$PWD/build/saturn-rings.png" '1) a planet is surrounded by broad, thin rings; 2) the rings contain a visible dark gap; 3) the planet casts a shadow on the rings and the whole system fits in frame.'
qa kepler-orbit "$PWD/build/kepler-orbit/frame-35.png" '1) an eccentric orbit and a bright central Sun are visible; 2) an orbiting planet, radial spokes or equal-time position markers show motion geometry; 3) the orbit is fully framed.'
qa bouncing-ball-strobe "$PWD/build/bouncing-ball-strobe.png" '1) multiple positions of a white bouncing ball are visible as a strobe trail; 2) red markers indicate successive bounce peak heights; 3) the trail and markers fit in frame.'
total=$(wc -l < build/story-qa/results.txt)
printf 'story QA totals: %s pass, %s fail\n' "$((total - failures))" "$failures"
((failures == 0))
