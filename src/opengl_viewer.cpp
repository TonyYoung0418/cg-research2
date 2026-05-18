#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/glut.h>
#endif

#include "scene_builder.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

Vec3 operator+(const Vec3 &a, const Vec3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec3 operator-(const Vec3 &a, const Vec3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec3 operator*(const Vec3 &a, float s) { return {a.x * s, a.y * s, a.z * s}; }
Vec3 operator/(const Vec3 &a, float s) { return {a.x / s, a.y / s, a.z / s}; }

Vec3 cross(const Vec3 &a, const Vec3 &b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}

float dot(const Vec3 &a, const Vec3 &b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3 normalize(const Vec3 &v) {
    float len = std::sqrt(std::max(0.0f, dot(v, v)));
    return len > 1e-8f ? v / len : Vec3{0.0f, 1.0f, 0.0f};
}

struct Material {
    Vec3 kd{0.7f, 0.7f, 0.7f};
    Vec3 ke{0.0f, 0.0f, 0.0f};
};

struct Tri {
    Vec3 a;
    Vec3 b;
    Vec3 c;
    Vec3 n;
    Material mat;
    bool cameraOnly = false;
    bool reflectionOnly = false;
};

std::vector<Tri> gTris;
bool gShowReflectionOnly = false;
int gWidth = 1280;
int gHeight = 800;
float gYaw = 0.0f;
float gPitch = -0.08f;
float gDistance = 5.2f;
Vec3 gTarget{0.0f, 1.25f, 1.75f};
bool gDragging = false;
bool gPanning = false;
int gLastX = 0;
int gLastY = 0;

int parseFaceIndex(const std::string &token) {
    size_t slash = token.find('/');
    return std::stoi(slash == std::string::npos ? token : token.substr(0, slash));
}

std::map<std::string, Material> loadMtl(const std::string &path) {
    std::ifstream in(path);
    std::map<std::string, Material> mats;
    std::string current;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "newmtl") {
            ss >> current;
            mats[current] = {};
        } else if (tag == "Kd" && !current.empty()) {
            ss >> mats[current].kd.x >> mats[current].kd.y >> mats[current].kd.z;
        } else if (tag == "Ke" && !current.empty()) {
            ss >> mats[current].ke.x >> mats[current].ke.y >> mats[current].ke.z;
        }
    }
    return mats;
}

bool startsWith(const std::string &s, const std::string &prefix) {
    return s.rfind(prefix, 0) == 0;
}

void loadObj(const std::string &objPath, const std::string &mtlPath) {
    std::ifstream in(objPath);
    if (!in) {
        std::cerr << "Could not open " << objPath << "\n";
        std::exit(1);
    }

    std::map<std::string, Material> mats = loadMtl(mtlPath);
    Material currentMat;
    std::string currentObj = "Object";
    std::vector<Vec3> verts(1);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string tag;
        ss >> tag;
        if (tag == "v") {
            Vec3 v;
            ss >> v.x >> v.y >> v.z;
            verts.push_back(v);
        } else if (tag == "usemtl") {
            std::string name;
            ss >> name;
            auto it = mats.find(name);
            currentMat = it == mats.end() ? Material{} : it->second;
        } else if (tag == "o") {
            ss >> currentObj;
        } else if (tag == "f") {
            std::vector<int> ids;
            std::string token;
            while (ss >> token) ids.push_back(parseFaceIndex(token));
            for (size_t i = 1; i + 1 < ids.size(); ++i) {
                Tri tri;
                tri.a = verts[ids[0]];
                tri.b = verts[ids[i]];
                tri.c = verts[ids[i + 1]];
                tri.n = normalize(cross(tri.b - tri.a, tri.c - tri.a));
                tri.mat = currentMat;
                tri.cameraOnly = startsWith(currentObj, "CameraOnly");
                tri.reflectionOnly = startsWith(currentObj, "ReflectionOnly");
                gTris.push_back(tri);
            }
        }
    }
    std::cerr << "Loaded " << gTris.size() << " triangles from " << objPath << "\n";
}

Vec3 cameraPosition() {
    float cp = std::cos(gPitch);
    return {
        gTarget.x + gDistance * std::sin(gYaw) * cp,
        gTarget.y + gDistance * std::sin(gPitch),
        gTarget.z + gDistance * std::cos(gYaw) * cp
    };
}

