#pragma once

#include <string>
#include <deque>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <chrono>
#include "ProtocolParser.h"

class DashboardState {
public:

    struct NodeInfo {
        std::string name;
        float freq = -1.0f;
        float vrms = -1.0f;
        float rssi = 0.0f;
        float snr = 0.0f;
        bool hasLinkInfo = false;
        PowerStatus status = PowerStatus::Unknown;
        std::chrono::steady_clock::time_point lastSeen{};
    };

    struct SessionStats {
        int sagCount = 0;
        int swellCount = 0;
        int freqErrorCount = 0;
        float minVrms = -1.0f;
        float maxVrms = -1.0f;
        float longestEventMs = -1.0f;
        std::string longestEventLabel;
    };

    struct Snapshot {
        std::string nodeName = "--";
        float freq = -1.0f;
        float vrms = -1.0f;
        PowerStatus status = PowerStatus::Unknown;
        std::deque<std::string> log;

        std::vector<NodeInfo> nodes;
        SessionStats stats;

        std::deque<float> vrmsHistory;
        std::deque<float> freqHistory;
    };

    void apply(const ParsedEvent& ev);

    Snapshot snapshot() const;

private:
    static constexpr size_t kMaxLogLines = 40;
    static constexpr size_t kMaxHistoryPoints = 200;

    struct ActiveEvent {
        PowerStatus status;
        float magnitudePercent;
    };

    mutable std::mutex mutex_;
    Snapshot state_;
    std::unordered_map<std::string, NodeInfo> nodeMap_;
    std::unordered_map<std::string, ActiveEvent> activeEvents_;
};
