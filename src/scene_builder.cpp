#include "scene_builder.hpp"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace scene_builder {
namespace {

constexpr double kPi = 3.14159265358979323846;

struct Vec3 {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

class ObjWriter {
public:
    explicit ObjWriter(const std::string &path) : obj(path) {}

    int vertex(const Vec3 &p) {
        obj << "v " << p.x << " " << p.y << " " << p.z << "\n";
        return nextIndex++;
    }

    void tri(const std::string &object, const std::string &mat, const Vec3 &a, const Vec3 &b, const Vec3 &c) {
        obj << "o " << object << "\nusemtl " << mat << "\n";
        int ia = vertex(a), ib = vertex(b), ic = vertex(c);
        obj << "f " << ia << " " << ib << " " << ic << "\n";
    }

    void quad(const std::string &object, const std::string &mat, const Vec3 &a, const Vec3 &b, const Vec3 &c, const Vec3 &d) {
        tri(object, mat, a, b, c);
        tri(object, mat, a, c, d);
    }

    void box(const std::string &object, const std::string &mat, Vec3 mn, Vec3 mx) {
        Vec3 p000{mn.x, mn.y, mn.z}, p001{mn.x, mn.y, mx.z}, p010{mn.x, mx.y, mn.z}, p011{mn.x, mx.y, mx.z};
        Vec3 p100{mx.x, mn.y, mn.z}, p101{mx.x, mn.y, mx.z}, p110{mx.x, mx.y, mn.z}, p111{mx.x, mx.y, mx.z};
        quad(object, mat, p000, p001, p011, p010);
        quad(object, mat, p100, p110, p111, p101);
        quad(object, mat, p000, p100, p101, p001);
        quad(object, mat, p010, p011, p111, p110);
        quad(object, mat, p000, p010, p110, p100);
        quad(object, mat, p001, p101, p111, p011);
    }

    void ellipseBand(const std::string &object, const std::string &mat, Vec3 center,
                     double innerX, double innerY, double outerX, double outerY, double z, int segments) {
        obj << "o " << object << "\nusemtl " << mat << "\n";
        std::vector<int> in(segments), out(segments);
        for (int i = 0; i < segments; ++i) {
            double a = 2.0 * kPi * i / segments;
            in[i] = vertex({center.x + innerX * std::cos(a), center.y + innerY * std::sin(a), z});
            out[i] = vertex({center.x + outerX * std::cos(a), center.y + outerY * std::sin(a), z});
        }
        for (int i = 0; i < segments; ++i) {
            int j = (i + 1) % segments;
            obj << "f " << in[i] << " " << out[i] << " " << out[j] << "\n";
            obj << "f " << in[i] << " " << out[j] << " " << in[j] << "\n";
        }
    }

    void ellipseSurface(const std::string &object, const std::string &mat, Vec3 center,
                        double rx, double ry, double z, int bands) {
        obj << "o " << object << "\nusemtl " << mat << "\n";
        for (int b = 0; b < bands; ++b) {
            double y0n = -1.0 + 2.0 * b / bands;
            double y1n = -1.0 + 2.0 * (b + 1) / bands;
            double x0 = rx * std::sqrt(std::max(0.0, 1.0 - y0n * y0n));
            double x1 = rx * std::sqrt(std::max(0.0, 1.0 - y1n * y1n));
            double y0 = center.y + ry * y0n;
            double y1 = center.y + ry * y1n;
            int a = vertex({center.x - x0, y0, z});
            int bb = vertex({center.x + x0, y0, z});
            int c = vertex({center.x + x1, y1, z});
            int d = vertex({center.x - x1, y1, z});
            obj << "f " << a << " " << bb << " " << c << "\n";
            obj << "f " << a << " " << c << " " << d << "\n";
        }
    }

