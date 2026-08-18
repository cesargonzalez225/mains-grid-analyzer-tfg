#pragma once

#include <thread>
#include <atomic>
#include "SerialPort.h"
#include "ProtocolParser.h"
#include "DashboardState.h"
#include "CsvLogger.h"

class SerialReaderThread {
public:
    SerialReaderThread(SerialPort& port, DashboardState& state, CsvLogger& csv);
    ~SerialReaderThread();

    SerialReaderThread(const SerialReaderThread&) = delete;
    SerialReaderThread& operator=(const SerialReaderThread&) = delete;

    void start();
    void stop();

    bool connectionLost() const { return connectionLost_.load(); }

private:
    void run();

    SerialPort& port_;
    DashboardState& state_;
    CsvLogger& csv_;
    ProtocolParser parser_;
    std::thread thread_;
    std::atomic<bool> running_{false};
    std::atomic<bool> connectionLost_{false};
};
