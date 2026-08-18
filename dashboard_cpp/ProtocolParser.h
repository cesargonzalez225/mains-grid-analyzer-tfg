#pragma once

#include <string>
#include <optional>

enum class EventKind { Reading, LogMessage };

enum class PowerStatus { Unknown, Normal, Sag, Swell, FreqError };

struct ParsedEvent {
    EventKind kind;
    std::string nodeName;

    float freq = -1.0f;
    float vrms = -1.0f;
    PowerStatus status = PowerStatus::Unknown;

    float rssi = 0.0f;
    float snr = 0.0f;
    bool hasLinkInfo = false;

    std::string text;

    float eventDurationMs = -1.0f;
};

class ProtocolParser {
public:

    std::optional<ParsedEvent> feedLine(const std::string& line);

private:
    bool inBlock_ = false;
    std::string blockNodeName_;
    int blockCode_ = -1;
    std::string blockText_;
    float blockRssi_ = 0.0f;
    float blockSnr_ = 0.0f;

    static std::string valueAfterColon(const std::string& line);

    static float tryExtractEventDurationMs(const std::string& text);

    static PowerStatus classifyFromReading(float vrms, float freq);

    static bool tryExtractVrmsFreq(const std::string& text, float& outVrms, float& outFreq);

    std::optional<ParsedEvent> tryParseDirectSensorLine(const std::string& line);
    std::optional<ParsedEvent> finishBlock();
};
