#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <iostream>
#include <cmath>
#include <vector>
#include <string>
#include "src/Shader.h"

constexpr float PI = 3.14159265f;
constexpr int TEX_SIZE = 512;

struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 1.0f, 4.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    float speed = 1.2f;
    float sensitivity = 0.08f;

    glm::mat4 getViewMatrix() const {
        return glm::lookAt(position, position + front, up);
    }

    void updateVectors() {
        glm::vec3 f;
        f.x = std::cos(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        f.y = std::sin(glm::radians(pitch));
        f.z = std::sin(glm::radians(yaw)) * std::cos(glm::radians(pitch));
        front = glm::normalize(f);
    }
};

Camera camera;
float deltaTime = 0.0f, lastFrame = 0.0f;
float timeScale = 1.0f;
bool firstMouse = true;
float lastX = 600.0f, lastY = 400.0f;

void responsive(GLFWwindow* w, int width, int height);
void userInput(GLFWwindow* w);
void mouseCallback(GLFWwindow* w, double xpos, double ypos);

// --- Noise helpers for procedural textures ---
static float hash2D(int a, int b) {
    float v = std::sin(a * 12.9898f + b * 78.233f) * 43758.5453f;
    return v - std::floor(v);
}

static float smoothNoise(float x, float y) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3.0f - 2.0f * fx);
    fy = fy * fy * (3.0f - 2.0f * fy);
    float a = hash2D(ix, iy), b = hash2D(ix + 1, iy);
    float c = hash2D(ix, iy + 1), d = hash2D(ix + 1, iy + 1);
    return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
}

static float fbm(float x, float y, int octaves = 4) {
    float v = 0.0f, amp = 1.0f, freq = 1.0f, maxV = 0.0f;
    for (int i = 0; i < octaves; i++) {
        v += amp * smoothNoise(x * freq, y * freq);
        maxV += amp; amp *= 0.5f; freq *= 2.0f;
    }
    return v / maxV;
}

static void uploadTexture(GLuint id, int w, int h, const std::vector<unsigned char>& pixels) {
    glBindTexture(GL_TEXTURE_2D, id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

static GLuint makePlanetTexture(const glm::vec3& color, float variation, float seed) {
    std::vector<unsigned char> pix(TEX_SIZE * TEX_SIZE * 4);
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float u = (float)x / TEX_SIZE, v = (float)y / TEX_SIZE;
            float theta = u * 2.0f * PI, phi = v * PI;
            float sx = std::sin(phi) * std::cos(theta);
            float sy = std::cos(phi);
            float sz = std::sin(phi) * std::sin(theta);

            float n = fbm(sx * 3.0f + seed, sy * 3.0f + sz * 2.0f, 5);
            float band = std::sin(sy * 18.0f + n * 3.0f) * 0.5f + 0.5f;
            float spot = fbm(sx * 7.0f + seed + 10.0f, sz * 7.0f + sy * 3.0f, 3);

            float blend = n * 0.5f + band * 0.3f + spot * 0.2f;
            float dark = 1.0f - variation * 0.4f * (1.0f - blend);
            float bright = 0.3f + variation * 0.7f * blend;

            float r = color.r * dark + (1.0f - color.r) * bright * 0.15f;
            float g = color.g * dark + (1.0f - color.g) * bright * 0.15f;
            float b = color.b * dark + (1.0f - color.b) * bright * 0.15f;

            int idx = (y * TEX_SIZE + x) * 4;
            pix[idx]     = (unsigned char)(std::min(1.0f, std::max(0.0f, r)) * 255);
            pix[idx + 1] = (unsigned char)(std::min(1.0f, std::max(0.0f, g)) * 255);
            pix[idx + 2] = (unsigned char)(std::min(1.0f, std::max(0.0f, b)) * 255);
            pix[idx + 3] = 255;
        }
    }
    GLuint t;
    glGenTextures(1, &t);
    uploadTexture(t, TEX_SIZE, TEX_SIZE, pix);
    return t;
}

