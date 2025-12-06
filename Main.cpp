#define STB_IMAGE_IMPLEMENTATION
#define _USE_MATH_DEFINES

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include <cmath>
#include <map>
#include <algorithm>
#include <ft2build.h>
#include "Util.h"
#include "stb_image.h"

#include FT_FREETYPE_H

GLFWwindow* window;
int screenWidth, screenHeight;
float aspect;

GLFWcursor* cursor;

const double targetFPS = 75.0f; 
auto targetFrameDuration = std::chrono::duration<double>(1.0f / targetFPS);

bool isCPressed = false;
bool isZPressed = false;
bool isKPressed = false;
bool isEnterPressed = false;

enum ItemType {
    ITEM_COIN,
    ITEM_GEM
};

struct Coin {
    float x, y;
    float size;
    unsigned int vao, vbo;
    float r, g, b;
    ItemType type;
};

struct Chest {
    float x, y;
    bool isOpen = false;
    unsigned int vao;
    unsigned int vbo;
    std::vector<Coin> contents;
};

Chest chest;

struct Bubble {
    float x, y;
    float speed = 0.4f;
    float radius = 0.005f;
    float r, g, b;
    unsigned int vao, vbo;
};

struct FishEye {
    unsigned int vao, vbo;
    float x, y;
    float radius = 0.003f;
    float r = 0.0f, g = 0.0f, b = 0.0f;
};

struct Fish {
    float x, y;
    float w;
    float h;
    float size;
    float r, g, b; 
    unsigned int vao, vbo;
    bool isClown;
    bool facingRight = true;
    std::vector<Bubble> bubbles;
    FishEye eye;
};

struct Stripe {
    unsigned int vao, vbo;
    float r, g, b;
    int numVertices;
};

Fish goldfish;
Fish clownfish;

struct FoodParticle {
    float x, y;
    float radius;
    float speed;
};

std::vector<FoodParticle> foodList;

struct Character {
    unsigned int TextureID;
    int SizeX, SizeY;
    int BearingX, BearingY;
    unsigned int Advance;
};

std::map<char, Character> Characters;

void loadFont(const char* fontPath, int fontSize)
{
    FT_Library ft;
    if (FT_Init_FreeType(&ft)) {
        std::cout << "ERROR: FreeType init failed" << std::endl;
        return;
    }

    FT_Face face;
    if (FT_New_Face(ft, fontPath, 0, &face)) {
        std::cout << "ERROR: Failed to load font" << std::endl;
        return;
    }

    FT_Set_Pixel_Sizes(face, 0, fontSize);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // byte alignment

    for (unsigned char c = 32; c < 128; c++)
    {
        if (FT_Load_Char(face, c, FT_LOAD_RENDER)) {
            std::cout << "Failed to load glyph " << c << std::endl;
            continue;
        }

        unsigned int tex;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RED,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            0, GL_RED, GL_UNSIGNED_BYTE,
            face->glyph->bitmap.buffer);

        GLint swizzleMask[] = { GL_ONE, GL_ONE, GL_ONE, GL_RED };
        glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        Character ch = {
            tex,
            face->glyph->bitmap.width,
            face->glyph->bitmap.rows,
            face->glyph->bitmap_left,
            face->glyph->bitmap_top,
            static_cast<unsigned int>(face->glyph->advance.x)
        };

        Characters.insert({ c, ch });
    }

    FT_Done_Face(face);
    FT_Done_FreeType(ft);
}

unsigned int loadTexture(const char* path) {
    unsigned int textureID;

    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &width, &height, &nrChannels, 0);
    if (data) {
        GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); // or GL_NEAREST
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); // or GL_NEAREST
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); // or GL_CLAMP_TO_EDGE
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); // or GL_CLAMP_TO_EDGE
    }
    else {
        std::cerr << "Failed to load texture: " << path << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

void createVAO(const std::vector<float>& vertices, unsigned int& vao, unsigned int& vbo)
{
    // Generiši i binduj VAO
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // Generiši i binduj VBO
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);

    // Popuni VBO podacima
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Atribut 0 = pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Odveži VAO i VBO
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void createTextVAO(unsigned int& vao, unsigned int& vbo)
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

std::vector<float> makeCircle(float cx, float cy, float r, int segments = 32)
{
    std::vector<float> verts;
    verts.reserve((segments + 2) * 2);

    // Centar kruga
    verts.push_back(cx);
    verts.push_back(cy);

    // Sve tacke okolo
    for (int i = 0; i <= segments; i++) {
        float angle = (float)i / segments * 2.0f * M_PI;
        float x = cx + cos(angle) * r;
        float y = cy + sin(angle) * r * aspect;
        verts.push_back(x);
        verts.push_back(y);
    }

    return verts;
}

void createAquarium(std::vector<float> &aquariumVertices)
{
    aquariumVertices.clear();

    aquariumVertices = {
        -1.0f, -1.0f,
         1.0f, -1.0f,
         1.0f,  0.1f,
        -1.0f,  0.1f
    };
}

void drawAquarium(unsigned int vao, unsigned int shader)
{
    glUseProgram(shader);
    glBindVertexArray(vao);
    int colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 0.2f); 
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void createAquariumBorder(std::vector<float>& aquariumBorderVertices)
{
    aquariumBorderVertices.clear();

    float thicknessPx = 10.0f;
    float thicknessW = thicknessPx / screenWidth * 2.0f;  
    float thicknessH = thicknessPx / screenHeight * 2.0f;

    float top = 0.1f; // gornja granica akvarijuma
    float bottom = -1.0f;
    float left = -1.0f;
    float right = 1.0f;

    // --- Donja ivica (2 trougla) ---
    std::vector<float> bottomQuad = {
        left, bottom,
        right, bottom,
        right, bottom + thicknessH,

        left, bottom,
        right, bottom + thicknessH,
        left, bottom + thicknessH
    };

    // --- Levi zid (2 trougla) ---
    std::vector<float> leftQuad = {
        left, bottom,
        left + thicknessW, bottom,
        left + thicknessW, top,

        left, bottom,
        left + thicknessW, top,
        left, top
    };

    // --- Desni zid (2 trougla) ---
    std::vector<float> rightQuad = {
        right - thicknessW, bottom,
        right, bottom,
        right, top,

        right - thicknessW, bottom,
        right, top,
        right - thicknessW, top
    };

    // Kombinujemo sve vertekse
    aquariumBorderVertices.insert(aquariumBorderVertices.end(), bottomQuad.begin(), bottomQuad.end());
    aquariumBorderVertices.insert(aquariumBorderVertices.end(), leftQuad.begin(), leftQuad.end());
    aquariumBorderVertices.insert(aquariumBorderVertices.end(), rightQuad.begin(), rightQuad.end());
}

void drawAquariumBorder(unsigned int vao, unsigned int shader)
{
    glUseProgram(shader);
    glBindVertexArray(vao);
    int colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 18);
    glBindVertexArray(0);
}

