// External resource note for submission:
// - textures/old_wood_floor.ppm is derived from Poly Haven's "Old Wood Floor"
//   CC0 texture and is sampled by the renderer below.
// - If any AI tools or additional borrowed code contributed to this file,
//   declare them here before submission as required by the assignment.

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <tuple>
#include <vector>

#include "scene_builder.hpp"

constexpr double kPi = 3.14159265358979323846;
constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr int kPrimaryRay = 0;
constexpr int kMirrorRay = 1;
constexpr int kVisibleToAll = 0;
constexpr int kCameraOnly = 1;
constexpr int kReflectionOnly = 2;

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;

    Vec3() = default;
    Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3 &operator+=(const Vec3 &v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3 &operator*=(double s) { x *= s; y *= s; z *= s; return *this; }
    Vec3 &operator/=(double s) { return *this *= 1.0 / s; }
    double operator[](int i) const { return i == 0 ? x : (i == 1 ? y : z); }
};

Vec3 operator+(Vec3 a, const Vec3 &b) { return a += b; }
Vec3 operator-(const Vec3 &a, const Vec3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(const Vec3 &a, const Vec3 &b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }
Vec3 operator*(Vec3 a, double s) { return a *= s; }
Vec3 operator*(double s, Vec3 a) { return a *= s; }
Vec3 operator/(Vec3 a, double s) { return a /= s; }

double dot(const Vec3 &a, const Vec3 &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 cross(const Vec3 &a, const Vec3 &b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double length(const Vec3 &v) { return std::sqrt(dot(v, v)); }
Vec3 normalize(Vec3 v) { double len = length(v); return len > 0.0 ? v / len : Vec3{}; }
Vec3 clamp01(const Vec3 &v) {
    return {std::clamp(v.x, 0.0, 1.0), std::clamp(v.y, 0.0, 1.0), std::clamp(v.z, 0.0, 1.0)};
}
Vec3 minVec(const Vec3 &a, const Vec3 &b) {
    return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
Vec3 maxVec(const Vec3 &a, const Vec3 &b) {
    return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}

struct Ray {
    Vec3 origin;
    Vec3 direction;
    int visibility = kPrimaryRay;
    Vec3 at(double t) const { return origin + t * direction; }
};

struct Rng {
    std::mt19937_64 gen;
    std::uniform_real_distribution<double> dist{0.0, 1.0};
    explicit Rng(uint64_t seed) : gen(seed) {}
    double next() { return dist(gen); }
};

Vec3 randomInUnitSphere(Rng &rng) {
    while (true) {
        Vec3 p{2.0 * rng.next() - 1.0, 2.0 * rng.next() - 1.0, 2.0 * rng.next() - 1.0};
        if (dot(p, p) < 1.0) return p;
    }
}

Vec3 cosineHemisphere(const Vec3 &n, Rng &rng) {
    double r1 = rng.next();
    double r2 = rng.next();
    double phi = 2.0 * kPi * r1;
    double r = std::sqrt(r2);
    double x = r * std::cos(phi);
    double z = r * std::sin(phi);
    double y = std::sqrt(std::max(0.0, 1.0 - r2));
    Vec3 w = normalize(n);
    Vec3 a = std::fabs(w.x) > 0.9 ? Vec3{0, 1, 0} : Vec3{1, 0, 0};
    Vec3 v = normalize(cross(w, a));
    Vec3 u = cross(v, w);
    return normalize(x * u + y * w + z * v);
}

Vec3 reflect(const Vec3 &v, const Vec3 &n) { return v - 2.0 * dot(v, n) * n; }

bool refract(const Vec3 &uv, const Vec3 &n, double eta, Vec3 &refracted) {
    double cosTheta = std::min(dot(-uv, n), 1.0);
    Vec3 rOutPerp = eta * (uv + cosTheta * n);
    double k = 1.0 - dot(rOutPerp, rOutPerp);
    if (k < 0.0) return false;
    Vec3 rOutParallel = -std::sqrt(k) * n;
    refracted = rOutPerp + rOutParallel;
    return true;
}

double schlick(double cosine, double refIdx) {
    double r0 = (1.0 - refIdx) / (1.0 + refIdx);
    r0 *= r0;
    return r0 + (1.0 - r0) * std::pow(1.0 - cosine, 5.0);
}

struct Material {
    std::string name;
    Vec3 albedo{0.8, 0.8, 0.8};
    Vec3 emission{0, 0, 0};
    double roughness = 0.0;
    double ior = 1.5;
    int type = 0; // 0 diffuse, 1 metal, 2 glass, 3 emissive
    bool sampleLight = false;
};

struct Texture {
    int width = 0;
    int height = 0;
    std::vector<Vec3> pixels;
};

bool hasTexture(const Texture &tex) {
    return tex.width > 0 && tex.height > 0 && !tex.pixels.empty();
}

bool fileExists(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    return static_cast<bool>(in);
}

double srgbToLinear(double v) {
    v = std::clamp(v, 0.0, 1.0);
    return std::pow(v, 2.2);
}

Texture loadPpmTexture(const std::string &path) {
    Texture tex;
    std::ifstream in(path, std::ios::binary);
    if (!in) return tex;

    std::string magic;
    int maxValue = 0;
    in >> magic >> tex.width >> tex.height >> maxValue;
    if (magic != "P6" || tex.width <= 0 || tex.height <= 0 || maxValue != 255) return tex;
    in.get();

    tex.pixels.resize(size_t(tex.width) * tex.height);
    for (int i = 0; i < tex.width * tex.height; ++i) {
        unsigned char rgb[3];
        in.read(reinterpret_cast<char *>(rgb), 3);
        if (!in) {
            tex.pixels.clear();
            tex.width = tex.height = 0;
            return tex;
        }
        tex.pixels[size_t(i)] = {
            srgbToLinear(rgb[0] / 255.0),
            srgbToLinear(rgb[1] / 255.0),
            srgbToLinear(rgb[2] / 255.0)
        };
    }
    return tex;
}

int wrapIndex(int value, int size) {
    value %= size;
    if (value < 0) value += size;
    return value;
}

Vec3 sampleTextureRepeat(const Texture &tex, double u, double v) {
    if (!hasTexture(tex)) return {1, 1, 1};
    u -= std::floor(u);
    v -= std::floor(v);
    double x = u * tex.width - 0.5;
    double y = (1.0 - v) * tex.height - 0.5;
    int x0 = int(std::floor(x));
    int y0 = int(std::floor(y));
    double tx = x - x0;
    double ty = y - y0;
    int x1 = x0 + 1;
    int y1 = y0 + 1;
    const Vec3 &c00 = tex.pixels[size_t(wrapIndex(y0, tex.height)) * tex.width + size_t(wrapIndex(x0, tex.width))];
    const Vec3 &c10 = tex.pixels[size_t(wrapIndex(y0, tex.height)) * tex.width + size_t(wrapIndex(x1, tex.width))];
    const Vec3 &c01 = tex.pixels[size_t(wrapIndex(y1, tex.height)) * tex.width + size_t(wrapIndex(x0, tex.width))];
    const Vec3 &c11 = tex.pixels[size_t(wrapIndex(y1, tex.height)) * tex.width + size_t(wrapIndex(x1, tex.width))];
    Vec3 a = (1.0 - tx) * c00 + tx * c10;
    Vec3 b = (1.0 - tx) * c01 + tx * c11;
    return (1.0 - ty) * a + ty * b;
}

struct Triangle {
    Vec3 v0, v1, v2;
    Vec3 n0, n1, n2;
    Vec3 normal;
    Vec3 centroid;
    Vec3 bmin, bmax;
    double area = 0.0;
    int vi0 = 0;
    int vi1 = 0;
    int vi2 = 0;
    int material = 0;
    int visibility = kVisibleToAll;
    bool hasVertexNormals = false;
    std::string object;
};

struct Aabb {
    Vec3 mn{kInf, kInf, kInf};
    Vec3 mx{-kInf, -kInf, -kInf};

    void expand(const Vec3 &p) { mn = minVec(mn, p); mx = maxVec(mx, p); }
    void expand(const Aabb &b) { expand(b.mn); expand(b.mx); }

    bool hit(const Ray &r, double tMin, double tMax) const {
        for (int a = 0; a < 3; ++a) {
            double invD = 1.0 / r.direction[a];
            double t0 = (mn[a] - r.origin[a]) * invD;
            double t1 = (mx[a] - r.origin[a]) * invD;
            if (invD < 0.0) std::swap(t0, t1);
            tMin = t0 > tMin ? t0 : tMin;
            tMax = t1 < tMax ? t1 : tMax;
            if (tMax <= tMin) return false;
        }
        return true;
    }
};

struct BvhNode {
    Aabb box;
    int left = -1;
    int right = -1;
    int start = 0;
    int count = 0;
};

struct Hit {
    double t = kInf;
    Vec3 p;
    Vec3 normal;
    int material = 0;
    bool frontFace = true;
};

struct Options {
    int width = 800;
    int height = 600;
    int spp = 64;
    int maxDepth = 8;
    int threads = std::max(1u, std::thread::hardware_concurrency());
    bool shadows = true;
    bool extraLight = true;
    std::string output = "renders/view_main.ppm";
    std::string objPath = "scenes/generated_scene.obj";
    std::string mtlPath = "scenes/generated_scene.mtl";
    std::string preset = "main";
    std::string environment = "studio";
    Vec3 camera{-0.25, 0.96, 5.90};
    Vec3 lookAt{-0.05, 0.50, 2.35};
    Vec3 lightPos{0.00, 3.45, 2.40};
    Vec3 objectOffset{0.0, 0.0, 0.0};
    Vec3 floorColor{0.43, 0.27, 0.16};
    double objectRotateY = 0.0;
    double fov = 52.0;
    double lightSize = 1.60;
    double metalRoughness = 0.12;
};

double parseDouble(const char *s) {
    return std::stod(std::string(s));
}

void applyPreset(Options &opt) {
    if (opt.preset == "side") {
        opt.camera = {-2.85, 0.90, 5.35};
        opt.lookAt = {-0.12, 0.48, 2.45};
        opt.fov = 52.0;
    } else if (opt.preset == "wide") {
        opt.camera = {-0.10, 1.06, 6.45};
        opt.lookAt = {-0.05, 0.48, 2.20};
        opt.fov = 60.0;
    } else {
        opt.camera = {-0.10, 1.06, 6.45};
        opt.lookAt = {-0.05, 0.48, 2.20};
        opt.fov = 60.0;
    }
}

Options parseArgs(int argc, char **argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--preset" && i + 1 < argc) opt.preset = argv[++i];
    }
    applyPreset(opt);
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](int n) {
            if (i + n >= argc) {
                std::cerr << "Missing value for " << a << "\n";
                std::exit(2);
            }
        };
        if (a == "--preset") { ++i; }
        else if (a == "--width") { need(1); opt.width = std::stoi(argv[++i]); }
        else if (a == "--height") { need(1); opt.height = std::stoi(argv[++i]); }
        else if (a == "--spp") { need(1); opt.spp = std::stoi(argv[++i]); }
        else if (a == "--max-depth") { need(1); opt.maxDepth = std::stoi(argv[++i]); }
        else if (a == "--threads") { need(1); opt.threads = std::max(1, std::stoi(argv[++i])); }
        else if (a == "--output") { need(1); opt.output = argv[++i]; }
        else if (a == "--environment") { need(1); opt.environment = argv[++i]; }
        else if (a == "--shadows") { need(1); opt.shadows = std::stoi(argv[++i]) != 0; }
        else if (a == "--extra-light") { need(1); opt.extraLight = std::stoi(argv[++i]) != 0; }
        else if (a == "--camera") {
            need(3);
            double x = parseDouble(argv[++i]), y = parseDouble(argv[++i]), z = parseDouble(argv[++i]);
            opt.camera = {x, y, z};
        }
        else if (a == "--look-at") {
            need(3);
            double x = parseDouble(argv[++i]), y = parseDouble(argv[++i]), z = parseDouble(argv[++i]);
            opt.lookAt = {x, y, z};
        }
        else if (a == "--light-pos") {
            need(3);
            double x = parseDouble(argv[++i]), y = parseDouble(argv[++i]), z = parseDouble(argv[++i]);
            opt.lightPos = {x, y, z};
        }
        else if (a == "--object-offset") {
            need(3);
            double x = parseDouble(argv[++i]), y = parseDouble(argv[++i]), z = parseDouble(argv[++i]);
            opt.objectOffset = {x, y, z};
        }
        else if (a == "--floor-color") {
            need(3);
            double x = parseDouble(argv[++i]), y = parseDouble(argv[++i]), z = parseDouble(argv[++i]);
            opt.floorColor = {x, y, z};
        }
        else if (a == "--object-rotate-y") { need(1); opt.objectRotateY = parseDouble(argv[++i]); }
        else if (a == "--fov") { need(1); opt.fov = parseDouble(argv[++i]); }
        else if (a == "--light-size") { need(1); opt.lightSize = parseDouble(argv[++i]); }
        else if (a == "--metal-roughness") { need(1); opt.metalRoughness = parseDouble(argv[++i]); }
        else if (a == "--help") {
            std::cout << "MirrorReflectionRenderer options are documented in README.md\n";
            std::exit(0);
        } else {
            std::cerr << "Unknown argument: " << a << "\n";
            std::exit(2);
        }
    }
    return opt;
}

std::vector<Material> makeMaterials(const Options &opt, std::map<std::string, int> &ids) {
    std::vector<Material> m;
    auto add = [&](Material mat) {
        ids[mat.name] = int(m.size());
        m.push_back(mat);
    };
    add({"floor", opt.floorColor, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"floor_sheen", {0.62, 0.50, 0.38}, {0, 0, 0}, 0.42, 1.5, 1, false});
    add({"wall", {0.55, 0.36, 0.24}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"dark_wall", {0.20, 0.12, 0.075}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"trim", {0.78, 0.68, 0.52}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"curtain", {0.36, 0.045, 0.040}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"curtain_dark", {0.16, 0.025, 0.025}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"rug", {0.26, 0.14, 0.075}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"rug_border", {0.70, 0.50, 0.28}, {0, 0, 0}, 0.1, 1.5, 1, false});
    add({"table_top", {0.46, 0.27, 0.12}, {0, 0, 0}, 0.05, 1.5, 0, false});
    add({"table_base", {0.22, 0.12, 0.055}, {0, 0, 0}, 0.02, 1.5, 0, false});
    add({"tabletop_light", {0.72, 0.56, 0.34}, {0, 0, 0}, 0.04, 1.5, 0, false});
    add({"tabletop_vessel", {0.80, 0.75, 0.68}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"tabletop_reed", {0.26, 0.17, 0.09}, {0, 0, 0}, 0.02, 1.5, 0, false});
    add({"tabletop_cup_white", {0.91, 0.89, 0.85}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"tabletop_cup_gold", {0.62, 0.42, 0.18}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"tabletop_book", {0.97, 0.97, 0.97}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"wood_light", {0.63, 0.33, 0.15}, {0, 0, 0}, 0.02, 1.5, 0, false});
    add({"frame", {0.62, 0.42, 0.18}, {0, 0, 0}, 0.028, 1.5, 1, false});
    add({"brass", {0.80, 0.57, 0.22}, {0, 0, 0}, 0.12, 1.5, 1, false});
    add({"sofa_body", {0.46, 0.29, 0.16}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"sofa_cushion", {0.62, 0.45, 0.28}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"sofa_pillow", {0.67, 0.58, 0.45}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"chair_frame", {0.57, 0.36, 0.18}, {0, 0, 0}, 0.02, 1.5, 0, false});
    add({"chair_cushion", {0.63, 0.51, 0.39}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"ceramic", {0.82, 0.76, 0.64}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"leaf", {0.10, 0.35, 0.16}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"book_red", {0.45, 0.055, 0.040}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"lamp_shade", {0.88, 0.77, 0.56}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"window_glow", {1.0, 0.72, 0.20}, {1.25, 0.72, 0.20}, 0, 1.5, 0, false});
    add({"stone", {0.30, 0.31, 0.29}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"bark", {0.22, 0.10, 0.035}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"blossom", {0.94, 0.88, 0.78}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"painting_blue", {0.15, 0.48, 0.52}, {0.025, 0.075, 0.08}, 0, 1.5, 0, false});
    add({"painting_yellow", {0.72, 0.53, 0.14}, {0.10, 0.07, 0.015}, 0, 1.5, 0, false});
    add({"mirror", {0.98, 0.99, 1.0}, {0, 0, 0}, 0.0, 1.5, 1, false});
    add({"painting_green", {0.16, 0.46, 0.40}, {0.035, 0.08, 0.06}, 0, 1.5, 0, false});
    add({"painting_orange", {0.78, 0.34, 0.12}, {0.12, 0.045, 0.015}, 0, 1.5, 0, false});
    add({"painting_red", {0.52, 0.08, 0.06}, {0.08, 0.012, 0.008}, 0, 1.5, 0, false});
    add({"white_piece", {0.78, 0.75, 0.66}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"black_piece", {0.035, 0.030, 0.026}, {0, 0, 0}, 0.0, 1.5, 0, false});
    add({"matte_black", {0.02, 0.02, 0.025}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"plaster_light", {0.72, 0.58, 0.43}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"plaster_raw", {0.36, 0.20, 0.12}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"plaster_shadow", {0.16, 0.095, 0.060}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"old_wood", {0.34, 0.16, 0.060}, {0, 0, 0}, 0.08, 1.5, 0, false});
    add({"old_wood_dark", {0.12, 0.060, 0.030}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"old_wood_light", {0.60, 0.30, 0.11}, {0, 0, 0}, 0.04, 1.5, 0, false});
    add({"dust", {0.18, 0.14, 0.10}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"shadow_hole", {0.018, 0.014, 0.011}, {0, 0, 0}, 0, 1.5, 0, false});
    add({"metal", {0.78, 0.78, 0.74}, {0, 0, 0}, opt.metalRoughness, 1.5, 1, false});
    add({"glass", {0.94, 0.98, 1.0}, {0, 0, 0}, 0.01, 1.52, 2, false});
    add({"light_panel", {1, 1, 1}, {18.0, 14.0, 10.0}, 0, 1.5, 3, true});
    add({"light_fill", {1, 1, 1}, {10.0, 8.0, 6.0}, 0, 1.5, 3, true});
    return m;
}

struct FaceVertex {
    int v = 0;
    int vn = 0;
};

FaceVertex parseFaceVertex(const std::string &tok) {
    FaceVertex out;
    size_t slash = tok.find('/');
    if (slash == std::string::npos) {
        out.v = std::stoi(tok);
        return out;
    }
    out.v = std::stoi(tok.substr(0, slash));
    size_t slash2 = tok.find('/', slash + 1);
    if (slash2 == std::string::npos) {
        return out;
    }
    if (slash2 + 1 < tok.size()) {
        out.vn = std::stoi(tok.substr(slash2 + 1));
    }
    return out;
}

void finalizeTriangle(Triangle &t) {
    Vec3 e1 = t.v1 - t.v0;
    Vec3 e2 = t.v2 - t.v0;
    t.normal = normalize(cross(e1, e2));
    t.area = 0.5 * length(cross(e1, e2));
    t.centroid = (t.v0 + t.v1 + t.v2) / 3.0;
    t.bmin = minVec(t.v0, minVec(t.v1, t.v2));
    t.bmax = maxVec(t.v0, maxVec(t.v1, t.v2));
}

Vec3 rotateY(const Vec3 &p, const Vec3 &center, double radians) {
    double c = std::cos(radians), s = std::sin(radians);
    Vec3 q = p - center;
    return {center.x + c * q.x + s * q.z, center.y + q.y, center.z - s * q.x + c * q.z};
}

int objectVisibility(const std::string &object) {
    if (object.rfind("CameraOnly", 0) == 0) return kCameraOnly;
    if (object.rfind("ReflectionOnly", 0) == 0) return kReflectionOnly;
    return kVisibleToAll;
}

void rebuildSmoothNormalsForMaterial(std::vector<Triangle> &tris, int materialId) {
    std::map<std::pair<std::string, int>, Vec3> accum;
    for (const auto &tri : tris) {
        if (tri.material != materialId) continue;
        Vec3 weightedNormal = tri.area * tri.normal;
        accum[{tri.object, tri.vi0}] += weightedNormal;
        accum[{tri.object, tri.vi1}] += weightedNormal;
        accum[{tri.object, tri.vi2}] += weightedNormal;
    }

    for (auto &tri : tris) {
        if (tri.material != materialId) continue;
        auto smoothNormal = [&](int vertexId) {
            auto it = accum.find({tri.object, vertexId});
            return (it != accum.end() && length(it->second) > 0.0) ? normalize(it->second) : tri.normal;
        };
        tri.n0 = smoothNormal(tri.vi0);
        tri.n1 = smoothNormal(tri.vi1);
        tri.n2 = smoothNormal(tri.vi2);
        tri.hasVertexNormals = true;
    }
}

void rebuildSmoothNormalsForObjectGroup(std::vector<Triangle> &tris,
                                        const std::vector<std::string> &objectPrefixes,
                                        double mergeRadius) {
    auto matchesPrefix = [&](const std::string &object) {
        for (const auto &prefix : objectPrefixes) {
            if (object.rfind(prefix, 0) == 0) return true;
        }
        return false;
    };

    struct VertexEntry {
        Vec3 position;
        Vec3 normalSum{0.0, 0.0, 0.0};
    };
    using VertexKey = std::pair<std::string, int>;
    using CellKey = std::tuple<int, int, int>;

    std::map<VertexKey, VertexEntry> entries;
    auto accumulateVertex = [&](const Triangle &tri, int vertexId, const Vec3 &position) {
        auto &entry = entries[{tri.object, vertexId}];
        entry.position = position;
        entry.normalSum += tri.area * tri.normal;
    };

    for (const auto &tri : tris) {
        if (!matchesPrefix(tri.object)) continue;
        accumulateVertex(tri, tri.vi0, tri.v0);
        accumulateVertex(tri, tri.vi1, tri.v1);
        accumulateVertex(tri, tri.vi2, tri.v2);
    }
    if (entries.empty()) return;

    std::map<CellKey, std::vector<VertexKey>> grid;
    double invCell = 1.0 / mergeRadius;
    auto cellFor = [&](const Vec3 &p) {
        return CellKey{int(std::floor(p.x * invCell)), int(std::floor(p.y * invCell)), int(std::floor(p.z * invCell))};
    };
    for (const auto &[key, entry] : entries) {
        grid[cellFor(entry.position)].push_back(key);
    }

    double radius2 = mergeRadius * mergeRadius;
    std::map<VertexKey, Vec3> smoothed;
    for (const auto &[key, entry] : entries) {
        auto [cx, cy, cz] = cellFor(entry.position);
        Vec3 sum{0.0, 0.0, 0.0};
        for (int dx = -1; dx <= 1; ++dx) {
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dz = -1; dz <= 1; ++dz) {
                    auto it = grid.find(CellKey{cx + dx, cy + dy, cz + dz});
                    if (it == grid.end()) continue;
                    for (const auto &otherKey : it->second) {
                        const auto &other = entries.at(otherKey);
                        Vec3 delta = other.position - entry.position;
                        if (dot(delta, delta) <= radius2) sum += other.normalSum;
                    }
                }
            }
        }
        smoothed[key] = length(sum) > 0.0 ? normalize(sum) : normalize(entry.normalSum);
    }

    for (auto &tri : tris) {
        if (!matchesPrefix(tri.object)) continue;
        tri.n0 = smoothed[{tri.object, tri.vi0}];
        tri.n1 = smoothed[{tri.object, tri.vi1}];
        tri.n2 = smoothed[{tri.object, tri.vi2}];
        tri.hasVertexNormals = true;
    }
}

