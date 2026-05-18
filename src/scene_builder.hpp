#pragma once

#include <string>

namespace scene_builder {

struct BuildOptions {
    std::string objPath = "scenes/generated_scene.obj";
    std::string mtlPath = "scenes/generated_scene.mtl";
    double floorColor[3] = {0.36, 0.34, 0.30};
    double lightPos[3] = {-1.65, 2.82, 3.35};
    double lightSize = 0.45;
    bool extraLight = false;
};

void writeSceneObj(const BuildOptions &opt);

} // namespace scene_builder
