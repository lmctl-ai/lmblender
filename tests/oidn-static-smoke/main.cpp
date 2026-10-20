// SPDX-FileCopyrightText: 2025-2026 lmblender contributors
// SPDX-License-Identifier: MIT

#include <OpenImageDenoise/oidn.hpp>
#include <cmath>
#include <iostream>
#include <vector>
int main() {
  constexpr int width = 32, height = 32;
  oidn::DeviceRef device = oidn::newDevice(oidn::DeviceType::CPU);
  device.commit();
  std::vector<float> color(width * height * 3, 0.4f), albedo(width * height * 3, 0.5f);
  std::vector<float> normal(width * height * 3, 0.0f), output(width * height * 3, 0.0f);
  for (int i = 0; i < width * height; ++i) normal[i * 3 + 2] = 1.0f;
  oidn::FilterRef filter = device.newFilter("RT");
  filter.setImage("color", color.data(), oidn::Format::Float3, width, height);
  filter.setImage("albedo", albedo.data(), oidn::Format::Float3, width, height);
  filter.setImage("normal", normal.data(), oidn::Format::Float3, width, height);
  filter.setImage("output", output.data(), oidn::Format::Float3, width, height);
  filter.set("hdr", true);
  filter.commit();
  filter.execute();
  if (device.getError() != oidn::Error::None || !std::isfinite(output[(height/2*width+width/2)*3])) return 2;
  std::cout << "OIDN static CPU RT filter executed; center=" << output[(height/2*width+width/2)*3] << "\n";
  return 0;
}