static GLuint makeSunTexture() {
    std::vector<unsigned char> pix(TEX_SIZE * TEX_SIZE * 4);
    for (int y = 0; y < TEX_SIZE; y++) {
        for (int x = 0; x < TEX_SIZE; x++) {
            float u = (float)x / TEX_SIZE, v = (float)y / TEX_SIZE;
            float theta = u * 2.0f * PI, phi = v * PI;
            float sx = std::sin(phi) * std::cos(theta);
            float sy = std::cos(phi);

            float n1 = fbm(sx * 6.0f, sy * 6.0f, 5);
            float n2 = fbm(sx * 12.0f + 50.0f, sy * 12.0f + 50.0f, 3);

            float r = 1.0f;
            float g = 0.5f + 0.5f * n1;
            float b = 0.05f + 0.25f * n1 + 0.2f * n2;

            int idx = (y * TEX_SIZE + x) * 4;
            pix[idx]     = (unsigned char)(r * 255);
            pix[idx + 1] = (unsigned char)(std::min(1.0f, g) * 255);
            pix[idx + 2] = (unsigned char)(std::min(1.0f, b) * 255);
            pix[idx + 3] = 255;
        }
    }
    GLuint t;
    glGenTextures(1, &t);
    uploadTexture(t, TEX_SIZE, TEX_SIZE, pix);
    return t;
}

static GLuint makeRingTexture() {
    std::vector<unsigned char> pix(1024 * 64);
    for (int x = 0; x < 1024; x++) {
        float t = (float)x / 1024.0f;
        float n = fbm(t * 20.0f, 0.5f, 4);
        float alpha = 1.0f;
        if (t < 0.05f || t > 0.95f) alpha = 0.0f;
        else if (t < 0.15f) alpha = (t - 0.05f) / 0.1f;
        else if (t > 0.85f) alpha = (0.95f - t) / 0.1f;

        float brightness = 0.6f + 0.4f * n;
        alpha *= (0.4f + 0.6f * (1.0f - std::abs(t - 0.5f) * 2.0f));

        pix[x * 4]     = (unsigned char)((0.7f + 0.3f * n) * brightness * 255);
        pix[x * 4 + 1] = (unsigned char)((0.5f + 0.3f * n) * brightness * 255);
        pix[x * 4 + 2] = (unsigned char)((0.2f + 0.2f * n) * brightness * 255);
        pix[x * 4 + 3] = (unsigned char)(std::min(1.0f, alpha) * 255);
    }
    GLuint t;
    glGenTextures(1, &t);
    glBindTexture(GL_TEXTURE_2D, t);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1024, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pix.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return t;
}

// --- Sphere geometry with UVs ---
struct SphereMesh {
    std::vector<float> vertices;
    std::vector<unsigned int> indices;
};

SphereMesh makeSphere(int stacks, int slices) {
    SphereMesh m;
    for (int i = 0; i <= stacks; i++) {
        float phi = PI * i / stacks;
        float y = std::cos(phi), r = std::sin(phi);
        float v = 1.0f - (float)i / stacks;
        for (int j = 0; j <= slices; j++) {
            float theta = 2.0f * PI * j / slices;
            float x = r * std::cos(theta), z = r * std::sin(theta);
            float u = (float)j / slices;
            m.vertices.push_back(x); m.vertices.push_back(y); m.vertices.push_back(z);
            m.vertices.push_back(x); m.vertices.push_back(y); m.vertices.push_back(z);
            m.vertices.push_back(u); m.vertices.push_back(v);
        }
    }
    for (int i = 0; i < stacks; i++) {
        for (int j = 0; j < slices; j++) {
            unsigned int a = i * (slices + 1) + j;
            unsigned int b = a + slices + 1;
            m.indices.push_back(a); m.indices.push_back(b); m.indices.push_back(a + 1);
            m.indices.push_back(b); m.indices.push_back(b + 1); m.indices.push_back(a + 1);
        }
    }
    return m;
}

struct OrbitLines { unsigned int vao, vbo; int count; };

OrbitLines makeOrbit(float radius, int segs = 96) {
    OrbitLines o;
    o.count = segs * 2;
    std::vector<float> verts;
    for (int i = 0; i < segs; i++) {
        float a0 = glm::radians((360.0f / segs) * i);
        float a1 = glm::radians((360.0f / segs) * (i + 1));
        float x0 = radius * std::cos(a0), z0 = radius * std::sin(a0);
        float x1 = radius * std::cos(a1), z1 = radius * std::sin(a1);
        verts.push_back(x0); verts.push_back(0); verts.push_back(z0);
        verts.push_back(0); verts.push_back(1); verts.push_back(0);
        verts.push_back(0); verts.push_back(0);
        verts.push_back(x1); verts.push_back(0); verts.push_back(z1);
        verts.push_back(0); verts.push_back(1); verts.push_back(0);
        verts.push_back(0); verts.push_back(0);
    }
    glGenVertexArrays(1, &o.vao); glGenBuffers(1, &o.vbo);
    glBindVertexArray(o.vao);
    glBindBuffer(GL_ARRAY_BUFFER, o.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * 4, verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * 4, 0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(3 * 4)); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(6 * 4)); glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return o;
}

