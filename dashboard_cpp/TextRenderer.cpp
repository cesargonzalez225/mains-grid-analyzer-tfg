#include "TextRenderer.h"
#include <vector>
#include <algorithm>
#include <cmath>

TextRenderer::TextRenderer() {
    memDC_ = CreateCompatibleDC(nullptr);
}

TextRenderer::~TextRenderer() {
    for (auto& kv : cache_) {
        if (kv.second.tex) glDeleteTextures(1, &kv.second.tex);
    }
    for (auto& kv : fonts_) {
        DeleteObject(kv.second);
    }
    if (memDC_) DeleteDC(memDC_);
}

HFONT TextRenderer::fontFor(int pixelSize) {
    auto it = fonts_.find(pixelSize);
    if (it != fonts_.end()) return it->second;

    int physicalSize = (int)std::lround(pixelSize * scale_);
    HFONT f = CreateFontA(
        -physicalSize, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
        DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    fonts_[pixelSize] = f;
    return f;
}

TextRenderer::Glyph& TextRenderer::glyphFor(int pixelSize, const std::string& text) {
    std::string key = std::to_string(pixelSize) + "|" + text;
    auto found = cache_.find(key);
    if (found != cache_.end()) return found->second;

    HFONT font = fontFor(pixelSize);
    SelectObject(memDC_, font);

    TEXTMETRICA tm{};
    GetTextMetricsA(memDC_, &tm);
    SIZE sz{};
    GetTextExtentPoint32A(memDC_, text.c_str(), (int)text.length(), &sz);
    int w = std::max((int)sz.cx, 1);
    int h = std::max((int)tm.tmHeight, 1);

    BITMAPINFO bmi{};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = w;
    bmi.bmiHeader.biHeight = -h;
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = nullptr;
    HBITMAP dib = CreateDIBSection(memDC_, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    HGDIOBJ oldBmp = SelectObject(memDC_, dib);

    SetBkMode(memDC_, OPAQUE);
    SetBkColor(memDC_, RGB(0, 0, 0));
    SetTextColor(memDC_, RGB(255, 255, 255));
    RECT rc{ 0, 0, w, h };
    FillRect(memDC_, &rc, (HBRUSH)GetStockObject(BLACK_BRUSH));
    TextOutA(memDC_, 0, 0, text.c_str(), (int)text.length());
    GdiFlush();

    std::vector<unsigned char> rgba((size_t)w * h * 4);
    const unsigned char* src = (const unsigned char*)bits;
    for (int i = 0; i < w * h; ++i) {
        unsigned char lum = src[i * 4 + 0];
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = lum;
    }

    GLuint tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());

    SelectObject(memDC_, oldBmp);
    DeleteObject(dib);

    Glyph g;
    g.tex = tex;
    g.texW = w;
    g.texH = h;
    g.descent = (float)tm.tmDescent;
    auto& slot = cache_[key];
    slot = g;
    return slot;
}

void TextRenderer::draw(float x, float y, int pixelSize, const std::string& text) {
    if (text.empty()) return;
    Glyph& g = glyphFor(pixelSize, text);
    if (!g.tex) return;

    float bottom = y - g.descent / scale_;
    float top = bottom + (float)g.texH / scale_;
    float left = x;
    float right = x + (float)g.texW / scale_;

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, g.tex);
    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 1.0f); glVertex2f(left, bottom);
    glTexCoord2f(1.0f, 1.0f); glVertex2f(right, bottom);
    glTexCoord2f(1.0f, 0.0f); glVertex2f(right, top);
    glTexCoord2f(0.0f, 0.0f); glVertex2f(left, top);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

float TextRenderer::width(int pixelSize, const std::string& text) {
    if (text.empty()) return 0.0f;
    return (float)glyphFor(pixelSize, text).texW / scale_;
}
