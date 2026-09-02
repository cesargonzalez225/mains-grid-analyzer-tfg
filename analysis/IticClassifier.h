#pragma once

enum class IticZone {
    Acceptable,
    ProhibitedOvervoltage,
    ProhibitedUndervoltage
};

IticZone classifyIticZone(float magnitudePercent, float durationMs);

const char* iticZoneName(IticZone zone);