bool canRaySee(const Ray &ray, const Triangle &tri) {
    if (tri.visibility == kVisibleToAll) return true;
    if (tri.visibility == kCameraOnly) return ray.visibility == kPrimaryRay;
    if (tri.visibility == kReflectionOnly) return ray.visibility == kMirrorRay;
    return true;
}

std::vector<Triangle> loadObj(const Options &opt, const std::map<std::string, int> &matIds) {
    std::ifstream in(opt.objPath);
    if (!in) {
        std::cerr << "Could not open " << opt.objPath << "\n";
        std::exit(1);
    }
    std::vector<Vec3> verts(1);
    std::vector<Vec3> norms(1);
    std::vector<Triangle> tris;
    std::string currentMat = "wall";
    std::string currentObj = "Object";
    std::string line;
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
        } else if (tag == "usemtl") {
            ss >> currentMat;
        } else if (tag == "o") {
            ss >> currentObj;
        } else if (tag == "f") {
            std::vector<FaceVertex> ids;
            std::string tok;
            while (ss >> tok) ids.push_back(parseFaceVertex(tok));
            for (size_t i = 1; i + 1 < ids.size(); ++i) {
                Triangle t;
                t.v0 = verts[ids[0].v];
                t.v1 = verts[ids[i].v];
                t.v2 = verts[ids[i + 1].v];
                t.vi0 = ids[0].v;
                t.vi1 = ids[i].v;
                t.vi2 = ids[i + 1].v;
                if (ids[0].vn > 0 && ids[0].vn < int(norms.size()) &&
                    ids[i].vn > 0 && ids[i].vn < int(norms.size()) &&
                    ids[i + 1].vn > 0 && ids[i + 1].vn < int(norms.size())) {
                    t.n0 = normalize(norms[ids[0].vn]);
                    t.n1 = normalize(norms[ids[i].vn]);
                    t.n2 = normalize(norms[ids[i + 1].vn]);
                    t.hasVertexNormals = true;
                }
                t.object = currentObj;
                t.visibility = objectVisibility(currentObj);
                auto it = matIds.find(currentMat);
                t.material = it == matIds.end() ? 0 : it->second;
                finalizeTriangle(t);
                tris.push_back(t);
            }
        }
    }

    Vec3 transformCenter{0.0, 1.0, 1.75};
    double radians = opt.objectRotateY * kPi / 180.0;
    for (auto &t : tris) {
        bool movable = t.object.rfind("Visible", 0) == 0 ||
                       t.object.rfind("CameraOnly", 0) == 0 ||
                       t.object.rfind("ReflectionOnly", 0) == 0;
        if (movable) {
            t.v0 = rotateY(t.v0, transformCenter, radians) + opt.objectOffset;
            t.v1 = rotateY(t.v1, transformCenter, radians) + opt.objectOffset;
            t.v2 = rotateY(t.v2, transformCenter, radians) + opt.objectOffset;
            if (t.hasVertexNormals) {
                t.n0 = normalize(rotateY(t.n0, Vec3{0.0, 0.0, 0.0}, radians));
                t.n1 = normalize(rotateY(t.n1, Vec3{0.0, 0.0, 0.0}, radians));
                t.n2 = normalize(rotateY(t.n2, Vec3{0.0, 0.0, 0.0}, radians));
            }
            finalizeTriangle(t);
        }
    }
    for (const char *softFurnitureMat : {"sofa_body", "sofa_cushion", "sofa_pillow"}) {
        auto it = matIds.find(softFurnitureMat);
        if (it == matIds.end()) continue;
        // The borrowed room asset ships broken upholstery normals that create
        // visible seams and blotchy shading; rebuild smooth normals for the
        // soft sofa pieces inside the renderer.
        rebuildSmoothNormalsForMaterial(tris, it->second);
    }
    // The three back-cushion objects are modeled as separate strips with tiny
    // gaps, so smooth them together across nearby boundary vertices.
    rebuildSmoothNormalsForObjectGroup(
        tris,
        {"CameraOnlyImportedRoomFurniture_28", "CameraOnlyImportedRoomFurniture_29", "CameraOnlyImportedRoomFurniture_30"},
        0.04);
    return tris;
}