void createSand(std::vector<float>& aquariumVertices)
{
    aquariumVertices.clear();

    aquariumVertices = {
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  0.1f, 1.0f, 1.0f,
        -1.0f,  0.1f, 0.0f, 1.0f
    };
}

void createSandVAO(const std::vector<float>& vertices, unsigned int& vao, unsigned int& vbo)
{
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    // Atribut 0 = pozicija (x, y)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // Atribut 1 = texture coordinates (u, v)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void drawSand(unsigned int vao, unsigned int shader, unsigned int texture)
{
    glUseProgram(shader);
    glBindVertexArray(vao);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    glBindVertexArray(0);
}

void createSeaweed(std::vector<float>& vertices, float x0, float y0, float height, int numLeaves, int segments, float maxOffset)
{
    vertices.clear();

    for (int l = 0; l < numLeaves; ++l) {

        float leafBaseOffset = ((float)l / (numLeaves - 1) - 0.5f) * maxOffset; // osnovna horizontalna pozicija lista
        float phase = ((float)rand() / RAND_MAX) * 2.0f * M_PI; // nasumična faza za svaki list
        float heightFactor = 0.8f + ((float)rand() / RAND_MAX) * 0.4f; // random visina od 0.8 do 1.2 puta osnovna visina
        float yOffset = ((float)rand() / RAND_MAX - 0.5f) * 0.05f * height; // random donji pomak 

        for (int i = 0; i < segments; ++i) {
            float t = (float)i / (segments - 1);
            // x koordinata: osnovna offset + sinus za lelujanje + mala random devijacija
            float sway = 0.02f * sin(t * M_PI + phase);
            float jitter = ((float)rand() / RAND_MAX - 0.5f) * 0.01f; // mala random devijacija
            float x = x0 + leafBaseOffset + sway + jitter;
            float y = y0 + yOffset + t * height * heightFactor;

            vertices.push_back(x);
            vertices.push_back(y);
        }
    }
}

void drawSeaweed(unsigned int vao, unsigned int shader, size_t seaweedSize, float r = 0.2f, float g = 0.4f, float b = 0.1f)
{
    glUseProgram(shader);
    int colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform4f(colorLoc, r, g, b, 1.0f); 
    glBindVertexArray(vao);
    glDrawArrays(GL_LINE_STRIP, 0, (GLsizei)(seaweedSize / 5));
    glBindVertexArray(0);
}

void createChest() 
{
    float x = -0.7f;
    float y = -0.9f;
    float width = 0.1f;
    float height = 0.15f;

    chest.x = x;
    chest.y = y;

    std::vector<float> vertices = {
        x,        y,
        x + width,  y,
        x + width,  y + height,
        x,        y + height
    };

    glGenVertexArrays(1, &chest.vao);
    glBindVertexArray(chest.vao);

    glGenBuffers(1, &chest.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, chest.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), vertices.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void drawChest(unsigned int shader)
{
    glUseProgram(shader);
    glBindVertexArray(chest.vao);

    int colorLoc = glGetUniformLocation(shader, "uColor");
    int offsetLoc = glGetUniformLocation(shader, "uOffset");

    if (chest.isOpen) {
        glUniform4f(colorLoc, 0.6f, 0.3f, 0.0f, 1.0f);
    }
    else {
        glUniform4f(colorLoc, 0.3f, 0.15f, 0.0f, 1.0f); 
    }
    glUniform2f(offsetLoc, 0.0f, 0.0f);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if (chest.isOpen) {
        glUniform4f(colorLoc, 0.6f, 0.3f, 0.0f, 1.0f); 
        glUniform2f(offsetLoc, 0.0f, 0.15f);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    glBindVertexArray(0);
}

void createChestBorder(std::vector<float>& chestBorderVertices)
{
    chestBorderVertices.clear();

    float thickness = 0.01f;

    float top = -0.75f; 
    float bottom = -0.9f;
    float left = -0.7;
    float right = -0.6f;

    // --- Donja ivica (2 trougla) ---
    std::vector<float> bottomQuad = {
        left, bottom,
        right, bottom,
        right, bottom + thickness,

        left, bottom,
        right, bottom + thickness,
        left, bottom + thickness
    };

    // --- Gornja ivica (2 trougla) ---
    std::vector<float> topQuad = {
        left, top - thickness,
        right, top - thickness,
        right, top,

        left, top - thickness,
        right, top,
        left, top
    };

    // --- Levi zid (2 trougla) ---
    std::vector<float> leftQuad = {
        left, bottom,
        left + thickness, bottom,
        left + thickness, top,

        left, bottom,
        left + thickness, top,
        left, top
    };

    // --- Desni zid (2 trougla) ---
    std::vector<float> rightQuad = {
        right - thickness, bottom,
        right, bottom,
        right, top,

        right - thickness, bottom,
        right, top,
        right - thickness, top
    };

    // Kombinujemo sve vertekse
    chestBorderVertices.insert(chestBorderVertices.end(), bottomQuad.begin(), bottomQuad.end());
    chestBorderVertices.insert(chestBorderVertices.end(), topQuad.begin(), topQuad.end());
    chestBorderVertices.insert(chestBorderVertices.end(), leftQuad.begin(), leftQuad.end());
    chestBorderVertices.insert(chestBorderVertices.end(), rightQuad.begin(), rightQuad.end());
}

void drawChestBorder(unsigned int vao, unsigned int shader)
{
    glUseProgram(shader);
    glBindVertexArray(vao);
    int colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform4f(colorLoc, 0.0f, 0.0f, 0.0f, 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 24);
    glBindVertexArray(0);
}

void createChestContents() {
    chest.contents.clear();

    float size = 0.011f;

    // 3 zlatna novčića
    std::vector<std::tuple<float, float, float, float, float>> coinData = {
        {chest.x + 0.03f, chest.y + 0.035f, 1.0f, 0.84f, 0.0f},
        {chest.x + 0.07f, chest.y + 0.04f, 1.0f, 0.84f, 0.0f},
        {chest.x + 0.05f, chest.y + 0.07f, 1.0f, 0.84f, 0.0f}
    };

    for (auto& d : coinData) {
        Coin c;
        c.x = std::get<0>(d);
        c.y = std::get<1>(d);
        c.size = size;
        c.r = std::get<2>(d);
        c.g = std::get<3>(d);
        c.b = std::get<4>(d);
        c.type = ITEM_COIN;

        std::vector<float> vertices = makeCircle(c.x, c.y, c.size);

        createVAO(vertices, c.vao, c.vbo);

        chest.contents.push_back(c);
    }

    Coin gem;
    gem.x = chest.x + 0.07f;
    gem.y = chest.y + 0.11f;
    gem.size = size * 2.5f;
    gem.r = 1.0f;
    gem.g = 0.0f;
    gem.b = 0.0f;
    gem.type = ITEM_GEM;

    float topX = gem.x;
    float topY = gem.y + gem.size;

    float rightX = gem.x + gem.size * 0.4f;
    float rightY = gem.y;

    float bottomX = gem.x;
    float bottomY = gem.y - gem.size;

    float leftX = gem.x - gem.size * 0.4f;
    float leftY = gem.y;

    std::vector<float> verts;

    verts.push_back(gem.x); verts.push_back(gem.y);

    verts.push_back(topX);    verts.push_back(topY);
    verts.push_back(rightX);  verts.push_back(rightY);
    verts.push_back(bottomX); verts.push_back(bottomY);
    verts.push_back(leftX);   verts.push_back(leftY);

    verts.push_back(topX);    verts.push_back(topY);

    createVAO(verts, gem.vao, gem.vbo);

    chest.contents.push_back(gem);
}

void drawChestContents(unsigned int shader) {
    if (!chest.isOpen) return; // crtamo samo kada je otvoren

    glUseProgram(shader);

    for (auto& c : chest.contents) {
        glBindVertexArray(c.vao);
        int colorLoc = glGetUniformLocation(shader, "uColor");
        glUniform4f(colorLoc, c.r, c.g, c.b, 1.0f);
        if (c.type == ITEM_COIN) {
            glDrawArrays(GL_TRIANGLE_FAN, 0, 34);
        }
        else if (c.type == ITEM_GEM) {
            glDrawArrays(GL_TRIANGLE_FAN, 0, 6);
        }
    }

    glBindVertexArray(0);
}

std::vector<float> makeFishBody(float cx, float cy, float w, float h, float &r, float &g, float &b, bool isClown=false)
{
    std::vector<float> verts;

    if (isClown) {
        r = 1.0f;  
        g = 0.55f; 
        b = 0.0f;  
    }
    else {
        r = 1.0f;  
        g = 0.84f;
        b = 0.0f;
    }

    // --- Telo (elipsa) ---
    int segments = 20;
    verts.push_back(0.0f); verts.push_back(0.0f); // centar
    for (int i = 0; i <= segments; i++) {
        float a = (float)i / segments * 2.0f * M_PI;
        float x = cos(a) * w * 0.4f;
        float y = sin(a) * h * 0.6f;
        verts.push_back(x);
        verts.push_back(y);
    }

    // rep (trougao)
    float tailLength = w * 0.4f;
    float tailHeight = h * 0.5f;
    verts.push_back(-w * 0.6f); verts.push_back(0.0f);
    verts.push_back(-w * 0.6f - tailLength); verts.push_back(tailHeight);
    verts.push_back(-w * 0.6f - tailLength); verts.push_back(-tailHeight);

    return verts;
}

std::vector<float> makeClownStripe(float stripeXOffset, float stripeWidth, float bodyWidth, float bodyHeight)
{
    std::vector<float> verts;
    int segments = 20;

    float halfBodyW = bodyWidth * 0.4f;
    float halfBodyH = bodyHeight * 0.6f;
    float halfStripeW = stripeWidth * 0.5f;

    float leftX = stripeXOffset - halfStripeW;
    float rightX = stripeXOffset + halfStripeW;

    for (int i = 0; i <= segments; i++) {
        float t = (float)i / segments;
        float xLocal = leftX + t * (rightX - leftX);

        float dx = std::max(-halfBodyW, std::min(halfBodyW, xLocal));

        float dy = halfBodyH * sqrtf(1.0f - (dx * dx) / (halfBodyW * halfBodyW));

        float topY = dy;
        float bottomY = -dy;

        verts.push_back(xLocal);
        verts.push_back(bottomY);

        verts.push_back(xLocal);
        verts.push_back(topY);
    }

    return verts;
}

void createFishes(Stripe &stripe1, Stripe &stripe2) {
    // GOLD FISH
    goldfish.x = -0.5f;
    goldfish.y = -0.2f;
    goldfish.w = 0.07f * 1.5f;
    goldfish.h = 0.07f * 1.0f;
    goldfish.size = 1.0f;
    goldfish.isClown = false;

    std::vector<float> goldVerts = makeFishBody(goldfish.x, goldfish.y, goldfish.w, goldfish.h, goldfish.r, goldfish.g, goldfish.b);
    createVAO(goldVerts, goldfish.vao, goldfish.vbo);
    goldfish.eye.x = goldfish.w * 0.25f;
    goldfish.eye.y = goldfish.h * 0.13f * goldfish.size;
    createVAO(makeCircle(goldfish.eye.x, goldfish.eye.y, goldfish.eye.radius), goldfish.eye.vao, goldfish.eye.vbo);

    // CLOWN FISH (narandžasto-bela)
    clownfish.x = 0.7f;
    clownfish.y = -0.3f;
    clownfish.w = 0.06f * 1.5f;
    clownfish.h = 0.06f * 1.0f;
    clownfish.size = 1.0f;
    clownfish.isClown = true;

    std::vector<float> clownVerts = makeFishBody(clownfish.x, clownfish.y, clownfish.w, clownfish.h, clownfish.r, clownfish.g, clownfish.b, clownfish.isClown);
    createVAO(clownVerts, clownfish.vao, clownfish.vbo);
    clownfish.eye.x = clownfish.w * 0.25f;
    clownfish.eye.y = clownfish.h * 0.13f * clownfish.size;
    createVAO(makeCircle(clownfish.eye.x, clownfish.eye.y, clownfish.eye.radius), clownfish.eye.vao, clownfish.eye.vbo);

    stripe1.r = 1.0f;
    stripe1.g = 1.0f;
    stripe1.b = 1.0f;

    float stripe1Offset = -clownfish.w * 0.20f;
    std::vector<float> stripe1Verts = makeClownStripe(stripe1Offset, 0.01f, clownfish.w, clownfish.h);
    createVAO(stripe1Verts, stripe1.vao, stripe1.vbo);
    stripe1.numVertices = stripe1Verts.size() / 2;

    stripe2.r = 1.0f;
    stripe2.g = 1.0f;
    stripe2.b = 1.0f;

    float stripe2Offset = clownfish.w * 0.12f;
    std::vector<float> stripe2Verts = makeClownStripe(stripe2Offset, 0.01f, clownfish.w, clownfish.h);
    createVAO(stripe2Verts, stripe2.vao, stripe2.vbo);
    stripe2.numVertices = stripe2Verts.size() / 2;
}

void drawStripe(const Stripe& stripe, unsigned shader, float fishX, float fishY) {
    glUseProgram(shader);
    glBindVertexArray(stripe.vao);

    int colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform4f(colorLoc, stripe.r, stripe.g, stripe.b, 1.0f);

    int offsetLoc = glGetUniformLocation(shader, "uOffset");
    glUniform2f(offsetLoc, fishX, fishY);

    int scaleXLoc = glGetUniformLocation(shader, "uScaleX");
    glUniform1f(scaleXLoc, clownfish.facingRight ? 1.0f : -1.0f);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, stripe.numVertices);
    glBindVertexArray(0);
}

void drawFish(const Fish& f, unsigned shader) {
    glUseProgram(shader);
    glBindVertexArray(f.vao);

    int colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform4f(colorLoc, f.r, f.g, f.b, 1.0f);

    int scaleLoc = glGetUniformLocation(shader, "uScaleX");
    glUniform1f(scaleLoc, f.facingRight ? 1.0f : -1.0f);

    int offsetLoc = glGetUniformLocation(shader, "uOffset");
    glUniform2f(offsetLoc, f.x, f.y);

    int sizeLoc = glGetUniformLocation(shader, "uSize");
    glUniform1f(sizeLoc, f.size);

    glDrawArrays(GL_TRIANGLE_FAN, 0, 24);
    glBindVertexArray(0);
}

void drawFishEye(const Fish& f, unsigned int shader) {
    glUseProgram(shader);

    glUniform2f(glGetUniformLocation(shader, "uOffset"), f.x, f.y);
    glUniform4f(glGetUniformLocation(shader, "uColor"), f.eye.r, f.eye.g, f.eye.b, 1.0f);
    glUniform1f(glGetUniformLocation(shader, "uScaleX"), f.facingRight ? 1.0f : -1.0f);
    glUniform1f(glGetUniformLocation(shader, "uScaleY"), 1.0f);

    glBindVertexArray(f.eye.vao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 34);
    glBindVertexArray(0);
}

void clampFishPosition(Fish& f) 
{
    float left = -0.995f;
    float right = 0.995f;
    float bottom = -0.98f;
    float top = 0.09f;

    // Sirina/visina ribe zbog okretanja (scaleX ne menja ovo)
    float halfW = f.w * 0.5f;
    float halfH = f.h * f.size * 0.5f;

    // Clamp X
    if (f.x < left + halfW)
        f.x = left + halfW;
    if (f.x > right - halfW)
        f.x = right - halfW;

    // Clamp Y
    if (f.y < bottom + halfH)
        f.y = bottom + halfH;
    if (f.y > top - halfH)
        f.y = top - halfH;
}

void spawnBubbles(Fish& f) 
{
    float bodyLength = f.w;
    float bubbleOffsetX = f.facingRight ? 0.05f : -0.05f;

    for (int i = 0; i < 3; i++) {
        Bubble b;
        float xShift = i * 0.01f * (f.facingRight ? 1.0f : -1.0f);
        b.x = f.x + bubbleOffsetX + xShift;
        b.y = f.y + i * i * 0.01f;
        b.r = 0.9f;
        b.g = 0.95f;
        b.b = 1.0f;

        std::vector<float> vertices = makeCircle(0, 0, b.radius);
        createVAO(vertices, b.vao, b.vbo);
        f.bubbles.push_back(b);
    }
}

void updateBubbles(Fish& f, float dt)
{
    for (auto& b : f.bubbles) {
        b.y += b.speed * dt * 0.3f;
    }

    f.bubbles.erase(
        std::remove_if(f.bubbles.begin(), f.bubbles.end(),
            [](const Bubble& b) {
                return b.y >  0.1f; 
            }),
        f.bubbles.end()
    );
}

void drawBubble(const Bubble& b, unsigned shader) {
    glUseProgram(shader);

    glUniform2f(glGetUniformLocation(shader, "uOffset"), b.x, b.y);
    glUniform4f(glGetUniformLocation(shader, "uColor"), b.r, b.g, b.b, 1.0f);
    glUniform1f(glGetUniformLocation(shader, "uScaleX"), 1.0f);
    glUniform1f(glGetUniformLocation(shader, "uScaleY"), 1.0f);

    glBindVertexArray(b.vao); 
    glDrawArrays(GL_TRIANGLE_FAN, 0, 34);
    glBindVertexArray(0);
}

void initFoodVAO(unsigned int& foodVao, unsigned int& foodVbo, float radius)
{
    std::vector<float> verts = makeCircle(0, 0, radius);
    createVAO(verts, foodVao, foodVbo);
}

void spawnFood(std::vector<FoodParticle>& foodList)
{
    const float minDist = 0.05f;
    const int count = 6;

    std::vector<float> newXs; 

    for (int i = 0; i < count; i++) {
        FoodParticle p;

        bool good = false;
        int attempts = 0;
        const int maxAttempts = 100;

        while (!good && attempts < maxAttempts) {
            attempts++;
            good = true;

            p.x = -0.8f + static_cast<float>(rand()) / RAND_MAX * 1.6f;

            for (float xExisting : newXs) {
                if (fabs(xExisting - p.x) < minDist) {
                    good = false;
                    break;
                }
            }
        }

        if (!good) continue; 

        newXs.push_back(p.x);     
        p.y = 1.05f + ((float)rand() / RAND_MAX) * 0.1f;
        p.speed = 1.0f;
        p.radius = 0.05f;         

        foodList.push_back(p);
    }
}

void updateFood(std::vector<FoodParticle>& foodList, float dt)
{
    const float sandLevel = -0.7f;

    for (auto& p : foodList) {
        if (p.y > sandLevel)
            p.y -= p.speed * dt * 0.3f;
    }
}

void drawFood(const FoodParticle& p, unsigned shader, unsigned int foodVao)
{
    glUseProgram(shader);

    glUniform2f(glGetUniformLocation(shader, "uOffset"), p.x, p.y);
    glUniform4f(glGetUniformLocation(shader, "uColor"), 0.45f, 0.32f, 0.12f, 1.0f);
    glUniform1f(glGetUniformLocation(shader, "uScaleX"), 1.0f);
    glUniform1f(glGetUniformLocation(shader, "uScaleY"), 1.0f);

    glBindVertexArray(foodVao);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 34);
    glBindVertexArray(0);
}

bool fishEats(const Fish& f, const FoodParticle& p)
{
    float rx = f.w * 0.4f; 
    float ry = f.h * 0.6f * f.size;       

    float dx = p.x - f.x;
    float dy = p.y - f.y;

    float value = (dx * dx) / (rx * rx) + (dy * dy) / (ry * ry);

    return value <= 1.0f;
}

void checkFoodEating(Fish& fish, std::vector<FoodParticle>& foodList)
{
    foodList.erase(
        std::remove_if(foodList.begin(), foodList.end(),
            [&](const FoodParticle& p) {
                if (fishEats(fish, p)) {
                    fish.size += 0.01f;   
                    return true;          
                }
                return false;
            }),
        foodList.end()
    );
}

void createAnchorVertices(std::vector<float>& V)
{
    V.clear();

    // --- Parametri ---
    const float stemHeight = 0.1f;   // visina stem-a
    const float hookSize = 0.03f;    // duzina stranice trougla (vrh kuka)

    // Centar stem-a
    float cx = 0.0f;
    float cyTop = 0.0f;         // vrh stem-a
    float cyBottom = -stemHeight; // dno stem-a

    // Stem
    V.push_back(cx); V.push_back(cyTop);
    V.push_back(cx); V.push_back(cyBottom);

    float dxLeft = cx - stemHeight / 5.0f;
    float dxRight = cx + stemHeight / 5.0f;
    float dy = cyTop - 0.02f;

    V.push_back(dxLeft); V.push_back(dy);
    V.push_back(dxRight); V.push_back(dy);

    // Sredina stem-a
    float cyMid = (cyTop + cyBottom) / 2.0f;
    float hxLeft = cx - stemHeight / 2.0f;
    float hxRight = cx + stemHeight / 2.0f;

    /*V.push_back(hxLeft);  V.push_back(cyMid);
    V.push_back(hxRight); V.push_back(cyMid);*/

    // Kuke
    float h = hookSize * sqrt(3.0f) / 2.0f; // visina trougla

    // Levi vrh
    float topXL = hxLeft;
    float topYL = cyMid;

    float baseYL = topYL - h;
    float baseX1L = topXL - hookSize / 2.0f;
    float baseX2L = topXL + hookSize / 2.0f;

    V.push_back(topXL);    V.push_back(topYL);
    V.push_back(baseX1L);  V.push_back(baseYL);
    V.push_back(baseX2L);  V.push_back(baseYL);

    // Desni vrh
    float topXR = hxRight;
    float topYR = cyMid;

    float baseYR = topYR - h;
    float baseX1R = topXR - hookSize / 2.0f;
    float baseX2R = topXR + hookSize / 2.0f;

    V.push_back(topXR);    V.push_back(topYR);
    V.push_back(baseX1R);  V.push_back(baseYR);
    V.push_back(baseX2R);  V.push_back(baseYR);

    // Luk
    float arcCenterX = (topXL + topXR) / 2.0f;
    float arcCenterY = baseYL;
    float a = ((baseX1R + baseX2R) / 2.0f - (baseX1L + baseX2L) / 2.0f) / 2.0f;
    float b = cyMid - baseYL;

    int arcSegments = 20;
    float prevX = arcCenterX + a * cos(M_PI);  // počinjemo sa leve strane
    float prevY = arcCenterY + b * sin(M_PI);

    for (int i = 1; i <= arcSegments; i++)
    {
        float t = M_PI + (2.0f * M_PI - M_PI) * float(i) / arcSegments; // od 180° do 360°
        float x = arcCenterX + a * cos(t);
        float y = arcCenterY + b * sin(t);

        V.push_back(prevX); V.push_back(prevY);  
        V.push_back(x);     V.push_back(y);

        prevX = x;
        prevY = y;
    }
}

void drawAnchor(unsigned int vao, unsigned shader, float x, float y)
{
    glUseProgram(shader);
    glBindVertexArray(vao);

    int colorLoc = glGetUniformLocation(shader, "uColor");
    glUniform4f(colorLoc, 1.0f, 1.0f, 1.0f, 1.0f);

    int offsetLoc = glGetUniformLocation(shader, "uOffset");
    glUniform2f(offsetLoc, x, y);

    int scaleLoc = glGetUniformLocation(shader, "uScale");
    glUniform1f(scaleLoc, 0.5f);

    int angleLoc = glGetUniformLocation(shader, "uAngle");
    glUniform1f(angleLoc, 45.0f * M_PI / 180.0f);

    int aspectLoc = glGetUniformLocation(shader, "uAspect");
    glUniform1f(aspectLoc, aspect);

    int lineVertexCount = 4;
    int arcVertexCount = 40;
    int triangleVertexCount = 6;

    glDrawArrays(GL_LINES, 0, lineVertexCount);

    glDrawArrays(GL_TRIANGLES, lineVertexCount, triangleVertexCount);

    glDrawArrays(GL_LINES, lineVertexCount + triangleVertexCount, arcVertexCount);

    glBindVertexArray(0);
}

void RenderText(unsigned int shader, unsigned int vao, unsigned int vbo,
    std::string text, float x, float y, float scale,
    float screenWidth, float screenHeight)
{
    glUseProgram(shader);

    // Projekcija: (0,0) donji-levo, (screenWidth,screenHeight) gore-desno
    float projection[16] = {
        2.0f / screenWidth, 0, 0, 0,
        0, 2.0f / screenHeight, 0, 0,
        0, 0, -1, 0,
        -1, -1, 0, 1
    };
    glUniformMatrix4fv(glGetUniformLocation(shader, "uProjection"), 1, GL_FALSE, projection);

    float opacity = 0.5f; 
    glUniform1f(glGetUniformLocation(shader, "textAlpha"), opacity);
    glUniform3f(glGetUniformLocation(shader, "textColor"), 1.0f, 1.0f, 1.0f); // belo
    glActiveTexture(GL_TEXTURE0);
    glBindVertexArray(vao);

    for (char c : text)
    {
        if (Characters.find(c) == Characters.end()) continue;
        Character ch = Characters[c];

        float xpos = x + ch.BearingX * scale;
        float ypos = y + (ch.BearingY - ch.SizeY) * scale;
        float w = ch.SizeX * scale;
        float h = ch.SizeY * scale;

        float quad[6][4] = {
            {xpos,     ypos + h, 0.0f, 0.0f},
            {xpos,     ypos,     0.0f, 1.0f},
            {xpos + w, ypos,     1.0f, 1.0f},

            {xpos,     ypos + h, 0.0f, 0.0f},
            {xpos + w, ypos,     1.0f, 1.0f},
            {xpos + w, ypos + h, 1.0f, 0.0f}
        };

        glBindTexture(GL_TEXTURE_2D, ch.TextureID);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(quad), quad);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        x += (ch.Advance >> 6) * scale;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void getMonitorResolution(int& width, int& height)
{
    GLFWmonitor* primaryMonitor = glfwGetPrimaryMonitor();
    const GLFWvidmode* videoMode = glfwGetVideoMode(primaryMonitor);
    width = videoMode->width;
    height = videoMode->height;
    aspect = (float) width / height;
}

int endProgram(std::string message) {
    std::cout << message << std::endl;
    glfwTerminate();
    return -1;
}

void processInput(Fish &goldfish, Fish &clownfish, float deltaTime, unsigned int &foodVao, unsigned int &foodVbo)
{
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, true);
    }

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
        if (!isCPressed)
        {
            chest.isOpen = !chest.isOpen;
            isCPressed = true;
        }
    }

    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_RELEASE) {
        isCPressed = false;
    }

    float speed = deltaTime * 0.3f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) goldfish.y += speed;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) goldfish.y -= speed;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) { goldfish.x -= speed; goldfish.facingRight = false; }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) { goldfish.x += speed; goldfish.facingRight = true; }
    clampFishPosition(goldfish);

    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
        if (!isZPressed) {
            spawnBubbles(goldfish);
            isZPressed = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_RELEASE) {
        isZPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) clownfish.y += speed;
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) clownfish.y -= speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) { clownfish.x -= speed; clownfish.facingRight = false; }
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) { clownfish.x += speed; clownfish.facingRight = true; }
    clampFishPosition(clownfish);

    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_PRESS) {
        if (!isKPressed) {
            spawnBubbles(clownfish);
            isKPressed = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_K) == GLFW_RELEASE) {
        isKPressed = false;
    }

    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_PRESS) {
        if (!isEnterPressed) {
            spawnFood(foodList);
            isEnterPressed = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_ENTER) == GLFW_RELEASE) {
        isEnterPressed = false;
    }
}

