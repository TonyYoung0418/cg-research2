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

    int normal(const Vec3 &n) {
        obj << "vn " << n.x << " " << n.y << " " << n.z << "\n";
        return nextNormalIndex++;
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

    void disk(const std::string &object, const std::string &mat, Vec3 center,
              double rx, double ry, double z, int segments) {
        obj << "o " << object << "\nusemtl " << mat << "\n";
        int mid = vertex({center.x, center.y, z});
        std::vector<int> ring(segments);
        for (int i = 0; i < segments; ++i) {
            double a = 2.0 * kPi * i / segments;
            ring[i] = vertex({center.x + rx * std::cos(a), center.y + ry * std::sin(a), z});
        }
        for (int i = 0; i < segments; ++i) {
            int j = (i + 1) % segments;
            obj << "f " << mid << " " << ring[i] << " " << ring[j] << "\n";
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
        std::vector<Vec3> norms(1);
        std::vector<std::vector<int>> faces;
        std::vector<std::vector<int>> faceNormals;
        std::string line;
        auto faceIndex = [](const std::string &tok) {
            size_t slash = tok.find('/');
            return std::stoi(slash == std::string::npos ? tok : tok.substr(0, slash));
        };
        auto normalIndex = [](const std::string &tok) {
            size_t slash = tok.find('/');
            if (slash == std::string::npos) return 0;
            size_t slash2 = tok.find('/', slash + 1);
            if (slash2 == std::string::npos || slash2 + 1 >= tok.size()) return 0;
            return std::stoi(tok.substr(slash2 + 1));
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
            } else if (tag == "vn") {
                Vec3 n;
                ss >> n.x >> n.y >> n.z;
                norms.push_back(n);
            } else if (tag == "f") {
                std::vector<int> ids;
                std::vector<int> nids;
                std::string tok;
                while (ss >> tok) {
                    ids.push_back(faceIndex(tok));
                    nids.push_back(normalIndex(tok));
                }
                faces.push_back(ids);
                faceNormals.push_back(nids);
            }
        }

        std::vector<int> remap(verts.size());
        std::vector<int> normalRemap(norms.size());
        for (size_t i = 1; i < verts.size(); ++i) remap[i] = vertex(transform(verts[i]));
        for (size_t i = 1; i < norms.size(); ++i) {
            Vec3 n = norms[i];
            double c = std::cos(rotateYRadians), s = std::sin(rotateYRadians);
            Vec3 r{c * n.x + s * n.z, n.y, -s * n.x + c * n.z};
            double len = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
            if (len > 0.0) {
                r.x /= len;
                r.y /= len;
                r.z /= len;
            }
            normalRemap[i] = normal(r);
        }
        for (size_t f = 0; f < faces.size(); ++f) {
            const auto &ids = faces[f];
            const auto &nids = faceNormals[f];
            for (size_t i = 1; i + 1 < ids.size(); ++i) {
                int a = remap[ids[0]];
                int b = remap[ids[i]];
                int c = remap[ids[i + 1]];
                int na = (nids.size() > 0 && nids[0] > 0 && nids[0] < int(normalRemap.size())) ? normalRemap[nids[0]] : 0;
                int nb = (nids.size() > i && nids[i] > 0 && nids[i] < int(normalRemap.size())) ? normalRemap[nids[i]] : 0;
                int nc = (nids.size() > i + 1 && nids[i + 1] > 0 && nids[i + 1] < int(normalRemap.size())) ? normalRemap[nids[i + 1]] : 0;
                if (na > 0 && nb > 0 && nc > 0) {
                    obj << "f " << a << "//" << na << " " << b << "//" << nb << " " << c << "//" << nc << "\n";
                } else {
                    obj << "f " << a << " " << b << " " << c << "\n";
                }
            }
        }
    }

    std::ofstream obj;

private:
    int nextIndex = 1;
    int nextNormalIndex = 1;
};

