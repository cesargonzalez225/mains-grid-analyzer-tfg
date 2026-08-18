#pragma once

#include <string>
#include <deque>
#include <vector>
#include "SerialPort.h"
#include "DashboardState.h"
#include "CsvLogger.h"
#include "SerialReaderThread.h"
#include "TextRenderer.h"

class App {
public:
    App(const std::string& portName, int argc, char** argv);
    ~App();

    void run();

private:
    void onDisplay();
    void onTimer();
    void onKeyboard(unsigned char key);

    static void displayCallback();
    static void timerCallback(int value);
    static void keyboardCallback(unsigned char key, int x, int y);

    void drawText(float x, float y, int pixelSize, const std::string& text);
    float textWidth(int pixelSize, const std::string& text);
    static void drawFilledRect(float x, float y, float w, float h);
    static void drawRectOutline(float x, float y, float w, float h);
    static void drawHLine(float x1, float x2, float y);
    void drawReadoutCard(float x, float y, float w, float h,
                          const std::string& label, const std::string& value, bool hasValue);
    void drawStripChart(float x, float y, float w, float h,
                         const std::deque<float>& vrmsHist, const std::deque<float>& freqHist);
    void drawNodesPanel(float x, float y, float w, float h,
                         const std::vector<DashboardState::NodeInfo>& nodes);
    void drawStatsPanel(float x, float y, float w, float h,
                         const DashboardState::SessionStats& stats);
    static void setColorForStatus(PowerStatus status);
    static void setColorForLogLine(const std::string& line);

    static App* instance_;

    std::string portName_;
    SerialPort port_;
    DashboardState state_;
    CsvLogger csv_;
    SerialReaderThread reader_;
    TextRenderer text_;
    float scale_ = 1.0f;
    bool connected_ = false;
};
