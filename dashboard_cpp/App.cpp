#include "App.h"
#include <windows.h>
#include <GL/freeglut.h>
#include <cstdio>
#include <cmath>
#include <chrono>

App* App::instance_ = nullptr;

App::App(const std::string& portName, int argc, char** argv)
    : portName_(portName), reader_(port_, state_, csv_) {
    instance_ = this;

    HDC screenDC = GetDC(nullptr);
    scale_ = GetDeviceCaps(screenDC, LOGPIXELSX) / 96.0f;
    ReleaseDC(nullptr, screenDC);
    text_.setScale(scale_);

    glutInit(&argc, argv);

    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB | GLUT_MULTISAMPLE);
    glutInitWindowSize((int)(1180 * scale_), (int)(760 * scale_));
    glutCreateWindow("Mains Grid Analyzer - C++ Dashboard");
    glClearColor(0.06f, 0.07f, 0.09f, 1.0f);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

    glutDisplayFunc(displayCallback);
    glutTimerFunc(33, timerCallback, 0);
    glutKeyboardFunc(keyboardCallback);

    connected_ = port_.open(portName_);
    if (connected_) {
        reader_.start();
    } else {
        fprintf(stderr, "Failed to open %s - check the port name and that nothing else "
                         "has it open, then restart.\n", portName_.c_str());
    }
}

App::~App() {
    reader_.stop();
}

void App::run() {
    glutMainLoop();
}

void App::displayCallback() { instance_->onDisplay(); }
void App::keyboardCallback(unsigned char key, int, int) { instance_->onKeyboard(key); }
void App::timerCallback(int) { instance_->onTimer(); }

void App::drawText(float x, float y, int pixelSize, const std::string& text) {
    text_.draw(x, y, pixelSize, text);
}

float App::textWidth(int pixelSize, const std::string& text) {
    return text_.width(pixelSize, text);
}

