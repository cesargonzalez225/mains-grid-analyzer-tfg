#include "SerialPort.h"

SerialPort::~SerialPort() {
    close();
}

bool SerialPort::open(const std::string& portName, DWORD baudRate) {
    close();

    std::string fullName = "\\\\.\\" + portName;

    handle_ = CreateFileA(fullName.c_str(), GENERIC_READ | GENERIC_WRITE, 0,
                           nullptr, OPEN_EXISTING, 0, nullptr);
    if (handle_ == INVALID_HANDLE_VALUE) {
        return false;
    }

    DCB dcb = {};
    dcb.DCBlength = sizeof(dcb);
    if (!GetCommState(handle_, &dcb)) {
        close();
        return false;
    }
    dcb.BaudRate = baudRate;
    dcb.ByteSize = 8;
    dcb.Parity = NOPARITY;
    dcb.StopBits = ONESTOPBIT;
    if (!SetCommState(handle_, &dcb)) {
        close();
        return false;
    }

    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 100;
    timeouts.ReadTotalTimeoutMultiplier = 0;
    SetCommTimeouts(handle_, &timeouts);

    return true;
}

void SerialPort::close() {
    if (handle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(handle_);
        handle_ = INVALID_HANDLE_VALUE;
    }
    lineBuffer_.clear();
}

bool SerialPort::readMoreIntoBuffer() {
    char chunk[256];
    DWORD bytesRead = 0;
    if (!ReadFile(handle_, chunk, sizeof(chunk), &bytesRead, nullptr)) {
        return false;
    }
    if (bytesRead > 0) {
        lineBuffer_.append(chunk, bytesRead);
    }
    return true;
}

bool SerialPort::readLine(std::string& outLine) {
    while (isOpen()) {
        size_t newlinePos = lineBuffer_.find('\n');
        if (newlinePos != std::string::npos) {
            outLine = lineBuffer_.substr(0, newlinePos);
            if (!outLine.empty() && outLine.back() == '\r') outLine.pop_back();
            lineBuffer_.erase(0, newlinePos + 1);
            return true;
        }
        if (!readMoreIntoBuffer()) {
            return false;
        }
    }
    return false;
}