struct Scene {
    Options opt;
    std::vector<Material> materials;
    Texture floorTexture;
    std::vector<Triangle> triangles;
    std::vector<int> prims;
    std::vector<BvhNode> nodes;
    std::vector<int> lightTris;

    explicit Scene(Options options) : opt(std::move(options)) {
        std::map<std::string, int> matIds;
        materials = makeMaterials(opt, matIds);
        floorTexture = loadPpmTexture("textures/old_wood_floor.ppm");
        if (!hasTexture(floorTexture)) {
            std::cerr << "Warning: could not load textures/old_wood_floor.ppm, using flat floor color\n";
        }
        scene_builder::BuildOptions buildOpt;
        buildOpt.objPath = opt.objPath;
        buildOpt.mtlPath = opt.mtlPath;
        buildOpt.floorColor[0] = opt.floorColor.x;
        buildOpt.floorColor[1] = opt.floorColor.y;
        buildOpt.floorColor[2] = opt.floorColor.z;
        buildOpt.lightPos[0] = opt.lightPos.x;
        buildOpt.lightPos[1] = opt.lightPos.y;
        buildOpt.lightPos[2] = opt.lightPos.z;
        buildOpt.lightSize = opt.lightSize;
        buildOpt.extraLight = opt.extraLight;
        const std::string roomAssetPath = "57-estancia_comedor_obj/room.obj";
        if (fileExists(roomAssetPath)) {
            scene_builder::writeSceneObj(buildOpt);
        } else if (fileExists(opt.objPath)) {
            std::cerr << "Warning: " << roomAssetPath
                      << " not found; using existing " << opt.objPath
                      << " without regenerating the scene\n";
        } else {
            std::cerr << "Could not regenerate scene because " << roomAssetPath
                      << " is missing, and fallback scene " << opt.objPath
                      << " was not found\n";
            std::exit(1);
        }
        triangles = loadObj(opt, matIds);
        for (int i = 0; i < int(triangles.size()); ++i) {
            const Material &m = materials[triangles[i].material];
            if (m.sampleLight && triangles[i].area > 0.0) lightTris.push_back(i);
        }
        prims.resize(triangles.size());
        for (int i = 0; i < int(prims.size()); ++i) prims[i] = i;
        buildBvh();
    }