void writeMtl(const BuildOptions &opt) {
    std::ofstream mtl(opt.mtlPath);
    auto mat = [&](const std::string &name, Vec3 kd, Vec3 ke = {0, 0, 0}) {
        mtl << "newmtl " << name << "\n";
        mtl << "Kd " << kd.x << " " << kd.y << " " << kd.z << "\n";
        mtl << "Ke " << ke.x << " " << ke.y << " " << ke.z << "\n\n";
    };
    mat("floor", {opt.floorColor[0], opt.floorColor[1], opt.floorColor[2]});
    mat("floor_sheen", {0.62, 0.50, 0.38});
    mat("wall", {0.55, 0.36, 0.24});
    mat("dark_wall", {0.20, 0.12, 0.075});
    mat("trim", {0.78, 0.68, 0.52});
    mat("curtain", {0.36, 0.045, 0.040});
    mat("curtain_dark", {0.16, 0.025, 0.025});
    mat("rug", {0.26, 0.14, 0.075});
    mat("rug_border", {0.70, 0.50, 0.28});
    mat("table", {0.32, 0.16, 0.065});
    mat("table_dark", {0.075, 0.042, 0.024});
    mat("wood_light", {0.63, 0.33, 0.15});
    mat("wood_glow", {0.82, 0.52, 0.27});
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
    mat("plaster_light", {0.72, 0.58, 0.43});
    mat("plaster_raw", {0.36, 0.20, 0.12});
    mat("plaster_shadow", {0.16, 0.095, 0.060});
    mat("old_wood", {0.34, 0.16, 0.060});
    mat("old_wood_dark", {0.12, 0.060, 0.030});
    mat("old_wood_light", {0.60, 0.30, 0.11});
    mat("dust", {0.18, 0.14, 0.10});
    mat("shadow_hole", {0.018, 0.014, 0.011});
    mat("light_panel", {1, 1, 1}, {18.0, 14.0, 10.0});
    mat("light_fill", {1, 1, 1}, {10.0, 8.0, 6.0});
}

} // namespace