// --- Ring geometry ---
struct RingMesh { unsigned int vao, vbo; int count; };

RingMesh makeRing(float inner, float outer, int segs = 64) {
    RingMesh r;
    r.count = segs * 6;
    std::vector<float> verts;
    for (int i = 0; i < segs; i++) {
        float a0 = glm::radians((360.0f / segs) * i);
        float a1 = glm::radians((360.0f / segs) * (i + 1));
        float c0 = std::cos(a0), s0 = std::sin(a0);
        float c1 = std::cos(a1), s1 = std::sin(a1);
        float x0i = inner * c0, z0i = inner * s0;
        float x0o = outer * c0, z0o = outer * s0;
        float x1i = inner * c1, z1i = inner * s1;
        float x1o = outer * c1, z1o = outer * s1;
        auto push = [&](float x, float y, float z, float u, float v) {
            verts.push_back(x); verts.push_back(y); verts.push_back(z);
            verts.push_back(0); verts.push_back(1); verts.push_back(0);
            verts.push_back(u); verts.push_back(v);
        };
        push(x0i, 0, z0i, 0, 0); push(x0o, 0, z0o, 1, 0);
        push(x1o, 0, z1o, 1, 1);
        push(x0i, 0, z0i, 0, 0); push(x1o, 0, z1o, 1, 1);
        push(x1i, 0, z1i, 0, 1);
    }
    glGenVertexArrays(1, &r.vao); glGenBuffers(1, &r.vbo);
    glBindVertexArray(r.vao);
    glBindBuffer(GL_ARRAY_BUFFER, r.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * 4, verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * 4, 0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(3 * 4)); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(6 * 4)); glEnableVertexAttribArray(2);
    glBindVertexArray(0);
    return r;
}

// --- Starfield ---
struct StarField { unsigned int vao, vbo; int count; };

StarField makeStars(int count) {
    StarField s;
    s.count = count;
    std::vector<float> verts;
    for (int i = 0; i < count; i++) {
        float theta = (float)(rand() % 10000) / 10000.0f * 2.0f * PI;
        float phi = std::acos((float)(rand() % 10000) / 10000.0f * 2.0f - 1.0f);
        float r = 80.0f + (float)(rand() % 10000) / 10000.0f * 20.0f;
        float x = r * std::sin(phi) * std::cos(theta);
        float y = r * std::cos(phi);
        float z = r * std::sin(phi) * std::sin(theta);
        float bright = 0.3f + (float)(rand() % 10000) / 10000.0f * 0.7f;
        verts.push_back(x); verts.push_back(y); verts.push_back(z);
        verts.push_back(bright);
    }
    glGenVertexArrays(1, &s.vao); glGenBuffers(1, &s.vbo);
    glBindVertexArray(s.vao);
    glBindBuffer(GL_ARRAY_BUFFER, s.vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * 4, verts.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * 4, 0); glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return s;
}