    Vec3 surfaceAlbedo(const Hit &hit, const Material &mat) const {
        if (hasTexture(floorTexture) && mat.name == "floor" && std::abs(hit.normal.y) > 0.9) {
            double u = hit.p.x * 0.18 + 0.11;
            double v = hit.p.z * 0.18 + 0.27;
            Vec3 tex = sampleTextureRepeat(floorTexture, u, v);
            return {tex.x * 0.98, tex.y * 0.96, tex.z * 0.92};
        }
        return mat.albedo;
    }

    int buildNode(int start, int end) {
        int nodeId = int(nodes.size());
        nodes.push_back({});
        BvhNode &node = nodes.back();
        Aabb box, centroidBox;
        for (int i = start; i < end; ++i) {
            const Triangle &t = triangles[prims[i]];
            box.expand(t.bmin);
            box.expand(t.bmax);
            centroidBox.expand(t.centroid);
        }
        node.box = box;
        int count = end - start;
        if (count <= 6) {
            node.start = start;
            node.count = count;
            return nodeId;
        }
        Vec3 diag = centroidBox.mx - centroidBox.mn;
        int axis = 0;
        if (diag.y > diag.x && diag.y > diag.z) axis = 1;
        else if (diag.z > diag.x) axis = 2;
        int mid = start + count / 2;
        std::nth_element(prims.begin() + start, prims.begin() + mid, prims.begin() + end,
                         [&](int a, int b) { return triangles[a].centroid[axis] < triangles[b].centroid[axis]; });
        node.left = buildNode(start, mid);
        node.right = buildNode(mid, end);
        return nodeId;
    }

