#include "IticClassifier.h"

namespace {

struct CurvePoint {
    float durationS;
    float limitPercent;
};

const CurvePoint kUpperCurve[] = {
    {0.001f,  500.0f},
    {0.003f,  200.0f},
    {0.5f,    140.0f},

    {1e9f,    110.0f},

};

const CurvePoint kLowerCurve[] = {
    {0.02f,   0.0f},
    {0.5f,    70.0f},
    {10.0f,   80.0f},
    {1e9f,    90.0f},
};

float upperLimitPercent(float durationS) {
    for (const auto& p : kUpperCurve) {
        if (durationS <= p.durationS) return p.limitPercent;
    }
    return kUpperCurve[3].limitPercent;
}

float lowerLimitPercent(float durationS) {
    for (const auto& p : kLowerCurve) {
        if (durationS <= p.durationS) return p.limitPercent;
    }
    return kLowerCurve[3].limitPercent;
}

}

IticZone classifyIticZone(float magnitudePercent, float durationMs) {
    float durationS = durationMs / 1000.0f;

    if (magnitudePercent > upperLimitPercent(durationS)) {
        return IticZone::ProhibitedOvervoltage;
    }
    if (magnitudePercent < lowerLimitPercent(durationS)) {
        return IticZone::ProhibitedUndervoltage;
    }
    return IticZone::Acceptable;
}

const char* iticZoneName(IticZone zone) {
    switch (zone) {
        case IticZone::ProhibitedOvervoltage:  return "PROHIBITED (overvoltage)";
        case IticZone::ProhibitedUndervoltage: return "PROHIBITED (undervoltage)";
        default:                                return "ACCEPTABLE";
    }
}