Vec3 cameraRight() {
    Vec3 pos = cameraPosition();
    return normalize(cross(normalize(gTarget - pos), {0.0f, 1.0f, 0.0f}));
}

void setPreset(int preset) {
    if (preset == 1) {
        gTarget = {0.0f, 1.25f, 1.75f};
        gYaw = 0.0f;
        gPitch = -0.08f;
        gDistance = 5.2f;
    } else if (preset == 2) {
        gTarget = {0.0f, 1.25f, 1.65f};
        gYaw = -0.72f;
        gPitch = -0.10f;
        gDistance = 4.0f;
    } else if (preset == 3) {
        gTarget = {0.0f, 1.0f, 2.3f};
        gYaw = 0.0f;
        gPitch = -1.15f;
        gDistance = 5.0f;
    } else if (preset == 4) {
        gTarget = {0.25f, 1.35f, 1.45f};
        gYaw = 0.0f;
        gPitch = -0.02f;
        gDistance = 3.1f;
    }
    glutPostRedisplay();
}

void drawText(float x, float y, const std::string &text) {
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    gluOrtho2D(0.0, gWidth, 0.0, gHeight);
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();
    glDisable(GL_LIGHTING);
    glColor3f(0.92f, 0.92f, 0.88f);
    glRasterPos2f(x, y);
    for (char c : text) glutBitmapCharacter(GLUT_BITMAP_8_BY_13, c);
    glEnable(GL_LIGHTING);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
}

void display() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(54.0, double(gWidth) / double(std::max(1, gHeight)), 0.03, 80.0);

    Vec3 eye = cameraPosition();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    gluLookAt(eye.x, eye.y, eye.z, gTarget.x, gTarget.y, gTarget.z, 0.0, 1.0, 0.0);

    GLfloat lightPos[] = {-2.0f, 4.5f, 3.5f, 1.0f};
    GLfloat lightDiffuse[] = {0.95f, 0.90f, 0.82f, 1.0f};
    GLfloat ambient[] = {0.20f, 0.20f, 0.20f, 1.0f};
    glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
    glLightfv(GL_LIGHT0, GL_DIFFUSE, lightDiffuse);
    glLightModelfv(GL_LIGHT_MODEL_AMBIENT, ambient);

    glBegin(GL_TRIANGLES);
    for (const Tri &tri : gTris) {
        if (gShowReflectionOnly) {
            if (tri.cameraOnly) continue;
        } else {
            if (tri.reflectionOnly) continue;
        }
        Vec3 color = tri.mat.kd + tri.mat.ke * 0.08f;
        color.x = std::min(color.x, 1.0f);
        color.y = std::min(color.y, 1.0f);
        color.z = std::min(color.z, 1.0f);
        glColor3f(color.x, color.y, color.z);
        glNormal3f(tri.n.x, tri.n.y, tri.n.z);
        glVertex3f(tri.a.x, tri.a.y, tri.a.z);
        glVertex3f(tri.b.x, tri.b.y, tri.b.z);
        glVertex3f(tri.c.x, tri.c.y, tri.c.z);
    }
    glEnd();

    drawText(14.0f, float(gHeight - 24), "Mouse drag: orbit | Right drag: pan | Wheel/W/S: zoom | 1-4: views | V: camera/reflection objects | R: reset | Esc: quit");
    drawText(14.0f, float(gHeight - 42), gShowReflectionOnly ? "Mode: reflection-object preview" : "Mode: camera-object preview");

    glutSwapBuffers();
}

void reshape(int w, int h) {
    gWidth = std::max(1, w);
    gHeight = std::max(1, h);
    glViewport(0, 0, gWidth, gHeight);
}

void mouse(int button, int state, int x, int y) {
    if (button == 3 && state == GLUT_DOWN) {
        gDistance = std::max(0.8f, gDistance * 0.90f);
        glutPostRedisplay();
        return;
    }
    if (button == 4 && state == GLUT_DOWN) {
        gDistance = std::min(18.0f, gDistance * 1.10f);
        glutPostRedisplay();
        return;
    }
    gLastX = x;
    gLastY = y;
    gDragging = button == GLUT_LEFT_BUTTON && state == GLUT_DOWN;
    gPanning = button == GLUT_RIGHT_BUTTON && state == GLUT_DOWN;
}

