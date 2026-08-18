#include "SerialReaderThread.h"

SerialReaderThread::SerialReaderThread(SerialPort& port, DashboardState& state, CsvLogger& csv)
    : port_(port), state_(state), csv_(csv) {}

SerialReaderThread::~SerialReaderThread() {
    stop();
}

void SerialReaderThread::start() {
    running_ = true;
    thread_ = std::thread(&SerialReaderThread::run, this);
}

void SerialReaderThread::stop() {
    if (!running_.exchange(false)) return;
    port_.close();
    if (thread_.joinable()) thread_.join();
}

void SerialReaderThread::run() {
    std::string line;
    while (running_.load() && port_.readLine(line)) {
        auto event = parser_.feedLine(line);
        if (event) {
            state_.apply(*event);
            csv_.log(*event);
        }
    }

    if (running_.load()) {
        connectionLost_ = true;
    }
}
