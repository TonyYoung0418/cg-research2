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
    mat("wall", {0.66, 0.62, 0.54});
    mat("dark_wall", {0.12, 0.10, 0.08});
    mat("trim", {0.78, 0.70, 0.56});
    mat("curtain", {0.36, 0.045, 0.040});
    mat("curtain_dark", {0.16, 0.025, 0.025});
    mat("rug", {0.34, 0.055, 0.045});
    mat("rug_border", {0.78, 0.58, 0.24});
    mat("table", {0.42, 0.20, 0.08});
    mat("table_dark", {0.08, 0.045, 0.025});
    mat("frame", {0.90, 0.62, 0.24});
    mat("brass", {0.80, 0.57, 0.22});
    mat("ceramic", {0.82, 0.76, 0.64});
    mat("leaf", {0.10, 0.35, 0.16});
    mat("book_red", {0.45, 0.055, 0.040});
    mat("book_blue", {0.055, 0.15, 0.34});
    mat("book_green", {0.08, 0.28, 0.16});
    mat("lamp_shade", {0.88, 0.77, 0.56});
    mat("mirror", {0.98, 0.99, 1.0});
    mat("painting_green", {0.16, 0.46, 0.40});
    mat("painting_orange", {0.78, 0.34, 0.12});
    mat("painting_red", {0.52, 0.08, 0.06});
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

    w.box("BackBaseboard", "trim", {-4.45, 0.02, -2.68}, {4.45, 0.18, -2.55});
    w.box("BackCrownMoulding", "trim", {-4.45, 2.90, -2.68}, {4.45, 3.06, -2.55});
    w.box("LeftBaseboard", "trim", {-4.42, 0.02, -2.70}, {-4.30, 0.18, 5.45});
    w.box("RightBaseboard", "trim", {4.30, 0.02, -2.70}, {4.42, 0.18, 5.45});
    for (int i = 0; i < 3; ++i) {
        double x0 = -4.05 + i * 1.25;
        double x1 = x0 + 0.92;
        w.box("WallPanelLeftTrim", "trim", {x0, 0.55, -2.66}, {x0 + 0.045, 2.05, -2.57});
        w.box("WallPanelRightTrim", "trim", {x1, 0.55, -2.66}, {x1 + 0.045, 2.05, -2.57});
        w.box("WallPanelTopTrim", "trim", {x0, 2.00, -2.66}, {x1 + 0.045, 2.05, -2.57});
        w.box("WallPanelBottomTrim", "trim", {x0, 0.55, -2.66}, {x1 + 0.045, 0.60, -2.57});
    }
    w.ellipseBand("WallClockFrame", "frame", {0.05, 2.55, -2.59}, 0.24, 0.24, 0.33, 0.33, -2.58, 56);
    w.ellipseSurface("WallClockFace", "ceramic", {0.05, 2.55, -2.585}, 0.22, 0.22, -2.575, 18);
    w.box("WallClockHandVertical", "matte_black", {0.03, 2.55, -2.55}, {0.07, 2.72, -2.51});
    w.box("WallClockHandHorizontal", "matte_black", {0.05, 2.53, -2.55}, {0.19, 2.57, -2.51});

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
    w.quad("TableRunner", "rug", {-1.70, 0.734, 1.12}, {1.70, 0.734, 1.12}, {1.70, 0.734, 4.78}, {-1.70, 0.734, 4.78});
    w.quad("TableRunnerLeftBorder", "rug_border", {-1.70, 0.736, 1.12}, {-1.58, 0.736, 1.12}, {-1.58, 0.736, 4.78}, {-1.70, 0.736, 4.78});
    w.quad("TableRunnerRightBorder", "rug_border", {1.58, 0.736, 1.12}, {1.70, 0.736, 1.12}, {1.70, 0.736, 4.78}, {1.58, 0.736, 4.78});
    w.quad("TableRunnerFrontBorder", "rug_border", {-1.70, 0.736, 4.62}, {1.70, 0.736, 4.62}, {1.70, 0.736, 4.78}, {-1.70, 0.736, 4.78});
    w.quad("TableRunnerBackBorder", "rug_border", {-1.70, 0.736, 1.12}, {1.70, 0.736, 1.12}, {1.70, 0.736, 1.28}, {-1.70, 0.736, 1.28});

    w.box("ArtFrameLeft", "frame", {-3.70, 2.20, -2.66}, {-3.58, 2.96, -2.54});
    w.box("ArtFrameRight", "frame", {-0.38, 2.20, -2.66}, {-0.26, 2.96, -2.54});
    w.box("ArtFrameTop", "frame", {-3.70, 2.84, -2.66}, {-0.26, 2.96, -2.54});
    w.box("ArtFrameBottom", "frame", {-3.70, 2.20, -2.66}, {-0.26, 2.32, -2.54});
    w.quad("WallArtLeft", "painting_green", {-3.6, 2.28, -2.76}, {-2.35, 2.28, -2.76}, {-2.35, 2.88, -2.76}, {-3.6, 2.88, -2.76});
    w.quad("WallArtMid", "painting_orange", {-2.35, 2.28, -2.76}, {-1.35, 2.28, -2.76}, {-1.35, 2.88, -2.76}, {-2.35, 2.88, -2.76});
    w.quad("WallArtRight", "painting_red", {-1.35, 2.28, -2.76}, {-0.35, 2.28, -2.76}, {-0.35, 2.88, -2.76}, {-1.35, 2.88, -2.76});

    w.box("LeftCurtain", "curtain", {1.82, 0.68, -2.58}, {2.12, 2.82, -2.34});
    w.box("RightCurtain", "curtain", {3.68, 0.68, -2.58}, {3.98, 2.82, -2.34});
    for (int i = 0; i < 5; ++i) {
        double y0 = 0.72 + i * 0.40;
        w.box("LeftCurtainFold", "curtain_dark", {1.83, y0, -2.32}, {2.12, y0 + 0.045, -2.20});
        w.box("RightCurtainFold", "curtain_dark", {3.68, y0, -2.32}, {3.97, y0 + 0.045, -2.20});
    }
    w.box("CurtainRod", "brass", {1.72, 2.80, -2.30}, {4.08, 2.90, -2.18});
    w.box("WindowFrameLeft", "matte_black", {2.15, 0.85, -2.72}, {2.25, 2.65, -2.60});
    w.box("WindowFrameRight", "matte_black", {3.55, 0.85, -2.72}, {3.65, 2.65, -2.60});
    w.box("WindowFrameTop", "matte_black", {2.15, 2.55, -2.72}, {3.65, 2.65, -2.60});
    w.box("WindowFrameBottom", "matte_black", {2.15, 0.85, -2.72}, {3.65, 0.95, -2.60});
    w.box("WindowMullionVertical", "matte_black", {2.86, 0.90, -2.70}, {2.94, 2.60, -2.56});
    w.box("WindowMullionHorizontal", "matte_black", {2.20, 1.70, -2.70}, {3.60, 1.78, -2.56});
    w.quad("WarmWindowGlow", "painting_orange", {2.25, 0.95, -2.68}, {3.55, 0.95, -2.68}, {3.55, 2.55, -2.68}, {2.25, 2.55, -2.68});

    w.box("BookcaseBody", "table_dark", {-3.95, 0.55, -2.50}, {-2.25, 1.48, -2.08});
    w.box("BookcaseTop", "table", {-4.02, 1.46, -2.55}, {-2.18, 1.58, -2.02});
    for (int i = 0; i < 3; ++i) {
        double x = -3.78 + i * 0.48;
        w.box("BookRed", "book_red", {x, 0.72, -2.02}, {x + 0.12, 1.26, -1.88});
        w.box("BookBlue", "book_blue", {x + 0.14, 0.72, -2.02}, {x + 0.25, 1.18, -1.88});
        w.box("BookGreen", "book_green", {x + 0.27, 0.72, -2.02}, {x + 0.39, 1.34, -1.88});
    }
    w.ellipseSurface("CeramicVaseBody", "ceramic", {-2.48, 1.03, -1.86}, 0.16, 0.28, -1.84, 18);
    w.box("PlantStem", "leaf", {-2.50, 1.25, -1.82}, {-2.46, 1.68, -1.78});
    w.quad("PlantLeafLeft", "leaf", {-2.48, 1.42, -1.80}, {-2.83, 1.60, -1.76}, {-2.78, 1.72, -1.74}, {-2.48, 1.54, -1.79});
    w.quad("PlantLeafRight", "leaf", {-2.48, 1.43, -1.80}, {-2.13, 1.61, -1.76}, {-2.18, 1.73, -1.74}, {-2.48, 1.55, -1.79});

    w.box("TableLampStem", "brass", {2.78, 0.73, 1.20}, {2.86, 1.34, 1.28});
    w.box("TableLampBase", "brass", {2.58, 0.72, 1.05}, {3.06, 0.82, 1.43});
    w.box("TableLampShade", "lamp_shade", {2.45, 1.27, 0.98}, {3.18, 1.60, 1.50});
    w.box("BookStackA", "book_blue", {-3.00, 0.73, 1.15}, {-2.30, 0.83, 1.55});
    w.box("BookStackB", "book_red", {-2.92, 0.84, 1.20}, {-2.20, 0.93, 1.62});
    w.ellipseSurface("SmallVaseFront", "ceramic", {-2.05, 1.00, 1.28}, 0.13, 0.25, 1.30, 18);
    w.quad("SmallVaseLeafA", "leaf", {-2.05, 1.22, 1.30}, {-2.32, 1.44, 1.34}, {-2.27, 1.55, 1.36}, {-2.03, 1.34, 1.31});
    w.quad("SmallVaseLeafB", "leaf", {-2.05, 1.24, 1.30}, {-1.77, 1.45, 1.34}, {-1.82, 1.56, 1.36}, {-2.04, 1.35, 1.31});

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
