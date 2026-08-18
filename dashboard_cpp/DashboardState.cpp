#include "DashboardState.h"
#include "IticClassifier.h"
#include <ctime>
#include <cstdio>
#include <algorithm>

static std::string timestampNow() {
    time_t t = time(nullptr);
    tm local;
    localtime_s(&local, &t);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    return buf;
}

void DashboardState::apply(const ParsedEvent& ev) {
    std::lock_guard<std::mutex> lock(mutex_);

    state_.nodeName = ev.nodeName;

    if (ev.freq >= 0.0f) state_.freq = ev.freq;
    if (ev.vrms >= 0.0f) state_.vrms = ev.vrms;

    NodeInfo& node = nodeMap_[ev.nodeName];
    node.name = ev.nodeName;
    if (ev.freq >= 0.0f) node.freq = ev.freq;
    if (ev.vrms >= 0.0f) node.vrms = ev.vrms;
    if (ev.hasLinkInfo) {
        node.rssi = ev.rssi;
        node.snr = ev.snr;
        node.hasLinkInfo = true;
    }
    node.lastSeen = std::chrono::steady_clock::now();

    if (ev.vrms >= 0.0f) {
        if (state_.stats.minVrms < 0.0f || ev.vrms < state_.stats.minVrms) state_.stats.minVrms = ev.vrms;
        if (ev.vrms > state_.stats.maxVrms) state_.stats.maxVrms = ev.vrms;
    }

    if (ev.kind == EventKind::Reading) {
        state_.status = ev.status;
        node.status = ev.status;

        state_.vrmsHistory.push_back(ev.vrms);
        if (state_.vrmsHistory.size() > kMaxHistoryPoints) state_.vrmsHistory.pop_front();
        state_.freqHistory.push_back(ev.freq);
        if (state_.freqHistory.size() > kMaxHistoryPoints) state_.freqHistory.pop_front();
        return;
    }

    state_.log.push_back(timestampNow() + "  " + ev.nodeName + ": " + ev.text);
    if (state_.log.size() > kMaxLogLines) state_.log.pop_front();
    if (ev.status != PowerStatus::Unknown) {
        state_.status = ev.status;
        node.status = ev.status;
    }

    if (ev.status == PowerStatus::Sag || ev.status == PowerStatus::Swell) {
        if (ev.status == PowerStatus::Sag) state_.stats.sagCount++;
        else state_.stats.swellCount++;
        if (ev.vrms >= 0.0f) {
            activeEvents_[ev.nodeName] = ActiveEvent{ ev.status, (ev.vrms / 230.0f) * 100.0f };
        }
    } else if (ev.status == PowerStatus::FreqError) {
        state_.stats.freqErrorCount++;
    } else if (ev.status == PowerStatus::Normal && ev.eventDurationMs >= 0.0f) {
        if (ev.eventDurationMs > state_.stats.longestEventMs) {
            state_.stats.longestEventMs = ev.eventDurationMs;
            state_.stats.longestEventLabel = ev.text.substr(0, ev.text.find(' '));
        }

        auto it = activeEvents_.find(ev.nodeName);
        if (it != activeEvents_.end() &&
            (it->second.status == PowerStatus::Sag || it->second.status == PowerStatus::Swell)) {

            IticZone zone = classifyIticZone(it->second.magnitudePercent, ev.eventDurationMs);
            char buf[192];
            snprintf(buf, sizeof(buf), "%s  %s: ITIC: %s (%.0f%% of nominal for %.0fms)",
                     timestampNow().c_str(), ev.nodeName.c_str(), iticZoneName(zone),
                     it->second.magnitudePercent, ev.eventDurationMs);
            state_.log.push_back(buf);
            if (state_.log.size() > kMaxLogLines) state_.log.pop_front();
            activeEvents_.erase(it);
        }
    }
}

DashboardState::Snapshot DashboardState::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    Snapshot snap = state_;
    snap.nodes.reserve(nodeMap_.size());
    for (const auto& kv : nodeMap_) snap.nodes.push_back(kv.second);
    std::sort(snap.nodes.begin(), snap.nodes.end(),
              [](const NodeInfo& a, const NodeInfo& b) { return a.name < b.name; });
    return snap;
}
