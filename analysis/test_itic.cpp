
#include "IticClassifier.h"
#include <cstdio>
#include <cstring>

struct TestCase {
    const char* label;
    float magnitudePercent;
    float durationMs;
    IticZone expected;
};

int main() {
    const TestCase cases[] = {

        {"300% for 0.5ms -> under 500% ceiling",      300.0f, 0.5f,   IticZone::Acceptable},
        {"600% for 0.5ms -> over 500% ceiling",        600.0f, 0.5f,   IticZone::ProhibitedOvervoltage},

        {"150% for 2ms -> under 200% ceiling",         150.0f, 2.0f,   IticZone::Acceptable},
        {"250% for 2ms -> over 200% ceiling",          250.0f, 2.0f,   IticZone::ProhibitedOvervoltage},

        {"130% for 100ms -> under 140% ceiling",       130.0f, 100.0f, IticZone::Acceptable},
        {"150% for 100ms -> over 140% ceiling",        150.0f, 100.0f, IticZone::ProhibitedOvervoltage},

        {"115% for 10s -> under 120% steady ceiling",  115.0f, 10000.0f, IticZone::Acceptable},
        {"125% for 10s -> over 120% steady ceiling",   125.0f, 10000.0f, IticZone::ProhibitedOvervoltage},

        {"10% for 10ms -> within tolerated interruption", 10.0f, 10.0f, IticZone::Acceptable},

        {"50% for 100ms -> under 70% floor",           50.0f,  100.0f, IticZone::ProhibitedUndervoltage},
        {"75% for 100ms -> at/above 70% floor",        75.0f,  100.0f, IticZone::Acceptable},

        {"75% for 5s -> under 80% floor",              75.0f,  5000.0f, IticZone::ProhibitedUndervoltage},
        {"85% for 5s -> at/above 80% floor",           85.0f,  5000.0f, IticZone::Acceptable},

        {"85% for 60s -> under 90% steady floor",      85.0f,  60000.0f, IticZone::ProhibitedUndervoltage},
        {"95% for 60s -> at/above 90% steady floor",   95.0f,  60000.0f, IticZone::Acceptable},
    };

    int total = 0, failed = 0;
    for (const auto& tc : cases) {
        total++;
        IticZone got = classifyIticZone(tc.magnitudePercent, tc.durationMs);
        bool ok = (got == tc.expected);
        if (!ok) failed++;
        printf("[%s] %-45s -> %s (expected %s)\n",
               ok ? "PASS" : "FAIL",
               tc.label,
               iticZoneName(got),
               iticZoneName(tc.expected));
    }

    printf("\n%d/%d passed\n", total - failed, total);
    return failed == 0 ? 0 : 1;
}
