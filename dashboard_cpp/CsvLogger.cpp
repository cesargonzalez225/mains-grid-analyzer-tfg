#include "CsvLogger.h"
#include <windows.h>
#include <ctime>
#include <cstdio>

static std::string nowTimestampForFilename() {
    time_t t = time(nullptr);
    tm local;
    localtime_s(&local, &t);
    char buf[32];
    snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d",
             local.tm_year + 1900, local.tm_mon + 1, local.tm_mday,
             local.tm_hour, local.tm_min, local.tm_sec);
    return buf;
}

static std::string nowTimestampForRow() {
    time_t t = time(nullptr);
    tm local;
    localtime_s(&local, &t);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", local.tm_hour, local.tm_min, local.tm_sec);
    return buf;
}

static const char* statusName(PowerStatus s) {
    switch (s) {
        case PowerStatus::Normal:    return "NORMAL";
        case PowerStatus::Sag:       return "SAG";
        case PowerStatus::Swell:     return "SWELL";
        case PowerStatus::FreqError: return "FREQ_ERROR";
        default:                     return "UNKNOWN";
    }
}

static const char* kSessionsDir = "sessions";

CsvLogger::CsvLogger() {

    CreateDirectoryA(kSessionsDir, nullptr);

    std::string filename = std::string(kSessionsDir) + "\\session_" + nowTimestampForFilename() + ".csv";
    file_.open(filename);
    if (file_) {
        file_ << "time,node,kind,freq_hz,vrms_v,status,rssi_dbm,snr_db,text\n";
        file_.flush();
    }
}

std::string CsvLogger::csvEscape(const std::string& field) {
    if (field.find(',') == std::string::npos && field.find('"') == std::string::npos) {
        return field;
    }
    std::string escaped = "\"";
    for (char c : field) {
        if (c == '"') escaped += "\"\"";
        else escaped += c;
    }
    escaped += "\"";
    return escaped;
}

void CsvLogger::log(const ParsedEvent& ev) {
    if (!file_) return;

    file_ << nowTimestampForRow() << ','
          << csvEscape(ev.nodeName) << ','
          << (ev.kind == EventKind::Reading ? "reading" : "event") << ',';

    if (ev.freq >= 0.0f) file_ << ev.freq;
    file_ << ',';
    if (ev.vrms >= 0.0f) file_ << ev.vrms;
    file_ << ',';
    file_ << statusName(ev.status) << ',';
    if (ev.hasLinkInfo) file_ << ev.rssi;
    file_ << ',';
    if (ev.hasLinkInfo) file_ << ev.snr;
    file_ << ',';
    file_ << csvEscape(ev.text);
    file_ << '\n';
    file_.flush();
}
