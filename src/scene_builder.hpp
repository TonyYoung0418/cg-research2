#pragma once

#include <string>

namespace scene_builder {

struct BuildOptions {
    std::string objPath = "scenes/generated_scene.obj";
    std::string mtlPath = "scenes/generated_scene.mtl";
    double floorColor[3] = {0.43, 0.27, 0.16};
    double lightPos[3] = {0.00, 3.45, 1.80};
    double lightSize = 2.20;
    bool extraLight = true;
};

void writeSceneObj(const BuildOptions &opt);

} // namespace scene_builder