    void model(const std::string &path, const std::string &object, const std::string &mat,
               Vec3 translate, double scale, double rotateYRadians = 0.0) {
        std::ifstream in(path);
        if (!in) {
            std::cerr << "Could not open asset " << path << "\n";
            std::exit(1);
        }

        std::vector<Vec3> verts(1);
        std::vector<std::vector<int>> faces;
        std::string line;
        auto faceIndex = [](const std::string &tok) {
            size_t slash = tok.find('/');
            return std::stoi(slash == std::string::npos ? tok : tok.substr(0, slash));
        };
        auto transform = [&](Vec3 p) {
            double c = std::cos(rotateYRadians), s = std::sin(rotateYRadians);
            Vec3 q{p.x * scale, p.y * scale, p.z * scale};
            return Vec3{translate.x + c * q.x + s * q.z, translate.y + q.y, translate.z - s * q.x + c * q.z};
        };

        obj << "o " << object << "\nusemtl " << mat << "\n";
        while (std::getline(in, line)) {
            std::istringstream ss(line);
            std::string tag;
            ss >> tag;
            if (tag == "v") {
                Vec3 p;
                ss >> p.x >> p.y >> p.z;
                verts.push_back(p);
            } else if (tag == "f") {
                std::vector<int> ids;
                std::string tok;
                while (ss >> tok) ids.push_back(faceIndex(tok));
                faces.push_back(ids);
            }
        }

        std::vector<int> remap(verts.size());
        for (size_t i = 1; i < verts.size(); ++i) remap[i] = vertex(transform(verts[i]));
        for (const auto &ids : faces) {
            for (size_t i = 1; i + 1 < ids.size(); ++i) {
                obj << "f " << remap[ids[0]] << " " << remap[ids[i]] << " " << remap[ids[i + 1]] << "\n";
            }
        }
    }

