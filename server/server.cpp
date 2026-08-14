#include <iostream>
#include <string>
#include <vector>
#include <winsock2.h>
#include "../protocol.h"

#pragma comment(lib, "ws2_32.lib")

int main() {
    WSADATA wsa;

    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(8080);

    if (bind(
        serverSocket,
        reinterpret_cast<sockaddr*>(&serverAddress),
        sizeof(serverAddress)
    ) == SOCKET_ERROR) {
        std::cerr << "Bind failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    if (listen(serverSocket, 5) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port 8080...\n";

    SOCKET clientSocket = accept(
        serverSocket,
        nullptr,
        nullptr
    );

    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Accept failed\n";
        closesocket(serverSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Client connected!\n";

    MessageType type;
    std::vector<char> payload;

    if (receiveMessage(
        clientSocket,
        type,
        payload
    )) {
        if (type == MessageType::TEXT) {
            std::string message(
                payload.begin(),
                payload.end()
            );

            std::cout << "Client says: "
                      << message << '\n';
        }
    } else {
        std::cerr << "Failed to receive message\n";
    }

    std::string response = "Hello from server!";

    std::vector<char> responsePayload(
        response.begin(),
        response.end()
    );

    if (!sendMessage(
        clientSocket,
        MessageType::TEXT,
        responsePayload
    )) {
        std::cerr << "Failed to send response\n";
    }

    closesocket(clientSocket);
    closesocket(serverSocket);

    WSACleanup();

    return 0;
}