    void buildBvh() {
        nodes.reserve(triangles.size() * 2);
        if (!triangles.empty()) buildNode(0, int(triangles.size()));
    }

    bool hitTri(const Triangle &tri, const Ray &r, double tMin, double tMax, Hit &hit) const {
        if (!canRaySee(r, tri)) return false;
        Vec3 e1 = tri.v1 - tri.v0;
        Vec3 e2 = tri.v2 - tri.v0;
        Vec3 pvec = cross(r.direction, e2);
        double det = dot(e1, pvec);
        if (std::fabs(det) < 1e-10) return false;
        double invDet = 1.0 / det;
        Vec3 tvec = r.origin - tri.v0;
        double u = dot(tvec, pvec) * invDet;
        if (u < 0.0 || u > 1.0) return false;
        Vec3 qvec = cross(tvec, e1);
        double v = dot(r.direction, qvec) * invDet;
        if (v < 0.0 || u + v > 1.0) return false;
        double t = dot(e2, qvec) * invDet;
        if (t < tMin || t > tMax) return false;
        hit.t = t;
        hit.p = r.at(t);
        double w = 1.0 - u - v;
        Vec3 shadingNormal = tri.hasVertexNormals ? normalize(w * tri.n0 + u * tri.n1 + v * tri.n2) : tri.normal;
        if (dot(shadingNormal, tri.normal) < 0.0) shadingNormal = -shadingNormal;
        hit.frontFace = dot(r.direction, tri.normal) < 0.0;
        hit.normal = hit.frontFace ? shadingNormal : -shadingNormal;
        hit.material = tri.material;
        return true;
    }

