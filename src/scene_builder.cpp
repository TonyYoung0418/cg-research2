// External asset note for submission:
// - 57-estancia_comedor_obj/room.obj is a borrowed source room mesh used only
//   as input furniture geometry during scene generation.
// - Free_Stuff_1_-__Chess_Set/OBJ/*.obj are TurboSquid chess meshes used as
//   imported scene assets.
// - Mirror/Mirror_Frame.obj and Mirror/Mirror_Surface.obj are user-authored
//   mirror meshes exported from Maya.
// - If any AI tools or additional borrowed code contributed to this file,
//   declare them here before submission as required by the assignment.

#include "scene_builder.hpp"

#include <algorithm>
#include <cctype>
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

std::string lowerCopy(std::string s) {
    for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

bool startsWith(const std::string &text, const std::string &prefix) {
    return text.rfind(prefix, 0) == 0;
}

std::string safeObjectName(const std::string &name) {
    std::string out;
    out.reserve(name.size());
    for (unsigned char c : name) {
        if (std::isalnum(c)) out.push_back(static_cast<char>(c));
        else out.push_back('_');
    }
    return out.empty() ? "Object" : out;
}

std::string baseName(const std::string &path) {
    size_t pos = path.find_last_of("/\\");
    return pos == std::string::npos ? path : path.substr(pos + 1);
}

bool isRoomFurnitureObject(const std::string &name) {
    std::string n = lowerCopy(name);
    if (n.empty()) return false;

    // Keep the borrowed room as furniture only: no walls, floor, ceiling,
    // windows/window frames, or the large outdoor/window backdrop plane.
    if (n.find("muro") != std::string::npos) return false;
    if (n == "piso" || n == "techo" || n == "plane001") return false;
    if (n.find("ventana") != std::string::npos) return false;
    if (n.find("carp") != std::string::npos) return false;
    if (startsWith(n, "rama")) return false;
    if (startsWith(n, "shelf")) return false;
    if (n.find("picture") != std::string::npos) return false;
    if (startsWith(n, "book")) return false;
    if (startsWith(n, "decor")) return false;
    if (startsWith(n, "vase")) return false;
    if (startsWith(n, "leaf")) return false;
    if (startsWith(n, "succulent")) return false;
    if (n.find("asket") != std::string::npos) return false;
    if (n.find("carafe") != std::string::npos || n.find("goods_pure_carafe_set") != std::string::npos) return false;

    return true;
}

bool isSofaObject(int serial) {
    return serial >= 24 && serial <= 34;
}

bool isSmallCabinetObject(int serial) {
    return serial == 35 || serial == 36;
}

bool isLoungeChairObject(int serial) {
    return serial >= 187 && serial <= 197;
}

bool isCoffeeTableObject(int serial) {
    return (serial >= 70 && serial <= 75) || (serial >= 200 && serial <= 229) || (serial >= 231 && serial <= 237);
}

bool isSofaBaseObject(int serial) {
    return serial == 24;
}

bool isSofaSeatCushionObject(int serial) {
    return serial == 25;
}

bool isSofaCushionObject(int serial) {
    return serial >= 26 && serial <= 30;
}

bool isSofaPillowObject(int serial) {
    return serial >= 31 && serial <= 34;
}

bool isCoffeeTableApronObject(int serial) {
    return serial == 70;
}

bool isCoffeeTableTopObject(int serial) {
    return serial == 73;
}

bool isCoffeeTableLegObject(int serial) {
    return serial == 71 || serial == 72 || serial == 74 || serial == 75;
}

bool isTabletopAccessoryObject(int serial) {
    return (serial >= 200 && serial <= 229) || (serial >= 231 && serial <= 237);
}

bool isTabletopVesselObject(int serial) {
    return serial == 200 || serial == 201 || serial == 203 || serial == 231 ||
           serial == 235 || serial == 236 || serial == 237;
}

bool isTabletopWhiteCupObject(int serial) {
    return serial == 231 || serial == 232;
}

bool isTabletopGoldCupObject(int serial) {
    return serial == 235 || serial == 236 || serial == 237;
}

bool isTabletopStemObject(int serial) {
    return serial == 202 || (serial >= 204 && serial <= 210) || (serial >= 219 && serial <= 229);
}

bool isTabletopLeafObject(int serial) {
    return serial >= 211 && serial <= 218;
}

bool isTabletopBookObject(int serial) {
    return serial == 233 || serial == 234;
}

bool isLoungeChairFrameObject(int serial) {
    return serial == 187 || serial == 188 || serial == 189 || serial == 190 ||
           serial == 192 || serial == 195;
}

bool isLoungeChairCushionObject(int serial) {
    return serial == 191 || serial == 193 || serial == 194 || serial == 196 || serial == 197;
}

bool isRemovedDiningSetObject(int serial) {
    if (serial >= 100 && serial <= 186) return true;
    if (serial >= 76 && serial <= 96) return true;
    return false;
}

struct RoomImportPolicy {
    bool include = false;
    std::string visibilityPrefix;
    Vec3 offset;
};

constexpr double kSofaGroundOffsetY = -0.00231;
constexpr double kCoffeeTableGroundOffsetY = -0.02011;
constexpr double kLoungeChairGroundOffsetY = -0.02205;

RoomImportPolicy roomImportPolicy(int serial, const std::string &name, bool reflectionLoungeCopy) {
    if (!isRoomFurnitureObject(name)) return {};

    if (reflectionLoungeCopy) {
        if (!isLoungeChairObject(serial)) return {};
        return {true, "ReflectionOnly", {-1.35, kLoungeChairGroundOffsetY, 0.06}};
    }

    if (isSmallCabinetObject(serial) || isRemovedDiningSetObject(serial)) return {};
    if (isSofaObject(serial)) {
        double yOffset = kSofaGroundOffsetY;
        if (serial == 24) yOffset -= 0.02441;
        if (serial == 26 || serial == 27) yOffset -= 0.02141;
        return {true, "CameraOnly", {0.25, yOffset, 0.0}};
    }
    if (isCoffeeTableObject(serial)) return {true, "", {0.85, kCoffeeTableGroundOffsetY, 0.0}};
    if (isLoungeChairObject(serial)) return {true, "", {1.25, kLoungeChairGroundOffsetY, 0.0}};
    return {true, "", {0.0, 0.0, 0.0}};
}

std::string roomFurnitureMaterial(int serial, const std::string &name) {
    std::string n = lowerCopy(name);
    if (isSofaBaseObject(serial)) return "sofa_body";
    if (isSofaSeatCushionObject(serial)) return "sofa_pillow";
    if (isSofaCushionObject(serial)) return "sofa_cushion";
    if (isSofaPillowObject(serial)) return "sofa_pillow";
    if (isLoungeChairFrameObject(serial)) return "chair_frame";
    if (isLoungeChairCushionObject(serial)) return "chair_cushion";
    if (isCoffeeTableTopObject(serial)) return "table_top";
    if (isCoffeeTableApronObject(serial) || isCoffeeTableLegObject(serial)) return "table_base";
    if (isTabletopLeafObject(serial)) return "leaf";
    if (isTabletopStemObject(serial)) return "tabletop_reed";
    if (isTabletopWhiteCupObject(serial)) return "tabletop_cup_white";
    if (isTabletopGoldCupObject(serial)) return "tabletop_cup_gold";
    if (isTabletopVesselObject(serial)) return "tabletop_vessel";
    if (isTabletopBookObject(serial)) return "tabletop_book";
    if (isTabletopAccessoryObject(serial)) return "tabletop_light";
    if (startsWith(n, "book")) return "book_red";
    if (n.find("shelf") != std::string::npos || n.find("basket") != std::string::npos) return "old_wood_dark";
    if (n.find("carpet") != std::string::npos) return "rug";
    if (n.find("vase") != std::string::npos || n.find("decor") != std::string::npos ||
        n.find("picture") != std::string::npos) {
        return "ceramic";
    }
    if (n.find("succulent") != std::string::npos || startsWith(n, "leaf")) return "leaf";
    if (n.find("cylinder") != std::string::npos || n.find("screw") != std::string::npos) return "metal";
    return "old_wood";
}

int parseObjIndex(const std::string &tok, size_t count) {
    size_t slash = tok.find('/');
    int id = std::stoi(slash == std::string::npos ? tok : tok.substr(0, slash));
    return id < 0 ? int(count) + id : id;
}

int parseObjNormalIndex(const std::string &tok, size_t count) {
    size_t slash = tok.find('/');
    if (slash == std::string::npos) return 0;
    size_t slash2 = tok.find('/', slash + 1);
    if (slash2 == std::string::npos || slash2 + 1 >= tok.size()) return 0;
    int id = std::stoi(tok.substr(slash2 + 1));
    return id < 0 ? int(count) + id : id;
}

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

    void roomFurniturePass(const std::string &path, bool reflectionLoungeCopy) {
        std::ifstream first(path);
        if (!first) {
            std::cerr << "Could not open room asset " << path << "\n";
            std::exit(1);
        }

        std::vector<Vec3> verts(1);
        std::vector<Vec3> norms(1);
        std::vector<unsigned char> usedVerts(1, 0);
        std::vector<unsigned char> usedNorms(1, 0);
        std::string currentObject;
        std::string line;
        int objectSerial = 0;

        while (std::getline(first, line)) {
            std::istringstream ss(line);
            std::string tag;
            ss >> tag;
            if (tag == "v") {
                Vec3 p;
                ss >> p.x >> p.y >> p.z;
                verts.push_back(p);
                usedVerts.push_back(0);
            } else if (tag == "vn") {
                Vec3 n;
                ss >> n.x >> n.y >> n.z;
                norms.push_back(n);
                usedNorms.push_back(0);
            } else if (tag == "o") {
                ss >> currentObject;
                ++objectSerial;
            } else if (tag == "f" && roomImportPolicy(objectSerial, currentObject, reflectionLoungeCopy).include) {
                std::string tok;
                while (ss >> tok) {
                    int vi = parseObjIndex(tok, verts.size());
                    int ni = parseObjNormalIndex(tok, norms.size());
                    if (vi > 0 && vi < int(usedVerts.size())) usedVerts[size_t(vi)] = 1;
                    if (ni > 0 && ni < int(usedNorms.size())) usedNorms[size_t(ni)] = 1;
                }
            }
        }

        constexpr double s = 0.008;
        auto transform = [&](Vec3 p, const Vec3 &offset) {
            return Vec3{
                0.25 + (p.x - 696.0) * s + offset.x,
                0.02 + p.y * s + offset.y,
                4.35 + (p.z + 796.0) * s + offset.z
            };
        };

        std::vector<int> remap(verts.size(), 0);
        std::vector<int> normalRemap(norms.size(), 0);
        for (size_t i = 1; i < norms.size(); ++i) {
            if (usedNorms[i]) normalRemap[i] = normal(norms[i]);
        }

        std::ifstream second(path);
        if (!second) {
            std::cerr << "Could not reopen room asset " << path << "\n";
            std::exit(1);
        }

        currentObject.clear();
        std::string emittedObject;
        objectSerial = 0;
        while (std::getline(second, line)) {
            std::istringstream ss(line);
            std::string tag;
            ss >> tag;
            if (tag == "o") {
                ss >> currentObject;
                emittedObject.clear();
                ++objectSerial;
            } else if (tag == "f") {
                RoomImportPolicy policy = roomImportPolicy(objectSerial, currentObject, reflectionLoungeCopy);
                if (!policy.include) continue;
                if (emittedObject != currentObject) {
                    obj << "o " << policy.visibilityPrefix << "ImportedRoomFurniture_" << objectSerial
                        << "_" << safeObjectName(currentObject)
                        << "\nusemtl " << roomFurnitureMaterial(objectSerial, currentObject) << "\n";
                    emittedObject = currentObject;
                }
                std::vector<int> ids;
                std::vector<int> nids;
                std::string tok;
                while (ss >> tok) {
                    ids.push_back(parseObjIndex(tok, verts.size()));
                    nids.push_back(parseObjNormalIndex(tok, norms.size()));
                }
                for (size_t i = 1; i + 1 < ids.size(); ++i) {
                    if (remap[size_t(ids[0])] == 0) remap[size_t(ids[0])] = vertex(transform(verts[size_t(ids[0])], policy.offset));
                    if (remap[size_t(ids[i])] == 0) remap[size_t(ids[i])] = vertex(transform(verts[size_t(ids[i])], policy.offset));
                    if (remap[size_t(ids[i + 1])] == 0) remap[size_t(ids[i + 1])] = vertex(transform(verts[size_t(ids[i + 1])], policy.offset));
                    int a = remap[size_t(ids[0])];
                    int b = remap[size_t(ids[i])];
                    int c = remap[size_t(ids[i + 1])];
                    int na = nids[0] > 0 ? normalRemap[size_t(nids[0])] : 0;
                    int nb = nids[i] > 0 ? normalRemap[size_t(nids[i])] : 0;
                    int nc = nids[i + 1] > 0 ? normalRemap[size_t(nids[i + 1])] : 0;
                    if (a <= 0 || b <= 0 || c <= 0) continue;
                    if (na > 0 && nb > 0 && nc > 0) {
                        obj << "f " << a << "//" << na << " " << b << "//" << nb << " " << c << "//" << nc << "\n";
                    } else {
                        obj << "f " << a << " " << b << " " << c << "\n";
                    }
                }
            }
        }
    }

    void roomFurniture(const std::string &path) {
        roomFurniturePass(path, false);
        roomFurniturePass(path, true);
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
    mat("table_top", {0.46, 0.27, 0.12});
    mat("table_base", {0.22, 0.12, 0.055});
    mat("tabletop_light", {0.72, 0.56, 0.34});
    mat("tabletop_vessel", {0.80, 0.75, 0.68});
    mat("tabletop_reed", {0.26, 0.17, 0.09});
    mat("tabletop_cup_white", {0.91, 0.89, 0.85});
    mat("tabletop_cup_gold", {0.62, 0.42, 0.18});
    mat("tabletop_book", {0.97, 0.97, 0.97});
    mat("wood_light", {0.63, 0.33, 0.15});
    mat("frame", {0.62, 0.42, 0.18});
    mat("brass", {0.80, 0.57, 0.22});
    mat("sofa_body", {0.46, 0.29, 0.16});
    mat("sofa_cushion", {0.62, 0.45, 0.28});
    mat("sofa_pillow", {0.67, 0.58, 0.45});
    mat("chair_frame", {0.57, 0.36, 0.18});
    mat("chair_cushion", {0.63, 0.51, 0.39});
    mat("ceramic", {0.82, 0.76, 0.64});
    mat("leaf", {0.10, 0.35, 0.16});
    mat("book_red", {0.45, 0.055, 0.040});
    mat("lamp_shade", {0.88, 0.77, 0.56});
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
    mat("white_piece", {0.78, 0.75, 0.66});
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
    w.obj << "mtllib " << baseName(opt.mtlPath) << "\n";

    w.quad("Floor", "floor", {-5.8, 0.0, -3.35}, {5.8, 0.0, -3.35}, {5.8, 0.0, 7.2}, {-5.8, 0.0, 7.2});
    w.quad("BackWall", "wall", {5.8, 0.0, -3.35}, {-5.8, 0.0, -3.35}, {-5.8, 3.5, -3.35}, {5.8, 3.5, -3.35});
    w.quad("LeftWall", "dark_wall", {-5.8, 0.0, 7.2}, {-5.8, 0.0, -3.35}, {-5.8, 3.5, -3.35}, {-5.8, 3.5, 7.2});
    w.quad("RightWall", "wall", {5.8, 0.0, -3.35}, {5.8, 0.0, 7.2}, {5.8, 3.5, 7.2}, {5.8, 3.5, -3.35});
    w.quad("FrontWall", "wall", {-5.8, 0.0, 7.2}, {5.8, 0.0, 7.2}, {5.8, 3.5, 7.2}, {-5.8, 3.5, 7.2});
    w.quad("Ceiling", "plaster_light", {-5.8, 3.5, 7.2}, {5.8, 3.5, 7.2}, {5.8, 3.5, -3.35}, {-5.8, 3.5, -3.35});

    w.box("BackBaseboard", "trim", {-5.70, 0.02, -3.24}, {5.70, 0.16, -3.08});
    w.box("FrontBaseboard", "trim", {-5.70, 0.02, 7.04}, {5.70, 0.16, 7.18});
    w.box("RightBaseboard", "trim", {5.62, 0.02, -3.10}, {5.78, 0.16, 7.00});
    w.box("LeftBaseboard", "old_wood_dark", {-5.78, 0.02, -3.10}, {-5.62, 0.16, 7.00});
    w.box("BackCrownMoulding", "trim", {-5.70, 3.26, -3.24}, {5.70, 3.44, -3.08});
    w.box("FrontCrownMoulding", "trim", {-5.70, 3.26, 7.02}, {5.70, 3.44, 7.18});

    w.box("DoorFrameLeft", "old_wood_dark", {3.10, 0.00, -3.06}, {3.30, 2.88, -2.76});
    w.box("DoorFrameRight", "old_wood_dark", {4.42, 0.00, -3.06}, {4.62, 2.88, -2.76});
    w.box("DoorFrameTop", "old_wood_dark", {3.10, 2.70, -3.06}, {4.62, 2.88, -2.76});
    w.box("OldDoorPanel", "old_wood", {3.32, 0.00, -3.00}, {4.40, 2.66, -2.82});
    w.box("DoorInsetUpper", "old_wood_light", {3.50, 1.55, -2.80}, {4.22, 2.42, -2.70});
    w.box("DoorInsetLower", "old_wood_light", {3.50, 0.32, -2.80}, {4.22, 1.20, -2.70});
    w.box("DoorCenterRail", "old_wood_dark", {3.84, 0.06, -2.72}, {3.95, 2.66, -2.60});
    w.ellipseBand("DoorHandleRosetteRing", "brass", {4.235, 1.28, 0.0}, 0.022, 0.022, 0.052, 0.052, -2.66, 18);
    w.disk("DoorHandleRosetteCore", "brass", {4.235, 1.28, 0.0}, 0.022, 0.022, -2.655, 18);
    w.box("DoorHandleStem", "brass", {4.205, 1.255, -2.655}, {4.255, 1.305, -2.595});
    w.box("DoorHandleLeverMain", "brass", {4.02, 1.264, -2.628}, {4.205, 1.296, -2.582});
    w.box("DoorHandleLeverTip", "brass", {3.95, 1.268, -2.622}, {4.02, 1.292, -2.588});

    w.box("MantelTop", "old_wood_dark", {-4.95, 1.18, -3.06}, {-2.08, 1.36, -2.68});
    w.box("MantelShelf", "old_wood", {-5.16, 1.34, -2.98}, {-1.86, 1.48, -2.56});
    w.box("MantelLeftPost", "old_wood_dark", {-4.92, 0.00, -3.00}, {-4.64, 1.30, -2.62});
    w.box("MantelRightPost", "old_wood_dark", {-2.38, 0.00, -3.00}, {-2.10, 1.30, -2.62});
    // Import the user-authored mirror model from Maya while keeping the same
    // frame and mirror materials used by the renderer.
    const Vec3 mirrorModelOffset{0.0, 0.86, 1.125};
    constexpr double mirrorModelScale = 0.01;
    w.model("Mirror/Mirror_Frame.obj", "CustomMirrorFrame", "frame", mirrorModelOffset, mirrorModelScale);
    w.model("Mirror/Mirror_Surface.obj", "CustomMirrorSurface", "mirror", mirrorModelOffset, mirrorModelScale);

    double ls = opt.lightSize;
    Vec3 lp{opt.lightPos[0], opt.lightPos[1], opt.lightPos[2]};
    w.quad("MainLight", "light_panel", {lp.x - ls, lp.y, lp.z - 0.35 * ls}, {lp.x + ls, lp.y, lp.z - 0.35 * ls},
           {lp.x + ls, lp.y, lp.z + 0.35 * ls}, {lp.x - ls, lp.y, lp.z + 0.35 * ls});
    if (opt.extraLight) {
        double fs = ls * 0.8;
        Vec3 fp{lp.x, lp.y, -1.85};
        w.quad("FillLight", "light_fill", {fp.x - fs, fp.y, fp.z - 0.30 * fs}, {fp.x + fs, fp.y, fp.z - 0.30 * fs},
               {fp.x + fs, fp.y, fp.z + 0.30 * fs}, {fp.x - fs, fp.y, fp.z + 0.30 * fs});
    }

    w.roomFurniture("57-estancia_comedor_obj/room.obj");

    const std::string assetDir = "Free_Stuff_1_-__Chess_Set/OBJ/";
    auto place = [&](const std::string &file, const std::string &name, const std::string &mat,
                     Vec3 p, double angle = 0.0, double scale = 7.1) {
        w.model(assetDir + file, name, mat, p, scale, angle);
    };

    place("GEO_WhitePawn_08.obj", "VisiblePawnA", "ceramic", {-1.02, -0.0006, 3.50}, 0.0, 5.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnB", "ceramic", {-0.36, -0.0006, 3.44}, 0.0, 5.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnC", "ceramic", {0.36, -0.0006, 3.44}, 0.0, 5.6);
    place("GEO_WhitePawn_08.obj", "VisiblePawnD", "ceramic", {1.02, -0.0006, 3.50}, 0.0, 5.6);

    place("GEO_WhiteRook_02.obj", "VisibleWhiteRook", "ceramic", {-0.86, 0.0, 2.92}, 0.0, 5.8);
    place("GEO_WhiteKnight_02.obj", "CameraOnlyWhiteKnight", "ceramic", {-0.48, 0.00234, 2.98}, -0.25, 5.8);
    place("GEO_WhiteBishop_02.obj", "ReflectionOnlyWhiteBishop", "ceramic", {-0.48, 0.0, 2.98}, 0.0, 6.0);
    place("GEO_WhiteBishop_02.obj", "VisibleWhiteBishop", "ceramic", {-0.14, 0.0, 3.02}, 0.0, 6.0);
    place("GEO_WhiteKing.obj", "VisibleWhiteKing", "ceramic", {0.22, -0.00009, 3.00}, 0.0, 6.7);
    place("GEO_WhiteQueen.obj", "CameraOnlyWhiteQueen", "ceramic", {0.72, 0.0, 2.92}, 0.0, 6.1);
    place("GEO_WhiteKing.obj", "ReflectionOnlyWhiteKing", "ceramic", {0.72, -0.00009, 2.92}, 0.0, 6.4);
}

} // namespace scene_builder
