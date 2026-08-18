#pragma once

#include <string>
#include <unordered_map>
#include <windows.h>
#include <GL/gl.h>

class TextRenderer {
public:
    TextRenderer();
    ~TextRenderer();

    void draw(float x, float y, int pixelSize, const std::string& text);

    float width(int pixelSize, const std::string& text);

    void setScale(float scale) { scale_ = scale; }

private:
    struct Glyph {
        GLuint tex = 0;
        int texW = 0, texH = 0;
        float descent = 0;
    };

    HFONT fontFor(int pixelSize);
    Glyph& glyphFor(int pixelSize, const std::string& text);

    HDC memDC_;
    float scale_ = 1.0f;
    std::unordered_map<int, HFONT> fonts_;
    std::unordered_map<std::string, Glyph> cache_;
};