struct Planet {
    std::string name;
    float orbitRadius, planetRadius, orbitSpeed, spinSpeed;
    float r, g, b;
    float orbitAngle, spinAngle, axialTilt, surfaceVariation;
    GLuint textureID;
    OrbitLines orbit;
};

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWwindow* window = glfwCreateWindow(1200, 800, "Sistema Solar 3D", NULL, NULL);
    if (!window) { std::cerr << "Error GLFW\n"; glfwTerminate(); return -1; }
    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, responsive);
    glfwSetCursorPosCallback(window, mouseCallback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    if (glewInit() != GLEW_OK) { std::cerr << "Error GLEW\n"; return -1; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);

    Shader shader("res/Shader/vertexShader.glsl", "res/Shader/fragmentShader.glsl");

    // Sphere geometry
    SphereMesh sphere = makeSphere(36, 36);
    unsigned int sphereVAO, sphereVBO, sphereEBO;
    glGenVertexArrays(1, &sphereVAO);
    glGenBuffers(1, &sphereVBO);
    glGenBuffers(1, &sphereEBO);
    glBindVertexArray(sphereVAO);
    glBindBuffer(GL_ARRAY_BUFFER, sphereVBO);
    glBufferData(GL_ARRAY_BUFFER, sphere.vertices.size() * 4, sphere.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, sphereEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sphere.indices.size() * 4, sphere.indices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * 4, 0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(3 * 4)); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * 4, (void*)(6 * 4)); glEnableVertexAttribArray(2);

    // Textures
    GLuint sunTex = makeSunTexture();

    // Stars
    StarField stars = makeStars(3000);

    // Saturn ring
    RingMesh saturnRing = makeRing(0.35f, 0.75f);
    GLuint ringTex = makeRingTexture();

    // Planets
    std::vector<Planet> planets = {
        {"Mercurio", 0.9f, 0.08f, 38.0f, 140.0f, 0.62f, 0.58f, 0.55f, 0, 0, 2.0f, 0.35f},
        {"Venus",    1.3f, 0.12f, 28.0f, 100.0f, 0.85f, 0.70f, 0.40f, 0, 0, 3.0f, 0.30f},
        {"Tierra",   1.8f, 0.13f, 22.0f, 220.0f, 0.20f, 0.45f, 0.85f, 0, 0, 23.4f,0.55f},
        {"Marte",    2.3f, 0.10f, 17.0f, 200.0f, 0.80f, 0.35f, 0.25f, 0, 0, 25.0f,0.45f},
        {"Jupiter",  3.1f, 0.30f, 9.0f,  300.0f, 0.80f, 0.60f, 0.45f, 0, 0, 3.1f, 0.95f},
        {"Saturno",  4.0f, 0.26f, 6.5f,  280.0f, 0.85f, 0.75f, 0.55f, 0, 0, 26.7f,0.90f},
        {"Urano",    4.8f, 0.18f, 4.5f,  190.0f, 0.55f, 0.80f, 0.85f, 0, 0, 97.8f,0.60f},
        {"Neptuno",  5.5f, 0.17f, 3.5f,  185.0f, 0.30f, 0.45f, 0.85f, 0, 0, 28.3f,0.65f},
    };

    for (size_t i = 0; i < planets.size(); i++) {
        planets[i].orbitAngle = i * 47.0f;
        planets[i].textureID = makePlanetTexture(
            glm::vec3(planets[i].r, planets[i].g, planets[i].b),
            planets[i].surfaceVariation, i * 73.0f);
        planets[i].orbit = makeOrbit(planets[i].orbitRadius);
    }

    glm::vec3 sunPos(0.0f);
    float sunRadius = 0.5f;

    while (!glfwWindowShouldClose(window)) {
        float cur = (float)glfwGetTime();
        deltaTime = cur - lastFrame; lastFrame = cur;
        userInput(window);

        for (auto& p : planets) {
            p.orbitAngle += p.orbitSpeed * timeScale * deltaTime;
            p.spinAngle  += p.spinSpeed  * timeScale * deltaTime;
            if (p.orbitAngle > 360.0f) p.orbitAngle -= 360.0f;
            if (p.spinAngle  > 360.0f) p.spinAngle  -= 360.0f;
        }

        glClearColor(0.0f, 0.0f, 0.02f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glm::mat4 view = camera.getViewMatrix();
        glm::mat4 proj = glm::perspective(glm::radians(60.0f), 1200.0f / 800.0f, 0.05f, 100.0f);

        shader.use();
        shader.setMat4("view", view);
        shader.setMat4("projection", proj);
        shader.setVec3("lightPos", sunPos);

        // --- Stars ---
        glBindVertexArray(stars.vao);
        glm::mat4 model = glm::mat4(1.0f);
        shader.setMat4("model", model);
        shader.setVec3("objectColor", glm::vec3(1.0f));
        shader.setBool("useTexture", false);
        shader.setBool("isLightSource", true);
        glDrawArrays(GL_POINTS, 0, stars.count);

        // --- Sun ---
        glBindVertexArray(sphereVAO);
        {
            float pulse = sunRadius * (1.0f + 0.04f * std::sin(cur * 2.0f));
            glm::mat4 m = glm::mat4(1.0f);
            m = glm::translate(m, sunPos);
            m = glm::scale(m, glm::vec3(pulse));
            shader.setMat4("model", m);
            shader.setVec3("objectColor", glm::vec3(1.0f, 0.75f, 0.15f));
            shader.setBool("useTexture", true);
            shader.setBool("isLightSource", true);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, sunTex);
            shader.setInt("planetTexture", 0);
            glDrawElements(GL_TRIANGLES, (GLsizei)sphere.indices.size(), GL_UNSIGNED_INT, 0);
        }

        // --- Planets ---
        shader.setBool("isLightSource", false);
        shader.setBool("useTexture", true);
        for (const auto& p : planets) {
            float rad = glm::radians(p.orbitAngle);
            float px = sunPos.x + p.orbitRadius * std::cos(rad);
            float pz = sunPos.z + p.orbitRadius * std::sin(rad);

            glm::mat4 m = glm::mat4(1.0f);
            m = glm::translate(m, glm::vec3(px, 0.0f, pz));
            m = glm::rotate(m, glm::radians(p.axialTilt), glm::vec3(0.0f, 0.0f, 1.0f));
            m = glm::rotate(m, glm::radians(p.spinAngle), glm::vec3(0.0f, 1.0f, 0.0f));
            m = glm::scale(m, glm::vec3(p.planetRadius));

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, p.textureID);
            shader.setInt("planetTexture", 0);
            shader.setMat4("model", m);
            shader.setVec3("objectColor", glm::vec3(p.r, p.g, p.b));
            glDrawElements(GL_TRIANGLES, (GLsizei)sphere.indices.size(), GL_UNSIGNED_INT, 0);

            // Saturn's rings
            if (p.name == "Saturno") {
                glm::mat4 rm = glm::mat4(1.0f);
                rm = glm::translate(rm, glm::vec3(px, 0.0f, pz));
                rm = glm::rotate(rm, glm::radians(26.7f), glm::vec3(1.0f, 0.0f, 0.0f));
                rm = glm::rotate(rm, glm::radians(p.spinAngle * 0.5f), glm::vec3(0.0f, 1.0f, 0.0f));
                shader.setMat4("model", rm);
                shader.setBool("isLightSource", true);
                shader.setBool("useTexture", true);
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, ringTex);
                shader.setInt("planetTexture", 0);
                glBindVertexArray(saturnRing.vao);
                glDrawArrays(GL_TRIANGLES, 0, saturnRing.count);
                shader.setBool("isLightSource", false);
                shader.setBool("useTexture", true);
                glBindVertexArray(sphereVAO);
            }
        }

        // --- Orbits ---
        shader.setBool("isLightSource", true);
        shader.setBool("useTexture", false);
        shader.setVec3("objectColor", glm::vec3(0.25f, 0.25f, 0.35f));
        for (const auto& p : planets) {
            glBindVertexArray(p.orbit.vao);
            shader.setMat4("model", glm::mat4(1.0f));
            glDrawArrays(GL_LINES, 0, p.orbit.count);
        }

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    glDeleteVertexArrays(1, &sphereVAO);
    glDeleteBuffers(1, &sphereVBO);
    glDeleteBuffers(1, &sphereEBO);
    for (const auto& p : planets) {
        glDeleteVertexArrays(1, &p.orbit.vao);
        glDeleteBuffers(1, &p.orbit.vbo);
        glDeleteTextures(1, &p.textureID);
    }
    glDeleteTextures(1, &sunTex);
    glDeleteTextures(1, &ringTex);
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void responsive(GLFWwindow*, int w, int h) { glViewport(0, 0, w, h); }

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) { lastX = (float)xpos; lastY = (float)ypos; firstMouse = false; }
    float xo = (float)xpos - lastX, yo = lastY - (float)ypos;
    lastX = (float)xpos; lastY = (float)ypos;
    camera.yaw += xo * camera.sensitivity;
    camera.pitch += yo * camera.sensitivity;
    if (camera.pitch > 89.0f) camera.pitch = 89.0f;
    if (camera.pitch < -89.0f) camera.pitch = -89.0f;
    camera.updateVectors();
}

void userInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    float v = camera.speed * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camera.position += camera.front * v;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camera.position -= camera.front * v;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        camera.position -= glm::normalize(glm::cross(camera.front, camera.up)) * v;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera.position += glm::normalize(glm::cross(camera.front, camera.up)) * v;
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) camera.position += camera.up * v;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) camera.position -= camera.up * v;
    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) timeScale += 0.5f * deltaTime * 5.0f;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) timeScale -= 0.5f * deltaTime * 5.0f;
    if (timeScale < 0.0f) timeScale = 0.0f;
}