    bool intersect(const Ray &r, double tMin, double tMax, Hit &out) const {
        bool any = false;
        double closest = tMax;
        int stack[96];
        int sp = 0;
        if (nodes.empty()) return false;
        stack[sp++] = 0;
        while (sp) {
            const BvhNode &node = nodes[stack[--sp]];
            if (!node.box.hit(r, tMin, closest)) continue;
            if (node.count > 0) {
                for (int i = 0; i < node.count; ++i) {
                    Hit h;
                    if (hitTri(triangles[prims[node.start + i]], r, tMin, closest, h)) {
                        any = true;
                        closest = h.t;
                        out = h;
                    }
                }
            } else {
                if (node.left >= 0) stack[sp++] = node.left;
                if (node.right >= 0) stack[sp++] = node.right;
            }
        }
        return any;
    }

    Vec3 background(const Ray &r) const {
        Vec3 d = normalize(r.direction);
        if (opt.environment == "studio") {
            double t = 0.5 * (d.y + 1.0);
            return (1.0 - t) * Vec3{0.30, 0.31, 0.34} + t * Vec3{0.75, 0.78, 0.84};
        }
        if (opt.environment == "sunset") {
            double t = std::clamp(0.5 * (d.y + 0.3), 0.0, 1.0);
            Vec3 sky = (1.0 - t) * Vec3{0.95, 0.38, 0.12} + t * Vec3{0.08, 0.12, 0.28};
            return 0.55 * sky;
        }

        Vec3 bhDir = normalize(Vec3{0.0, 0.02, -1.0});
        Vec3 right = normalize(cross(bhDir, Vec3{0, 1, 0}));
        Vec3 up = cross(right, bhDir);
        double y = dot(d, up);
        double z = dot(d, bhDir);
        double angle = std::acos(std::clamp(z, -1.0, 1.0));
        Vec3 color{0.002, 0.003, 0.010};
        double disk = std::exp(-std::pow(std::fabs(y) / 0.020, 2.0)) * std::exp(-std::pow((angle - 0.115) / 0.15, 2.0));
        color += disk * Vec3{2.2, 0.78, 0.14};
        double halo = std::exp(-std::pow(angle / 0.18, 2.0));
        color += halo * Vec3{0.18, 0.20, 0.28};
        if (angle < 0.060) color *= 0.03;

        double cellX = std::floor((std::atan2(d.z, d.x) + kPi) * 180.0);
        double cellY = std::floor((std::asin(d.y) + 0.5 * kPi) * 180.0);
        double h = std::sin(cellX * 127.1 + cellY * 311.7) * 43758.5453;
        h = h - std::floor(h);
        if (h > 0.994) color += Vec3{1.7, 1.8, 2.0} * ((h - 0.994) / 0.006);
        return color;
    }