    std::ofstream obj;

private:
    int nextIndex = 1;
};

void writeMtl(const BuildOptions &opt) {
    std::ofstream mtl(opt.mtlPath);
    auto mat = [&](const std::string &name, Vec3 kd, Vec3 ke = {0, 0, 0}) {
        mtl << "newmtl " << name << "\n";
        mtl << "Kd " << kd.x << " " << kd.y << " " << kd.z << "\n";
        mtl << "Ke " << ke.x << " " << ke.y << " " << ke.z << "\n\n";
    };
    mat("floor", {opt.floorColor[0], opt.floorColor[1], opt.floorColor[2]});
    mat("wall", {0.62, 0.56, 0.43});
    mat("dark_wall", {0.10, 0.085, 0.060});
    mat("trim", {0.78, 0.70, 0.56});
    mat("curtain", {0.36, 0.045, 0.040});
    mat("curtain_dark", {0.16, 0.025, 0.025});
    mat("rug", {0.34, 0.055, 0.045});
    mat("rug_border", {0.78, 0.58, 0.24});
    mat("table", {0.42, 0.20, 0.08});
    mat("table_dark", {0.08, 0.045, 0.025});
    mat("wood_light", {0.72, 0.36, 0.13});
    mat("wood_glow", {0.96, 0.55, 0.18});
    mat("frame", {0.90, 0.62, 0.24});
    mat("brass", {0.80, 0.57, 0.22});
    mat("ceramic", {0.82, 0.76, 0.64});
    mat("leaf", {0.10, 0.35, 0.16});
    mat("book_red", {0.45, 0.055, 0.040});
    mat("book_blue", {0.055, 0.15, 0.34});
    mat("book_green", {0.08, 0.28, 0.16});
    mat("lamp_shade", {0.88, 0.77, 0.56});
    mat("glass_warm", {0.78, 0.58, 0.18}, {0.55, 0.34, 0.09});
    mat("window_glow", {1.0, 0.72, 0.20}, {1.25, 0.72, 0.20});
    mat("stone", {0.30, 0.31, 0.29});
    mat("bark", {0.22, 0.10, 0.035});
    mat("blossom", {0.94, 0.88, 0.78});
    mat("painting_blue", {0.15, 0.48, 0.52}, {0.025, 0.075, 0.08});
    mat("painting_yellow", {0.72, 0.53, 0.14}, {0.10, 0.07, 0.015});
    mat("mirror", {0.98, 0.99, 1.0});
    mat("painting_green", {0.16, 0.46, 0.40}, {0.035, 0.08, 0.06});
    mat("painting_orange", {0.78, 0.34, 0.12}, {0.12, 0.045, 0.015});
    mat("painting_red", {0.52, 0.08, 0.06}, {0.08, 0.012, 0.008});
    mat("white_piece", {0.86, 0.82, 0.70});
    mat("black_piece", {0.035, 0.030, 0.026});
    mat("matte_black", {0.02, 0.02, 0.025});
    mat("light_panel", {1, 1, 1}, {22.0, 18.0, 13.5});
}

} // namespace

void writeSceneObj(const BuildOptions &opt) {
    writeMtl(opt);
    ObjWriter w(opt.objPath);
    w.obj << "mtllib generated_scene.mtl\n";

    w.quad("Floor", "floor", {-4.5, 0.0, -2.8}, {4.5, 0.0, -2.8}, {4.5, 0.0, 5.6}, {-4.5, 0.0, 5.6});
    w.quad("BackWall", "wall", {4.5, 0.0, -2.8}, {-4.5, 0.0, -2.8}, {-4.5, 3.1, -2.8}, {4.5, 3.1, -2.8});
    w.quad("LeftWall", "wall", {-4.5, 0.0, 5.6}, {-4.5, 0.0, -2.8}, {-4.5, 3.1, -2.8}, {-4.5, 3.1, 5.6});
    w.quad("RightWall", "dark_wall", {4.5, 0.0, -2.8}, {4.5, 0.0, 5.6}, {4.5, 3.1, 5.6}, {4.5, 3.1, -2.8});
    w.quad("Ceiling", "dark_wall", {-4.5, 3.1, 5.6}, {4.5, 3.1, 5.6}, {4.5, 3.1, -2.8}, {-4.5, 3.1, -2.8});

    w.box("TableTop", "table", {-4.2, 0.55, 0.10}, {4.2, 0.72, 5.35});
    for (int i = 0; i < 8; ++i) {
        double x = -4.2 + i * 1.2;
        w.box("TablePlankGap", "table_dark", {x - 0.016, 0.721, 0.12}, {x + 0.016, 0.729, 5.32});
    }
    for (int i = 0; i < 11; ++i) {
        double z = 0.35 + i * 0.42;
        w.quad("WoodLine", "table_dark", {-4.05, 0.731, z}, {4.05, 0.731, z + 0.012},
               {4.05, 0.731, z + 0.020}, {-4.05, 0.731, z + 0.008});
    }
    for (int i = 0; i < 18; ++i) {
        double z = 0.45 + i * 0.26;
        double x0 = -4.08 + 0.10 * (i % 3);
        w.quad("WoodGrainWarm", "wood_light", {x0, 0.738, z}, {4.05, 0.738, z + 0.018},
               {4.05, 0.738, z + 0.035}, {x0, 0.738, z + 0.012});
    }
    w.quad("WoodHighlight", "wood_glow", {-3.9, 0.740, 2.40}, {3.9, 0.740, 2.52}, {3.9, 0.740, 2.64}, {-3.9, 0.740, 2.55});
    w.quad("WoodHighlightRight", "wood_glow", {1.1, 0.741, 1.30}, {4.05, 0.741, 1.43}, {4.05, 0.741, 1.55}, {1.1, 0.741, 1.44});

    w.box("BackBaseboard", "trim", {-4.45, 0.02, -2.68}, {4.45, 0.16, -2.55});
    w.box("BackCrownMoulding", "trim", {-4.45, 2.92, -2.68}, {4.45, 3.08, -2.55});

    w.box("PaintingFrameLeft", "frame", {-4.02, 2.10, -2.62}, {-3.90, 2.92, -2.50});
    w.box("PaintingFrameRight", "frame", {0.45, 2.10, -2.62}, {0.57, 2.92, -2.50});
    w.box("PaintingFrameTop", "frame", {-4.02, 2.80, -2.62}, {0.57, 2.92, -2.50});
    w.box("PaintingFrameBottom", "frame", {-4.02, 2.10, -2.62}, {0.57, 2.22, -2.50});
    w.quad("PaintingPanelBlue", "painting_blue", {-3.90, 2.22, -2.57}, {-2.40, 2.22, -2.57}, {-2.40, 2.80, -2.57}, {-3.90, 2.80, -2.57});
    w.quad("PaintingPanelGreen", "painting_green", {-2.40, 2.22, -2.57}, {-1.10, 2.22, -2.57}, {-1.10, 2.80, -2.57}, {-2.40, 2.80, -2.57});
    w.quad("PaintingPanelYellow", "painting_yellow", {-1.10, 2.22, -2.57}, {-0.10, 2.22, -2.57}, {-0.10, 2.80, -2.57}, {-1.10, 2.80, -2.57});
    w.quad("PaintingPanelRed", "painting_red", {-0.10, 2.22, -2.57}, {0.45, 2.22, -2.57}, {0.45, 2.80, -2.57}, {-0.10, 2.80, -2.57});
    w.box("PaintingTreeTrunk", "bark", {-0.98, 2.18, -2.50}, {-0.88, 2.84, -2.44});
    w.quad("PaintingBranchA", "bark", {-0.95, 2.58, -2.49}, {-1.55, 2.70, -2.49}, {-1.52, 2.76, -2.47}, {-0.92, 2.64, -2.47});
    w.quad("PaintingBranchB", "bark", {-0.91, 2.66, -2.49}, {-0.30, 2.76, -2.49}, {-0.33, 2.82, -2.47}, {-0.94, 2.72, -2.47});
    for (int i = 0; i < 22; ++i) {
        double x = -1.95 + 0.18 * (i % 12);
        double y = 2.46 + 0.055 * (i % 5) + 0.10 * (i / 12);
        w.ellipseSurface("PaintingBlossom", "blossom", {x, y, -2.47}, 0.035, 0.030, -2.43, 8);
    }
    for (int i = 0; i < 16; ++i) {
        double x = -0.58 + 0.12 * (i % 8);
        double y = 2.48 + 0.065 * (i % 5);
        w.ellipseSurface("PaintingBlossomWarm", "blossom", {x, y, -2.47}, 0.040, 0.032, -2.43, 8);
    }
    w.quad("HighPaintingBlue", "painting_blue", {-3.82, 2.55, -2.38}, {-2.20, 2.55, -2.38}, {-2.20, 3.02, -2.38}, {-3.82, 3.02, -2.38});
    w.quad("HighPaintingGreen", "painting_green", {-2.20, 2.55, -2.38}, {-1.00, 2.55, -2.38}, {-1.00, 3.02, -2.38}, {-2.20, 3.02, -2.38});
    w.quad("HighPaintingYellow", "painting_yellow", {-1.00, 2.55, -2.38}, {-0.18, 2.55, -2.38}, {-0.18, 3.02, -2.38}, {-1.00, 3.02, -2.38});
    w.quad("HighPaintingRed", "painting_red", {-0.18, 2.55, -2.38}, {0.44, 2.55, -2.38}, {0.44, 3.02, -2.38}, {-0.18, 3.02, -2.38});
    w.box("HighPaintingTrunk", "bark", {-0.90, 2.52, -2.34}, {-0.80, 3.03, -2.28});
    for (int i = 0; i < 18; ++i) {
        double x = -1.62 + 0.14 * (i % 12);
        double y = 2.70 + 0.06 * (i % 4);
        w.ellipseSurface("HighPaintingBlossom", "blossom", {x, y, -2.34}, 0.035, 0.030, -2.27, 8);
    }
    w.quad("VisibleLeftPaintingBlue", "painting_blue", {-4.35, 2.34, -2.18}, {-3.30, 2.34, -2.18}, {-3.30, 2.88, -2.18}, {-4.35, 2.88, -2.18});
    w.quad("VisibleLeftPaintingGreen", "painting_green", {-3.30, 2.34, -2.18}, {-2.35, 2.34, -2.18}, {-2.35, 2.88, -2.18}, {-3.30, 2.88, -2.18});
    w.quad("VisibleLeftPaintingGold", "painting_yellow", {-2.35, 2.34, -2.18}, {-1.42, 2.34, -2.18}, {-1.42, 2.88, -2.18}, {-2.35, 2.88, -2.18});
    w.quad("VisibleLeftPaintingTrunk", "bark", {-2.05, 2.36, -2.12}, {-1.86, 2.36, -2.12}, {-1.66, 2.88, -2.12}, {-1.84, 2.88, -2.12});
    for (int i = 0; i < 26; ++i) {
        double x = -3.55 + 0.15 * (i % 12);
        double y = 2.52 + 0.065 * (i % 5);
        w.ellipseSurface("VisibleLeftPaintingBlossom", "blossom", {x, y, -2.12}, 0.034, 0.030, -2.06, 8);
    }

    w.box("WindowOuterLeft", "matte_black", {1.10, 0.35, -2.42}, {1.26, 3.05, -2.18});
    w.box("WindowOuterRight", "matte_black", {4.18, 0.35, -2.42}, {4.36, 3.05, -2.18});
    w.box("WindowOuterTop", "matte_black", {1.10, 2.88, -2.42}, {4.36, 3.05, -2.18});
    w.box("WindowOuterBottom", "matte_black", {1.10, 0.35, -2.42}, {4.36, 0.52, -2.18});
    w.quad("WindowWarmGlassA", "glass_warm", {1.26, 0.52, -2.37}, {2.52, 0.52, -2.37}, {2.52, 2.88, -2.37}, {1.26, 2.88, -2.37});
    w.quad("WindowWarmGlassB", "window_glow", {2.68, 0.52, -2.37}, {4.18, 0.52, -2.37}, {4.18, 2.88, -2.37}, {2.68, 2.88, -2.37});
    w.box("WindowCenterBar", "matte_black", {2.52, 0.40, -2.30}, {2.68, 3.05, -2.08});
    w.box("WindowRightBar", "matte_black", {3.78, 0.52, -2.30}, {3.92, 2.88, -2.08});
    for (int i = 0; i < 5; ++i) {
        double y = 0.78 + i * 0.34;
        w.box("WindowRightMullion", "matte_black", {2.86, y, -2.28}, {4.10, y + 0.055, -2.06});
    }
    w.quad("WarmInteriorGlow", "window_glow", {2.85, 0.55, -2.65}, {4.18, 0.55, -2.65}, {4.18, 2.85, -2.65}, {2.85, 2.85, -2.65});
    w.box("InteriorColumn", "matte_black", {2.95, 0.55, -2.02}, {3.10, 2.35, -1.80});
    w.box("InteriorColumnCap", "matte_black", {2.82, 2.28, -2.02}, {3.23, 2.43, -1.78});
    w.box("InteriorRailingBase", "bark", {1.50, 0.92, -2.03}, {3.00, 1.02, -1.82});
    w.quad("InteriorStairRailA", "bark", {1.40, 1.00, -1.92}, {2.62, 1.70, -1.92}, {2.58, 1.79, -1.86}, {1.36, 1.09, -1.86});
    w.quad("InteriorStairRailB", "bark", {1.55, 0.72, -1.91}, {2.82, 1.44, -1.91}, {2.78, 1.52, -1.85}, {1.51, 0.80, -1.85});
    for (int i = 0; i < 4; ++i) {
        double x = 1.65 + i * 0.32;
        w.box("RailingSpindle", "bark", {x, 0.88, -1.92}, {x + 0.045, 1.45, -1.82});
    }
    w.box("RightDarkDrape", "matte_black", {4.05, 0.40, -1.95}, {4.45, 3.00, -1.65});
    w.quad("StoneWallReflectionPlane", "stone", {-1.10, 0.86, -2.50}, {1.10, 0.86, -2.50}, {1.10, 1.80, -2.50}, {-1.10, 1.80, -2.50});
    for (int r = 0; r < 5; ++r) {
        for (int c = 0; c < 5; ++c) {
            double x0 = -1.08 + c * 0.44 + 0.05 * (r % 2);
            double y0 = 0.92 + r * 0.18;
            w.box("StoneJoint", "dark_wall", {x0, y0, -2.43}, {x0 + 0.36, y0 + 0.035, -2.36});
        }
    }

    w.box("MirrorBackPlate", "matte_black", {-1.32, 0.72, 0.84}, {1.32, 2.24, 0.90});
    w.quad("TableMirrorSurface", "mirror", {-1.08, 0.91, 0.915}, {1.08, 0.91, 0.915},
           {1.08, 2.05, 0.915}, {-1.08, 2.05, 0.915});
    w.box("MirrorFrameLeft", "frame", {-1.32, 0.72, 0.92}, {-1.08, 2.24, 1.03});
    w.box("MirrorFrameRight", "frame", {1.08, 0.72, 0.92}, {1.32, 2.24, 1.03});
    w.box("MirrorFrameTop", "frame", {-1.32, 2.05, 0.92}, {1.32, 2.24, 1.03});
    w.box("MirrorFrameBottom", "frame", {-1.32, 0.72, 0.92}, {1.32, 0.91, 1.03});

    double ls = opt.lightSize;
    Vec3 lp{opt.lightPos[0], opt.lightPos[1], opt.lightPos[2]};
    w.quad("MainLight", "light_panel", {lp.x - ls, lp.y, lp.z - 0.35 * ls}, {lp.x + ls, lp.y, lp.z - 0.35 * ls},
           {lp.x + ls, lp.y, lp.z + 0.35 * ls}, {lp.x - ls, lp.y, lp.z + 0.35 * ls});
    if (opt.extraLight) {
        w.quad("AuxLight", "light_panel", {-3.8, 2.1, 0.2}, {-3.8, 1.1, 0.2}, {-3.8, 1.1, 2.5}, {-3.8, 2.1, 2.5});
    }

    const std::string assetDir = "Free_Stuff_1_-__Chess_Set/OBJ/";
    auto place = [&](const std::string &file, const std::string &name, const std::string &mat,
                     Vec3 p, double angle = 0.0, double scale = 7.1) {
        w.model(assetDir + file, name, mat, p, scale, angle);
    };

    place("GEO_WhitePawn_08.obj", "VisiblePawnA", "white_piece", {-1.15, 0.73, 3.05}, 0.0, 6.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnB", "white_piece", {-0.46, 0.73, 2.98}, 0.0, 6.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnC", "white_piece", {0.46, 0.73, 2.98}, 0.0, 6.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnD", "white_piece", {1.15, 0.73, 3.05}, 0.0, 6.6);

    place("GEO_WhiteRook_02.obj", "VisibleWhiteRook", "white_piece", {-1.02, 0.73, 2.34}, 0.0, 6.8);
    place("GEO_WhiteKnight_02.obj", "CameraOnlyWhiteKnight", "white_piece", {-0.60, 0.73, 2.42}, -0.25, 6.8);
    place("GEO_WhiteBishop_02.obj", "ReflectionOnlyWhiteBishop", "white_piece", {-0.60, 0.73, 2.42}, 0.0, 7.0);
    place("GEO_WhiteBishop_02.obj", "VisibleWhiteBishop", "white_piece", {-0.22, 0.73, 2.50}, 0.0, 7.0);
    place("GEO_WhiteKing.obj", "VisibleWhiteKing", "white_piece", {0.20, 0.73, 2.47}, 0.0, 7.8);
    place("GEO_WhiteQueen.obj", "CameraOnlyWhiteQueen", "white_piece", {0.78, 0.73, 2.34}, 0.0, 7.1);
    place("GEO_WhiteKing.obj", "ReflectionOnlyWhiteKing", "white_piece", {0.78, 0.73, 2.34}, 0.0, 7.5);
}

} // namespace scene_builder