void writeSceneObj(const BuildOptions &opt) {
    writeMtl(opt);
    ObjWriter w(opt.objPath);
    w.obj << "mtllib generated_scene.mtl\n";

    w.quad("Floor", "floor", {-5.8, 0.0, -3.35}, {5.8, 0.0, -3.35}, {5.8, 0.0, 7.2}, {-5.8, 0.0, 7.2});
    w.quad("BackWall", "wall", {5.8, 0.0, -3.35}, {-5.8, 0.0, -3.35}, {-5.8, 3.5, -3.35}, {5.8, 3.5, -3.35});
    w.quad("LeftWall", "dark_wall", {-5.8, 0.0, 7.2}, {-5.8, 0.0, -3.35}, {-5.8, 3.5, -3.35}, {-5.8, 3.5, 7.2});
    w.quad("RightWall", "wall", {5.8, 0.0, -3.35}, {5.8, 0.0, 7.2}, {5.8, 3.5, 7.2}, {5.8, 3.5, -3.35});
    w.quad("Ceiling", "plaster_light", {-5.8, 3.5, 7.2}, {5.8, 3.5, 7.2}, {5.8, 3.5, -3.35}, {-5.8, 3.5, -3.35});

    w.box("BackBaseboard", "trim", {-5.70, 0.02, -3.24}, {5.70, 0.16, -3.08});
    w.box("RightBaseboard", "trim", {5.62, 0.02, -3.10}, {5.78, 0.16, 7.00});
    w.box("LeftBaseboard", "old_wood_dark", {-5.78, 0.02, -3.10}, {-5.62, 0.16, 7.00});
    w.box("BackCrownMoulding", "trim", {-5.70, 3.26, -3.24}, {5.70, 3.44, -3.08});

    w.box("DoorFrameLeft", "old_wood_dark", {3.10, 0.00, -3.06}, {3.30, 2.88, -2.76});
    w.box("DoorFrameRight", "old_wood_dark", {4.42, 0.00, -3.06}, {4.62, 2.88, -2.76});
    w.box("DoorFrameTop", "old_wood_dark", {3.10, 2.70, -3.06}, {4.62, 2.88, -2.76});
    w.box("OldDoorPanel", "old_wood", {3.32, 0.00, -3.00}, {4.40, 2.66, -2.82});
    w.box("DoorInsetUpper", "old_wood_light", {3.50, 1.55, -2.80}, {4.22, 2.42, -2.70});
    w.box("DoorInsetLower", "old_wood_light", {3.50, 0.32, -2.80}, {4.22, 1.20, -2.70});
    w.box("DoorCenterRail", "old_wood_dark", {3.83, 0.06, -2.68}, {3.96, 2.56, -2.56});
    w.disk("DoorKnob", "brass", {4.26, 1.28, -2.52}, 0.065, 0.065, -2.48, 16);

    w.box("MantelTop", "old_wood_dark", {-4.95, 1.18, -3.06}, {-2.08, 1.36, -2.68});
    w.box("MantelShelf", "old_wood", {-5.16, 1.34, -2.98}, {-1.86, 1.48, -2.56});
    w.box("MantelLeftPost", "old_wood_dark", {-4.92, 0.00, -3.00}, {-4.64, 1.30, -2.62});
    w.box("MantelRightPost", "old_wood_dark", {-2.38, 0.00, -3.00}, {-2.10, 1.30, -2.62});
    w.box("FireplaceShadow", "shadow_hole", {-4.48, 0.00, -2.88}, {-2.58, 1.05, -2.62});
    w.box("FireplaceRubbleBack", "plaster_shadow", {-4.36, 0.08, -2.58}, {-2.70, 0.98, -2.44});
    for (int i = 0; i < 8; ++i) {
        double x = -4.30 + 0.21 * i;
        w.box("FireplaceBrick", "plaster_raw", {x, 0.12 + 0.10 * (i % 3), -2.44}, {x + 0.14, 0.17 + 0.10 * (i % 3), -2.30});
    }
    w.disk("RoundWallHole", "shadow_hole", {-3.10, 1.82, -2.52}, 0.20, 0.17, -2.48, 24);

    w.quad("CeilingStain", "plaster_shadow", {-3.50, 3.45, 4.40}, {1.35, 3.45, 4.05}, {2.45, 3.45, -0.90}, {-2.05, 3.45, -0.55});
    w.quad("CeilingLightPatch", "plaster_light", {-0.80, 3.445, 3.05}, {4.65, 3.445, 2.82}, {4.70, 3.445, 0.12}, {0.10, 3.445, 0.42});
    for (int i = 0; i < 20; ++i) {
        double x = -4.80 + 0.52 * (i % 10);
        double z = -2.80 + 0.34 * (i / 10) + 0.15 * (i % 3);
        w.quad("CeilingTrowelMarks", "plaster_raw", {x, 3.435, z}, {x + 0.30, 3.435, z + 0.055},
               {x + 0.32, 3.435, z + 0.080}, {x + 0.02, 3.435, z + 0.025});
    }

    for (int i = 0; i < 18; ++i) {
        double x = -5.20 + 0.58 * (i % 9);
        double y = 0.62 + 0.22 * (i / 9) + 0.05 * (i % 4);
        w.quad("BackWallScrape", (i % 3 == 0) ? "plaster_light" : "plaster_raw",
               {x, y, -3.05}, {x + 0.36, y + 0.06, -3.05}, {x + 0.31, y + 0.12, -3.01}, {x - 0.04, y + 0.04, -3.01});
    }
    for (int i = 0; i < 12; ++i) {
        double x = -5.10 + 0.76 * (i % 6);
        double y = 1.70 + 0.24 * (i / 6) + 0.04 * (i % 3);
        w.quad("BackWallCrack", "plaster_shadow", {x, y, -3.03}, {x + 0.38, y + 0.09, -3.03},
               {x + 0.39, y + 0.11, -3.00}, {x + 0.01, y + 0.02, -3.00});
    }
    for (int i = 0; i < 10; ++i) {
        double x = -5.80;
        double y = 0.46 + 0.22 * (i % 5);
        double z = -2.45 + 0.50 * (i / 5);
        w.quad("LeftWallDamage", (i % 2 == 0) ? "plaster_raw" : "plaster_shadow",
               {x + 0.08, y, z}, {x + 0.08, y + 0.16, z + 0.05}, {x + 0.08, y + 0.13, z + 0.32}, {x + 0.08, y - 0.02, z + 0.20});
    }
    for (int i = 0; i < 14; ++i) {
        double x = 5.72;
        double y = 0.36 + 0.17 * (i % 7);
        double z = -2.20 + 0.54 * (i / 7);
        w.quad("RightWallPeel", (i % 3 == 0) ? "plaster_light" : "plaster_raw",
               {x - 0.08, y, z}, {x - 0.08, y + 0.11, z + 0.10}, {x - 0.08, y + 0.10, z + 0.36}, {x - 0.08, y - 0.02, z + 0.28});
    }

    w.quad("DustPatchNearDoor", "dust", {2.30, 0.018, -2.70}, {4.80, 0.018, -2.70}, {4.45, 0.018, -1.64}, {2.10, 0.018, -1.58});
    w.quad("DustPatchFireplace", "dust", {-4.80, 0.019, -2.60}, {-1.55, 0.019, -2.68}, {-1.85, 0.019, -1.48}, {-4.45, 0.019, -1.34});
    for (int i = 0; i < 20; ++i) {
        double x = -5.00 + 0.48 * (i % 10);
        double z = -2.35 + 0.28 * (i / 10) + 0.04 * (i % 4);
        w.box("SmallDebris", (i % 2 == 0) ? "old_wood_dark" : "plaster_raw",
              {x, 0.02, z}, {x + 0.07 + 0.03 * (i % 3), 0.055, z + 0.035 + 0.02 * (i % 2)});
    }
    w.box("LooseBoardA", "old_wood_dark", {-3.85, 0.035, -1.00}, {-1.95, 0.105, -0.82});
    w.box("LooseBoardB", "old_wood", {2.45, 0.035, -1.04}, {3.90, 0.100, -0.86});

    w.box("MirrorBackPlate", "matte_black", {-0.92, 0.06, 1.10}, {0.92, 1.16, 1.16});
    w.quad("FloorMirrorSurface", "mirror", {-0.72, 0.22, 1.175}, {0.72, 0.22, 1.175},
           {0.72, 0.98, 1.175}, {-0.72, 0.98, 1.175});
    w.box("MirrorFrameLeft", "frame", {-0.92, 0.06, 1.18}, {-0.72, 1.16, 1.28});
    w.box("MirrorFrameRight", "frame", {0.72, 0.06, 1.18}, {0.92, 1.16, 1.28});
    w.box("MirrorFrameTop", "frame", {-0.92, 0.98, 1.18}, {0.92, 1.16, 1.28});
    w.box("MirrorFrameBottom", "frame", {-0.92, 0.06, 1.18}, {0.92, 0.22, 1.28});

    double ls = opt.lightSize;
    Vec3 lp{opt.lightPos[0], opt.lightPos[1], opt.lightPos[2]};
    w.quad("MainLight", "light_panel", {lp.x - ls, lp.y, lp.z - 0.35 * ls}, {lp.x + ls, lp.y, lp.z - 0.35 * ls},
           {lp.x + ls, lp.y, lp.z + 0.35 * ls}, {lp.x - ls, lp.y, lp.z + 0.35 * ls});
    w.quad("FillLight", "light_fill", {-5.20, 2.95, 6.20}, {5.20, 2.95, 6.20}, {5.20, 2.95, 3.55}, {-5.20, 2.95, 3.55});
    if (opt.extraLight) {
        w.quad("AuxLight", "light_fill", {-5.45, 2.75, 6.20}, {-5.45, 1.35, 6.20}, {-5.45, 1.35, 3.55}, {-5.45, 2.75, 3.55});
    }

    const std::string assetDir = "Free_Stuff_1_-__Chess_Set/OBJ/";
    auto place = [&](const std::string &file, const std::string &name, const std::string &mat,
                     Vec3 p, double angle = 0.0, double scale = 7.1) {
        w.model(assetDir + file, name, mat, p, scale, angle);
    };

    place("GEO_WhitePawn_08.obj", "VisiblePawnA", "ceramic", {-1.02, 0.01, 3.50}, 0.0, 5.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnB", "ceramic", {-0.36, 0.01, 3.44}, 0.0, 5.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnC", "ceramic", {0.36, 0.01, 3.44}, 0.0, 5.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnD", "ceramic", {1.02, 0.01, 3.50}, 0.0, 5.6);

    place("GEO_WhiteRook_02.obj", "VisibleWhiteRook", "ceramic", {-0.86, 0.01, 2.92}, 0.0, 5.8);
    place("GEO_WhiteKnight_02.obj", "CameraOnlyWhiteKnight", "ceramic", {-0.48, 0.01, 2.98}, -0.25, 5.8);
    place("GEO_WhiteBishop_02.obj", "ReflectionOnlyWhiteBishop", "ceramic", {-0.48, 0.01, 2.98}, 0.0, 6.0);
    place("GEO_WhiteBishop_02.obj", "VisibleWhiteBishop", "ceramic", {-0.14, 0.01, 3.02}, 0.0, 6.0);
    place("GEO_WhiteKing.obj", "VisibleWhiteKing", "ceramic", {0.22, 0.01, 3.00}, 0.0, 6.7);
    place("GEO_WhiteQueen.obj", "CameraOnlyWhiteQueen", "ceramic", {0.72, 0.01, 2.92}, 0.0, 6.1);
    place("GEO_WhiteKing.obj", "ReflectionOnlyWhiteKing", "ceramic", {0.72, 0.01, 2.92}, 0.0, 6.4);
}

} // namespace scene_builder