void motion(int x, int y) {
    int dx = x - gLastX;
    int dy = y - gLastY;
    gLastX = x;
    gLastY = y;

    if (gDragging) {
        gYaw += 0.008f * float(dx);
        gPitch = std::clamp(gPitch - 0.006f * float(dy), -1.45f, 1.20f);
    } else if (gPanning) {
        Vec3 right = cameraRight();
        Vec3 up{0.0f, 1.0f, 0.0f};
        float scale = 0.0025f * gDistance;
        gTarget = gTarget - right * (float(dx) * scale) + up * (float(dy) * scale);
    }
    glutPostRedisplay();
}

void keyboard(unsigned char key, int, int) {
    Vec3 forward = normalize(gTarget - cameraPosition());
    Vec3 right = cameraRight();
    float step = 0.15f;
    switch (key) {
        case 27:
            std::exit(0);
        case '1':
            setPreset(1);
            return;
        case '2':
            setPreset(2);
            return;
        case '3':
            setPreset(3);
            return;
        case '4':
            setPreset(4);
            return;
        case 'r':
        case 'R':
            setPreset(1);
            return;
        case 'v':
        case 'V':
            gShowReflectionOnly = !gShowReflectionOnly;
            glutPostRedisplay();
            return;
        case 'w':
        case 'W':
            gDistance = std::max(0.8f, gDistance * 0.90f);
            break;
        case 's':
        case 'S':
            gDistance = std::min(18.0f, gDistance * 1.10f);
            break;
        case 'a':
        case 'A':
            gTarget = gTarget - right * step;
            break;
        case 'd':
        case 'D':
            gTarget = gTarget + right * step;
            break;
        case 'q':
        case 'Q':
            gTarget = gTarget + Vec3{0.0f, step, 0.0f};
            break;
        case 'e':
        case 'E':
            gTarget = gTarget - Vec3{0.0f, step, 0.0f};
            break;
        case 'z':
        case 'Z':
            gTarget = gTarget + forward * step;
            break;
        case 'x':
        case 'X':
            gTarget = gTarget - forward * step;
            break;
    }
    glutPostRedisplay();
}

void special(int key, int, int) {
    if (key == GLUT_KEY_LEFT) gYaw -= 0.08f;
    if (key == GLUT_KEY_RIGHT) gYaw += 0.08f;
    if (key == GLUT_KEY_UP) gPitch = std::clamp(gPitch + 0.06f, -1.45f, 1.20f);
    if (key == GLUT_KEY_DOWN) gPitch = std::clamp(gPitch - 0.06f, -1.45f, 1.20f);
    glutPostRedisplay();
}

void initGl() {
    glClearColor(0.06f, 0.06f, 0.06f, 1.0f);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_LIGHTING);
    glEnable(GL_LIGHT0);
    glEnable(GL_COLOR_MATERIAL);
    glColorMaterial(GL_FRONT_AND_BACK, GL_AMBIENT_AND_DIFFUSE);
    glEnable(GL_NORMALIZE);
    glShadeModel(GL_SMOOTH);
}

void printControls() {
    std::cerr << "OpenGL controls:\n"
              << "  Left mouse drag: orbit camera\n"
              << "  Right mouse drag: pan target\n"
              << "  Mouse wheel or W/S: zoom\n"
              << "  A/D/Q/E/Z/X: move target\n"
              << "  Arrow keys: orbit camera\n"
              << "  1 main, 2 side, 3 top, 4 close mirror\n"
              << "  V: toggle camera-only vs reflection-only objects\n"
              << "  R: reset, Esc: quit\n";
}

} // namespace

int main(int argc, char **argv) {
    scene_builder::writeSceneObj({});
    loadObj("scenes/generated_scene.obj", "scenes/generated_scene.mtl");
    printControls();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_DEPTH);
    glutInitWindowSize(gWidth, gHeight);
    glutCreateWindow("Mirror Chess OpenGL Viewer");
    initGl();
    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutMouseFunc(mouse);
    glutMotionFunc(motion);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(special);
    setPreset(1);
    glutMainLoop();
    return 0;
}