void App::drawFilledRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void App::drawRectOutline(float x, float y, float w, float h) {
    glBegin(GL_LINE_LOOP);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

void App::drawReadoutCard(float x, float y, float w, float h,
                           const std::string& label, const std::string& value, bool hasValue) {
    glColor3f(0.10f, 0.11f, 0.14f);
    drawFilledRect(x, y, w, h);

    glColor3f(0.35f, 0.75f, 0.70f);
    drawFilledRect(x, y, 4.0f, h);

    glColor3f(0.22f, 0.24f, 0.29f);
    drawRectOutline(x, y, w, h);

    glColor3f(0.55f, 0.58f, 0.63f);
    drawText(x + 14, y + h - 24, 13, label);
    if (hasValue) glColor3f(0.80f, 0.95f, 0.90f);
    else glColor3f(0.45f, 0.48f, 0.53f);
    drawText(x + 14, y + 24, 32, value);
}

void App::drawStripChart(float x, float y, float w, float h,
                          const std::deque<float>& vrmsHist, const std::deque<float>& freqHist) {
    glColor3f(0.10f, 0.11f, 0.14f);
    drawFilledRect(x, y, w, h);
    glColor3f(0.22f, 0.24f, 0.29f);
    drawRectOutline(x, y, w, h);

    glColor3f(0.55f, 0.58f, 0.63f);
    drawText(x + 12, y + h - 20, 12, "RECENT HISTORY");
    glColor3f(0.40f, 0.85f, 0.75f);
    drawText(x + w - 110, y + h - 20, 12, "Vrms");
    glColor3f(0.90f, 0.65f, 0.15f);
    drawText(x + w - 60, y + h - 20, 12, "Freq");

    const float plotLeft = x + 12;
    const float plotRight = x + w - 12;
    const float plotTop = y + h - 32;
    const float plotBottom = y + 12;

    auto plotSeries = [&](const std::deque<float>& hist, float r, float g, float b) {
        if (hist.size() < 2) return;
        float lo = hist[0], hi = hist[0];
        for (float v : hist) {
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        float range = hi - lo;
        if (range < 0.001f) range = 1.0f;

        glColor3f(r, g, b);
        glBegin(GL_LINE_STRIP);
        for (size_t i = 0; i < hist.size(); i++) {
            float tx = plotLeft + (plotRight - plotLeft) * ((float)i / (float)(hist.size() - 1));
            float ty = plotBottom + (plotTop - plotBottom) * ((hist[i] - lo) / range);
            glVertex2f(tx, ty);
        }
        glEnd();
    };

    if (vrmsHist.size() < 2) {
        glColor3f(0.40f, 0.43f, 0.48f);
        drawText(plotLeft, (plotTop + plotBottom) / 2.0f, 12, "Collecting data...");
    } else {
        plotSeries(vrmsHist, 0.40f, 0.85f, 0.75f);
        plotSeries(freqHist, 0.90f, 0.65f, 0.15f);
    }
}

void App::drawNodesPanel(float x, float y, float w, float h,
                          const std::vector<DashboardState::NodeInfo>& nodes) {
    glColor3f(0.10f, 0.11f, 0.14f);
    drawFilledRect(x, y, w, h);
    glColor3f(0.22f, 0.24f, 0.29f);
    drawRectOutline(x, y, w, h);

    char buf[128];
    float ty = y + h - 22;
    glColor3f(0.55f, 0.58f, 0.63f);
    snprintf(buf, sizeof(buf), "NODES (%d)", (int)nodes.size());
    drawText(x + 12, ty, 13, buf);
    ty -= 26;

    if (nodes.empty()) {
        glColor3f(0.40f, 0.43f, 0.48f);
        drawText(x + 12, ty, 12, "None heard from yet.");
        return;
    }

    auto now = std::chrono::steady_clock::now();
    for (const auto& n : nodes) {
        if (ty < y + 50) break;

        setColorForStatus(n.status);
        drawFilledRect(x + 12, ty - 1, 8, 8);

        glColor3f(0.85f, 0.87f, 0.90f);
        drawText(x + 28, ty, 13, n.name);
        ty -= 18;

        glColor3f(0.60f, 0.63f, 0.68f);
        if (n.freq >= 0.0f && n.vrms >= 0.0f) {
            snprintf(buf, sizeof(buf), "%.2fHz  %.1fV", n.freq, n.vrms);
        } else {
            snprintf(buf, sizeof(buf), "--");
        }
        drawText(x + 28, ty, 11, buf);
        ty -= 16;

        double agoS = std::chrono::duration<double>(now - n.lastSeen).count();
        if (n.hasLinkInfo) {
            snprintf(buf, sizeof(buf), "RSSI %.0fdBm  SNR %.1fdB - %.0fs ago", n.rssi, n.snr, agoS);
        } else {
            snprintf(buf, sizeof(buf), "USB direct - %.0fs ago", agoS);
        }
        glColor3f(0.45f, 0.48f, 0.53f);
        drawText(x + 28, ty, 11, buf);
        ty -= 24;
    }
}

void App::drawStatsPanel(float x, float y, float w, float h,
                          const DashboardState::SessionStats& stats) {
    glColor3f(0.10f, 0.11f, 0.14f);
    drawFilledRect(x, y, w, h);
    glColor3f(0.22f, 0.24f, 0.29f);
    drawRectOutline(x, y, w, h);

    char buf[96];
    float ty = y + h - 22;
    glColor3f(0.55f, 0.58f, 0.63f);
    drawText(x + 12, ty, 13, "SESSION");
    ty -= 28;

    auto row = [&](const char* label, const std::string& value) {
        glColor3f(0.55f, 0.58f, 0.63f);
        drawText(x + 12, ty, 11, label);
        glColor3f(0.85f, 0.87f, 0.90f);
        drawText(x + 12, ty - 16, 15, value);
        ty -= 40;
    };

    snprintf(buf, sizeof(buf), "%d sag / %d swell / %d freq", stats.sagCount, stats.swellCount, stats.freqErrorCount);
    row("EVENTS", buf);

    if (stats.minVrms >= 0.0f) snprintf(buf, sizeof(buf), "%.1f - %.1f V", stats.minVrms, stats.maxVrms);
    else snprintf(buf, sizeof(buf), "--");
    row("VRMS RANGE", buf);

    if (stats.longestEventMs >= 0.0f) snprintf(buf, sizeof(buf), "%s, %.0f ms", stats.longestEventLabel.c_str(), stats.longestEventMs);
    else snprintf(buf, sizeof(buf), "--");
    row("LONGEST EVENT", buf);
}

void App::drawHLine(float x1, float x2, float y) {
    glBegin(GL_LINES);
    glVertex2f(x1, y);
    glVertex2f(x2, y);
    glEnd();
}

void App::setColorForStatus(PowerStatus status) {
    switch (status) {
        case PowerStatus::Normal:    glColor3f(0.30f, 0.82f, 0.31f); break;
        case PowerStatus::Sag:       glColor3f(0.88f, 0.32f, 0.32f); break;
        case PowerStatus::Swell:     glColor3f(0.90f, 0.65f, 0.15f); break;
        case PowerStatus::FreqError: glColor3f(0.72f, 0.42f, 0.86f); break;
        default:                     glColor3f(0.55f, 0.58f, 0.63f); break;
    }
}

void App::setColorForLogLine(const std::string& line) {
    if (line.find(" ended") != std::string::npos) {
        glColor3f(0.55f, 0.75f, 0.58f);
    } else if (line.find("FREQ_ERROR") != std::string::npos) {
        glColor3f(0.78f, 0.55f, 0.90f);
    } else if (line.find("SWELL") != std::string::npos) {
        glColor3f(0.92f, 0.70f, 0.25f);
    } else if (line.find("SAG") != std::string::npos) {
        glColor3f(0.90f, 0.45f, 0.45f);
    } else {
        glColor3f(0.72f, 0.73f, 0.76f);
    }
}

void App::onDisplay() {
    glClear(GL_COLOR_BUFFER_BIT);

    int winW = (int)(glutGet(GLUT_WINDOW_WIDTH) / scale_);
    int winH = (int)(glutGet(GLUT_WINDOW_HEIGHT) / scale_);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, winW, 0, winH, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glLineWidth(1.5f);

    const float marginX = 24;
    char buf[192];

    glColor3f(0.10f, 0.11f, 0.14f);
    drawFilledRect(0, (float)winH - 54, (float)winW, 54);

    float y = (float)winH - 36;
    glColor3f(0.92f, 0.92f, 0.94f);
    drawText(marginX, y, 20, "Mains Grid Analyzer");

    bool lost = connected_ && reader_.connectionLost();
    const char* statusWord = !connected_ ? "NOT CONNECTED - see console"
                            : lost        ? "CONNECTION LOST - restart the app"
                                          : "connected";
    snprintf(buf, sizeof(buf), "Port: %s  [%s]", portName_.c_str(), statusWord);
    if (!connected_ || lost) glColor3f(0.90f, 0.35f, 0.35f);
    else glColor3f(0.55f, 0.80f, 0.60f);
    drawText((float)winW - marginX - textWidth(13, buf), y, 13, buf);

    y -= 44;

    DashboardState::Snapshot snap = state_.snapshot();

    const float columnGap = 20.0f;
    const float rightW = 300.0f;
    const float leftW = (float)winW - 2 * marginX - columnGap - rightW;
    const float rightX = marginX + leftW + columnGap;
    const float columnTop = y;
    const float columnBottom = 34.0f;

    const float statsH = 180.0f;
    const float statsY = columnBottom;
    const float nodesY = statsY + statsH + 16.0f;
    const float nodesH = (columnTop - columnBottom) - statsH - 16.0f;
    drawNodesPanel(rightX, nodesY, rightW, nodesH, snap.nodes);
    drawStatsPanel(rightX, statsY, rightW, statsH, snap.stats);

    glColor3f(0.85f, 0.85f, 0.87f);
    snprintf(buf, sizeof(buf), "Node: %s", snap.nodeName.c_str());
    drawText(marginX, y, 20, buf);
    y -= 28;

    const float cardGap = 12.0f;
    const float cardH = 100.0f;
    const float cardW = (leftW - cardGap) / 2.0f;

    if (snap.freq >= 0) snprintf(buf, sizeof(buf), "%.2f Hz", snap.freq);
    else snprintf(buf, sizeof(buf), "--");
    drawReadoutCard(marginX, y - cardH, cardW, cardH, "FREQUENCY", buf, snap.freq >= 0);

    if (snap.vrms >= 0) snprintf(buf, sizeof(buf), "%.1f V", snap.vrms);
    else snprintf(buf, sizeof(buf), "--");
    drawReadoutCard(marginX + cardW + cardGap, y - cardH, cardW, cardH, "VRMS", buf, snap.vrms >= 0);
    y -= cardH + 24;

    setColorForStatus(snap.status);
    drawFilledRect(marginX, y - 30, leftW, 40);
    glColor3f(0.0f, 0.0f, 0.0f);
    drawRectOutline(marginX, y - 30, leftW, 40);

    static const char* statusText[] = { "UNKNOWN", "NORMAL", "SAG", "SWELL", "FREQ ERROR" };
    glColor3f(0.05f, 0.06f, 0.08f);
    snprintf(buf, sizeof(buf), "STATUS: %s", statusText[(int)snap.status]);

    drawText(marginX + 12, y - 16, 20, buf);
    y -= 54;

    const float stripH = 130.0f;
    drawStripChart(marginX, y - stripH, leftW, stripH, snap.vrmsHistory, snap.freqHistory);
    y -= stripH + 24;

    glColor3f(0.22f, 0.24f, 0.28f);
    drawHLine(marginX, marginX + leftW, y);
    y -= 22;

    glColor3f(0.55f, 0.58f, 0.63f);
    snprintf(buf, sizeof(buf), "Event log (most recent first):");
    drawText(marginX, y, 13, buf);
    y -= 20;

    if (snap.log.empty()) {
        glColor3f(0.40f, 0.43f, 0.48f);
        drawText(marginX, y, 13, "No events yet.");
    } else {
        for (auto it = snap.log.rbegin(); it != snap.log.rend(); ++it) {
            if (y < columnBottom) break;
            setColorForLogLine(*it);
            drawText(marginX, y, 13, *it);
            y -= 16;
        }
    }

    glColor3f(0.18f, 0.20f, 0.24f);
    drawHLine(marginX, (float)winW - marginX, 26);
    glColor3f(0.45f, 0.48f, 0.53f);
    drawText(marginX, 10, 11, "Esc to quit");

    glutSwapBuffers();
}

void App::onTimer() {
    glutPostRedisplay();
    glutTimerFunc(33, timerCallback, 0);
}

void App::onKeyboard(unsigned char key) {
    if (key == 27) {
        reader_.stop();
        glutLeaveMainLoop();
    }
}