int main()
{
    // Inicijalizacija GLFW i postavljanje na verziju 3 sa programabilnim pajplajnom
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Formiranje prozora za prikaz sa datim dimenzijama i naslovom
    getMonitorResolution(screenWidth, screenHeight);
    window = glfwCreateWindow(screenWidth, screenHeight, "Akvarijum", glfwGetPrimaryMonitor(), NULL);
    if (window == NULL) return endProgram("Prozor nije uspeo da se kreira.");
    glfwMakeContextCurrent(window);

    // Inicijalizacija GLEW
    if (glewInit() != GLEW_OK) return endProgram("GLEW nije uspeo da se inicijalizuje.");

    // Potrebno naglasiti da program koristi alfa kanal za providnost
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glViewport(0, 0, screenWidth, screenHeight);

    //glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    cursor = loadImageToCursor("cursor.png");
    glfwSetCursor(window, cursor);

    // Vsync isključeno (mi radimo frame limit)
    glfwSwapInterval(0);

    unsigned int basicShader = createShader("basic.vert", "basic.frag");
    unsigned int sandShader = createShader("sand.vert", "sand.frag");
    unsigned int chestShader = createShader("chest.vert", "chest.frag");
    unsigned int fishShader = createShader("fish.vert", "fish.frag");
    unsigned int objectShader = createShader("object.vert", "object.frag");
    unsigned int cursorShader = createShader("cursor.vert", "cursor.frag");
    unsigned int textShader = createShader("text.vert", "text.frag");

    std::vector<float> aquariumVertices;
    createAquarium(aquariumVertices);
    unsigned int aquariumVao, aquariumVbo;
    createVAO(aquariumVertices, aquariumVao, aquariumVbo);

    std::vector<float> aquariumBorderVertices;
    createAquariumBorder(aquariumBorderVertices);
    unsigned int aquariumBorderVao, aquariumBorderVbo;
    createVAO(aquariumBorderVertices, aquariumBorderVao, aquariumBorderVbo);

    std::vector<float> sandVertices;
    createSand(sandVertices);
    GLuint sandTexture = loadTexture("sand.png");
    unsigned int sandVao, sandVbo;
    createSandVAO(sandVertices, sandVao, sandVbo);

    std::vector<float> seaweed1Vertices, seaweed2Vertices;
    createSeaweed(seaweed1Vertices, -0.5f, -0.6f, 0.5f, 50, 50, 0.04f);
    createSeaweed(seaweed2Vertices, 0.3f, -0.7f, 0.6f, 50, 50, 0.05f);
    unsigned int seaweed1Vao, seaweed1Vbo;
    createVAO(seaweed1Vertices, seaweed1Vao, seaweed1Vbo);
    unsigned int seaweed2Vao, seaweed2Vbo;
    createVAO(seaweed2Vertices, seaweed2Vao, seaweed2Vbo);

    createChest();

    std::vector<float> chestBorderVertices;
    createChestBorder(chestBorderVertices);
    unsigned int chestBorderVao, chestBorderVbo;
    createVAO(chestBorderVertices, chestBorderVao, chestBorderVbo);

    createChestContents();

    Stripe stripe1Vertices, stripe2Vertices;
    createFishes(stripe1Vertices, stripe2Vertices);

    unsigned int foodVao;
    unsigned int foodVbo;
    initFoodVAO(foodVao, foodVbo, 0.005f);

    std::vector<float> anchorVertices;
    createAnchorVertices(anchorVertices);
    unsigned int anchorVao, anchorVbo;
    createVAO(anchorVertices, anchorVao, anchorVbo);

    loadFont("Arial.ttf", 36);
    unsigned int textVao, textVbo;
    createTextVAO(textVao, textVbo);

    /*int fpsCounter = 0;
    float fpsTimer = 0.0f;*/

    auto previous = std::chrono::high_resolution_clock::now();

    while (!glfwWindowShouldClose(window))
    {
        auto now = std::chrono::high_resolution_clock::now();
        float deltaTime = std::chrono::duration<float>(now - previous).count();
        previous = now;

        /*fpsCounter++;
        fpsTimer += deltaTime;
        if (fpsTimer >= 1.0f) {
            std::cout << "FPS: " << fpsCounter << std::endl;
            fpsCounter = 0;
            fpsTimer = 0.0f;
        }*/

        glClearColor(0.53f, 0.81f, 0.98f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        processInput(goldfish, clownfish, deltaTime, foodVao, foodVbo);

        drawSand(sandVao, sandShader, sandTexture);

        drawSeaweed(seaweed1Vao, basicShader, seaweed1Vertices.size());
        drawSeaweed(seaweed2Vao, basicShader, seaweed2Vertices.size());

        drawChest(chestShader);
        drawChestBorder(chestBorderVao, basicShader);
        drawChestContents(basicShader);

        updateBubbles(goldfish, deltaTime);
        updateBubbles(clownfish, deltaTime);

        for (auto& b : goldfish.bubbles)
            drawBubble(b, objectShader);

        for (auto& b : clownfish.bubbles)
            drawBubble(b, objectShader);

        updateFood(foodList, deltaTime);

        for (auto& p : foodList)
            drawFood(p, objectShader, foodVao);

        checkFoodEating(goldfish, foodList);
        checkFoodEating(clownfish, foodList);

        drawFish(goldfish, fishShader);
        drawFishEye(goldfish, objectShader);
        drawFish(clownfish, fishShader);
        drawStripe(stripe1Vertices, fishShader, clownfish.x, clownfish.y);
        drawStripe(stripe2Vertices, fishShader, clownfish.x, clownfish.y);
        drawFishEye(clownfish, objectShader);

        drawAquarium(aquariumVao, basicShader);
        drawAquariumBorder(aquariumBorderVao, basicShader);

        /*double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);
        float ndcX = (mouseX / screenWidth) * 2.0f - 1.0f;
        float ndcY = 1.0f - (mouseY / screenHeight) * 2.0f;
        drawAnchor(anchorVao, cursorShader, ndcX, ndcY);*/

        std::string name = "Andrija Slovic SV12/2021";
        float scale = 1.0f;
        float textX = 20.0f;
        float textY = screenHeight - 50.0f; 

        RenderText(textShader, textVao, textVbo, name, textX, textY, scale, screenWidth, screenHeight);

        glfwSwapBuffers(window); // Zamena bafera - prednji i zadnji bafer se menjaju kao štafeta; dok jedan procesuje, drugi se prikazuje.
        glfwPollEvents(); // Sinhronizacija pristiglih događaja

        auto frameTime = std::chrono::high_resolution_clock::now() - now;
        if (frameTime < targetFrameDuration) {
            std::this_thread::sleep_for(targetFrameDuration - frameTime);
        }
    }

    glDeleteVertexArrays(1, &aquariumVao);
    glDeleteVertexArrays(1, &aquariumBorderVao);
    glDeleteVertexArrays(1, &sandVao);
    glDeleteVertexArrays(1, &chest.vao);
    for (int i = 0; i < chest.contents.size(); i++)
    {
        glDeleteVertexArrays(1, &chest.contents[i].vao);
    }
    glDeleteVertexArrays(1, &goldfish.vao);
    glDeleteVertexArrays(1, &clownfish.vao);

    glDeleteBuffers(1, &aquariumVbo);
    glDeleteBuffers(1, &aquariumBorderVbo);
    glDeleteBuffers(1, &sandVbo);
    glDeleteBuffers(1, &chest.vbo);
    for (int i = 0; i < chest.contents.size(); i++)
    {
        glDeleteBuffers(1, &chest.contents[i].vbo);
    }
    glDeleteBuffers(1, &goldfish.vbo);
    glDeleteBuffers(1, &clownfish.vbo);

    glDeleteProgram(basicShader);
    glDeleteProgram(sandShader);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}