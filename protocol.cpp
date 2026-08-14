#include "protocol.h"

bool sendAll(SOCKET socket, const char* data, int length) {
    int totalSent = 0;

    while (totalSent < length) {
        int sent = send(
            socket,
            data + totalSent,
            length - totalSent,
            0
        );

        if (sent == SOCKET_ERROR || sent == 0) {
            return false;
        }

        totalSent += sent;
    }

    return true;
}

bool recvAll(SOCKET socket, char* data, int length) {
    int totalReceived = 0;

    while (totalReceived < length) {
        int received = recv(
            socket,
            data + totalReceived,
            length - totalReceived,
            0
        );

        if (received <= 0) {
            return false;
        }

        totalReceived += received;
    }

    return true;
}

bool sendMessage(
    SOCKET socket,
    MessageType type,
    const std::vector<char>& payload
) {
    uint8_t messageType = static_cast<uint8_t>(type);

    uint32_t length = static_cast<uint32_t>(payload.size());

    uint32_t networkLength = htonl(length);

    if (!sendAll(
        socket,
        reinterpret_cast<char*>(&messageType),
        sizeof(messageType)
    )) {
        return false;
    }

    if (!sendAll(
        socket,
        reinterpret_cast<char*>(&networkLength),
        sizeof(networkLength)
    )) {
        return false;
    }

    if (!payload.empty()) {
        if (!sendAll(
            socket,
            payload.data(),
            static_cast<int>(payload.size())
        )) {
            return false;
        }
    }

    return true;
}

bool receiveMessage(
    SOCKET socket,
    MessageType& type,
    std::vector<char>& payload
) {
    uint8_t messageType;

    uint32_t networkLength;

    if (!recvAll(
        socket,
        reinterpret_cast<char*>(&messageType),
        sizeof(messageType)
    )) {
        return false;
    }

    if (!recvAll(
        socket,
        reinterpret_cast<char*>(&networkLength),
        sizeof(networkLength)
    )) {
        return false;
    }

    uint32_t length = ntohl(networkLength);

    type = static_cast<MessageType>(messageType);

    payload.resize(length);

    if (length > 0) {
        if (!recvAll(
            socket,
            payload.data(),
            static_cast<int>(length)
        )) {
            return false;
        }
    }

    return true;
}