    Vec3 sampleDirect(const Hit &hit, const Material &mat, int visibility, Rng &rng) const {
        if (lightTris.empty() || mat.type == 2 || mat.type == 3) return {0, 0, 0};
        Vec3 result{0, 0, 0};
        Vec3 brdf = surfaceAlbedo(hit, mat) / kPi;
        for (int idx : lightTris) {
            const Triangle &lt = triangles[idx];
            double r1 = rng.next();
            double r2 = rng.next();
            double sr1 = std::sqrt(r1);
            Vec3 lp = (1.0 - sr1) * lt.v0 + (sr1 * (1.0 - r2)) * lt.v1 + (sr1 * r2) * lt.v2;
            Vec3 toLight = lp - hit.p;
            double dist2 = dot(toLight, toLight);
            double dist = std::sqrt(dist2);
            Vec3 wi = toLight / dist;
            double cosSurface = std::max(0.0, dot(hit.normal, wi));
            double cosLight = std::max(0.0, dot(lt.normal, -wi));
            if (cosSurface <= 0.0 || cosLight <= 0.0) continue;
            bool visible = true;
            if (opt.shadows) {
                Hit shadowHit;
                Ray shadowRay{hit.p + 1e-4 * hit.normal, wi, visibility};
                visible = !intersect(shadowRay, 1e-4, dist - 2e-4, shadowHit);
            }
            if (visible) {
                const Material &lm = materials[lt.material];
                result += lm.emission * brdf * (lt.area * cosSurface * cosLight / std::max(1e-8, dist2));
            }
        }
        return result;
    }

    bool scatter(const Ray &ray, const Hit &hit, const Material &mat, Rng &rng, Vec3 &atten, Ray &scattered) const {
        if (mat.type == 0) {
            Vec3 dir = cosineHemisphere(hit.normal, rng);
            scattered = {hit.p + 1e-4 * hit.normal, dir, ray.visibility};
            atten = surfaceAlbedo(hit, mat);
            return true;
        }
        if (mat.type == 1) {
            Vec3 dir = normalize(reflect(normalize(ray.direction), hit.normal) + mat.roughness * randomInUnitSphere(rng));
            int visibility = mat.name == "mirror" ? kMirrorRay : ray.visibility;
            scattered = {hit.p + 1e-4 * hit.normal, dir, visibility};
            atten = mat.albedo;
            return dot(scattered.direction, hit.normal) > 0.0;
        }
        if (mat.type == 2) {
            atten = mat.albedo;
            double refractionRatio = hit.frontFace ? (1.0 / mat.ior) : mat.ior;
            Vec3 unitDir = normalize(ray.direction);
            double cosTheta = std::min(dot(-unitDir, hit.normal), 1.0);
            double sinTheta = std::sqrt(std::max(0.0, 1.0 - cosTheta * cosTheta));
            Vec3 dir;
            bool cannotRefract = refractionRatio * sinTheta > 1.0;
            if (cannotRefract || schlick(cosTheta, refractionRatio) > rng.next()) {
                dir = reflect(unitDir, hit.normal);
            } else {
                refract(unitDir, hit.normal, refractionRatio, dir);
            }
            Vec3 offsetNormal = dot(dir, hit.normal) > 0.0 ? hit.normal : -hit.normal;
            scattered = {hit.p + 1e-4 * offsetNormal, normalize(dir), ray.visibility};
            return true;
        }
        return false;
    }

