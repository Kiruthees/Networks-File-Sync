#pragma once

#include <winsock2.h>
#include <cstdint>
#include <string>
#include <vector>

enum class MessageType : uint8_t {
    TEXT = 1,
    FILE = 2,
    ACK = 3
};

bool sendAll(SOCKET socket, const char* data, int length);

bool recvAll(SOCKET socket, char* data, int length);

bool sendMessage(
    SOCKET socket,
    MessageType type,
    const std::vector<char>& payload
);

bool receiveMessage(
    SOCKET socket,
    MessageType& type,
    std::vector<char>& payload
);