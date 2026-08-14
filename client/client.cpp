#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "../protocol.h"

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);

    if (inet_pton(
        AF_INET,
        "127.0.0.1",
        &serverAddress.sin_addr
    ) <= 0) {
        std::cerr << "Invalid server address\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    if (connect(
        clientSocket,
        reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)
    ) == SOCKET_ERROR) {
        std::cerr << "Connection failed\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Connected to server!\n";

    std::string message = "Hello from client!";

    std::vector<char> payload(
        message.begin(),
        message.end()
    );

    if (!sendMessage(
        clientSocket,
        MessageType::TEXT,
        payload
    )) {
        std::cerr << "Failed to send message\n";
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    MessageType type;
    std::vector<char> responsePayload;

    if (receiveMessage(
        clientSocket,
        type,
        responsePayload
    )) {
        if (type == MessageType::TEXT) {
            std::string response(
                responsePayload.begin(),
                responsePayload.end()
            );

            std::cout << "Server says: "
                      << response << '\n';
        }
    } else {
        std::cerr << "Failed to receive response\n";
    }

    closesocket(clientSocket);
    WSACleanup();

    return 0;
}