    Vec3 trace(const Ray &start, Rng &rng) const {
        Ray ray = start;
        Vec3 radiance{0, 0, 0};
        Vec3 throughput{1, 1, 1};
        for (int depth = 0; depth < opt.maxDepth; ++depth) {
            Hit hit;
            if (!intersect(ray, 1e-4, kInf, hit)) {
                radiance += throughput * background(ray);
                break;
            }
            const Material &mat = materials[hit.material];
            if (mat.type == 3) {
                // Keep area lights useful for direct illumination without turning the mirror into
                // a reflection of white light panels.
                if (depth == 0) radiance += throughput * mat.emission;
                break;
            }
            // Keep a small amount of direct light on the mirror for a softer, more staged look
            // without bringing back the frosted veil from full diffuse lighting.
            double directLightScale = mat.name == "mirror" ? 0.12 : 1.0;
            radiance += throughput * (directLightScale * sampleDirect(hit, mat, ray.visibility, rng));
            Ray scattered;
            Vec3 atten;
            if (!scatter(ray, hit, mat, rng, atten, scattered)) break;
            throughput = throughput * atten;
            if (depth >= 3) {
                double p = std::clamp(std::max({throughput.x, throughput.y, throughput.z}), 0.05, 0.95);
                if (rng.next() > p) break;
                throughput /= p;
            }
            ray = scattered;
        }
        return radiance;
    }
};

struct Camera {
    Vec3 origin, lowerLeft, horizontal, vertical;

    Camera(const Options &opt) {
        origin = opt.camera;
        double aspect = double(opt.width) / opt.height;
        double theta = opt.fov * kPi / 180.0;
        double viewportH = 2.0 * std::tan(theta / 2.0);
        double viewportW = aspect * viewportH;
        Vec3 w = normalize(opt.camera - opt.lookAt);
        Vec3 u = normalize(cross(Vec3{0, 1, 0}, w));
        Vec3 v = cross(w, u);
        horizontal = viewportW * u;
        vertical = viewportH * v;
        lowerLeft = origin - horizontal / 2.0 - vertical / 2.0 - w;
    }

    Ray getRay(double s, double t) const {
        return {origin, normalize(lowerLeft + s * horizontal + t * vertical - origin)};
    }
};

void writePpm(const std::string &path, const std::vector<Vec3> &pixels, int w, int h) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "Could not write " << path << "\n";
        std::exit(1);
    }
    out << "P6\n" << w << " " << h << "\n255\n";
    for (const Vec3 &p : pixels) {
        Vec3 c = clamp01({std::sqrt(std::max(0.0, p.x)), std::sqrt(std::max(0.0, p.y)), std::sqrt(std::max(0.0, p.z))});
        unsigned char rgb[3] = {
            static_cast<unsigned char>(255.999 * c.x),
            static_cast<unsigned char>(255.999 * c.y),
            static_cast<unsigned char>(255.999 * c.z)
        };
        out.write(reinterpret_cast<char *>(rgb), 3);
    }
}

int main(int argc, char **argv) {
    auto start = std::chrono::high_resolution_clock::now();
    Options opt = parseArgs(argc, argv);
    std::cerr << "Generating OBJ scene and loading mesh...\n";
    Scene scene(opt);
    Camera cam(opt);
    std::cerr << "Triangles: " << scene.triangles.size() << ", BVH nodes: " << scene.nodes.size()
              << ", sampled lights: " << scene.lightTris.size() << "\n";
    std::cerr << "Rendering " << opt.width << "x" << opt.height << " spp=" << opt.spp
              << " threads=" << opt.threads << " shadows=" << (opt.shadows ? "on" : "off") << "\n";

    std::vector<Vec3> pixels(size_t(opt.width) * opt.height);
    std::atomic<int> nextRow{0};
    auto worker = [&](int tid) {
        int y;
        while ((y = nextRow.fetch_add(1)) < opt.height) {
            uint64_t baseSeed = 1469598103934665603ull ^ (uint64_t(y) + 0x9e3779b97f4a7c15ull * uint64_t(tid + 1));
            Rng rng(baseSeed);
            for (int x = 0; x < opt.width; ++x) {
                Vec3 color{0, 0, 0};
                for (int s = 0; s < opt.spp; ++s) {
                    double u = (x + rng.next()) / (opt.width - 1);
                    double v = (opt.height - 1 - y + rng.next()) / (opt.height - 1);
                    color += scene.trace(cam.getRay(u, v), rng);
                }
                pixels[size_t(y) * opt.width + x] = color / double(opt.spp);
            }
            if (tid == 0 && y % 32 == 0) {
                std::cerr << "row " << y << "/" << opt.height << "\n";
            }
        }
    };

    std::vector<std::thread> threads;
    for (int i = 0; i < opt.threads; ++i) threads.emplace_back(worker, i);
    for (auto &t : threads) t.join();

    writePpm(opt.output, pixels, opt.width, opt.height);
    auto end = std::chrono::high_resolution_clock::now();
    double seconds = std::chrono::duration<double>(end - start).count();
    std::cerr << "Wrote " << opt.output << " in " << std::fixed << std::setprecision(2) << seconds << "s\n";
    return 0;
}
