#include "ProtocolParser.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

std::string ProtocolParser::valueAfterColon(const std::string& line) {
    size_t colon = line.find(':');
    if (colon == std::string::npos) return "";
    size_t start = colon + 1;
    while (start < line.size() && line[start] == ' ') start++;
    return line.substr(start);
}

PowerStatus ProtocolParser::classifyFromReading(float vrms, float freq) {
    float pctOfNominal = (vrms / 230.0f) * 100.0f;
    if (pctOfNominal < 90.0f) return PowerStatus::Sag;
    if (pctOfNominal > 110.0f) return PowerStatus::Swell;
    if (std::fabs(freq - 50.0f) > 0.8f) return PowerStatus::FreqError;
    return PowerStatus::Normal;
}

bool ProtocolParser::tryExtractVrmsFreq(const std::string& text, float& outVrms, float& outFreq) {
    const char* p = strstr(text.c_str(), "Vrms=");
    if (!p) return false;
    return sscanf(p, "Vrms=%f freq=%f", &outVrms, &outFreq) == 2;
}

float ProtocolParser::tryExtractEventDurationMs(const std::string& text) {
    const char* p = strstr(text.c_str(), "ended after ");
    if (!p) return -1.0f;
    float ms;
    if (sscanf(p, "ended after %fms", &ms) != 1) return -1.0f;
    return ms;
}

std::optional<ParsedEvent> ProtocolParser::tryParseDirectSensorLine(const std::string& line) {
    float freq, vpeak, vrms, crossing;
    int matched = sscanf(line.c_str(),
        "freq:%f Vpeak:%f Vrms:%f crossing:%f",
        &freq, &vpeak, &vrms, &crossing);
    if (matched != 4) return std::nullopt;

    ParsedEvent ev;
    ev.kind = EventKind::Reading;
    ev.nodeName = "(this board)";
    ev.freq = freq;
    ev.vrms = vrms;
    ev.status = classifyFromReading(vrms, freq);
    return ev;
}

std::optional<ParsedEvent> ProtocolParser::finishBlock() {
    inBlock_ = false;

    if (blockCode_ == 3) {

        float vrms, freq;
        if (!tryExtractVrmsFreq(blockText_, vrms, freq)) {
            return std::nullopt;
        }
        ParsedEvent ev;
        ev.kind = EventKind::Reading;
        ev.nodeName = blockNodeName_;
        ev.freq = freq;
        ev.vrms = vrms;
        ev.status = classifyFromReading(vrms, freq);
        ev.rssi = blockRssi_;
        ev.snr = blockSnr_;
        ev.hasLinkInfo = true;
        return ev;
    }

    ParsedEvent ev;
    ev.kind = EventKind::LogMessage;
    ev.nodeName = blockNodeName_;
    ev.text = blockText_;
    ev.rssi = blockRssi_;
    ev.snr = blockSnr_;
    ev.hasLinkInfo = true;
    if (blockCode_ == 1) ev.status = PowerStatus::Sag;
    else if (blockCode_ == 2) ev.status = PowerStatus::Swell;
    else if (blockCode_ == 4) ev.status = PowerStatus::FreqError;
    else if (blockCode_ == 0) {
        ev.status = PowerStatus::Normal;
        ev.eventDurationMs = tryExtractEventDurationMs(blockText_);
    }

    float vrms, freq;
    if (tryExtractVrmsFreq(blockText_, vrms, freq)) {
        ev.vrms = vrms;
        ev.freq = freq;
    }
    return ev;
}

std::optional<ParsedEvent> ProtocolParser::feedLine(const std::string& line) {
    if (line == "--- Warning received ---") {
        inBlock_ = true;
        blockNodeName_.clear();
        blockCode_ = -1;
        blockText_.clear();
        blockRssi_ = 0.0f;
        blockSnr_ = 0.0f;
        return std::nullopt;
    }

    if (inBlock_) {
        if (line.rfind("Name:", 0) == 0) {
            blockNodeName_ = valueAfterColon(line);
        } else if (line.rfind("Code:", 0) == 0) {
            blockCode_ = atoi(valueAfterColon(line).c_str());
        } else if (line.rfind("Text:", 0) == 0) {
            blockText_ = valueAfterColon(line);
        } else if (line.rfind("RSSI:", 0) == 0) {
            sscanf(valueAfterColon(line).c_str(), "%f", &blockRssi_);
        } else if (line.rfind("SNR:", 0) == 0) {

            sscanf(valueAfterColon(line).c_str(), "%f", &blockSnr_);
            return finishBlock();
        }
        return std::nullopt;
    }

    return tryParseDirectSensorLine(line);
}
