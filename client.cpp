#include <bits/stdc++.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define BUFFER_SIZE 4096

bool recv_all(
    SOCKET socket,
    char* data,
    uint32_t size
) {
    uint32_t received = 0;

    while (received < size) {
        int n =
            recv(
                socket,
                data + received,
                size - received,
                0
            );

        if (n <= 0)
            return false;

        received += n;
    }

    return true;
}

void combine_paths(
    const string& base,
    string relative,
    string& result
) {
    replace(
        relative.begin(),
        relative.end(),
        '/',
        '\\'
    );

    result =
        base +
        "\\" +
        relative;
}

void send_ignore_list(
    SOCKET socket,
    const string& file_path
) {
    ifstream file(
        file_path
    );

    string data =
        "IGNORE$";

    if (file) {
        string line;

        while (
            getline(
                file,
                line
            )
        ) {
            if (
                data.size() > 7
            )
                data += ',';

            data += line;
        }
    }

    data += '#';

    send(
        socket,
        data.data(),
        (int)data.size(),
        0
    );

    cout
        << "[CLIENT] Ignore list sent\n";
}

void process_message(
    const string& message,
    const string& directory
) {
    if (message.empty())
        return;

    char type =
        message[0];

    /*
       C$path$size$data
    */
    if (type == 'C') {
        size_t p1 =
            message.find('$', 2);

        if (p1 == string::npos)
            return;

        size_t p2 =
            message.find('$', p1 + 1);

        if (p2 == string::npos)
            return;

        string path =
            message.substr(
                2,
                p1 - 2
            );

        uint32_t file_size =
            stoul(
                message.substr(
                    p1 + 1,
                    p2 - p1 - 1
                )
            );

        if (
            message.size() <
            p2 + 1 + file_size
        )
            return;

        string full_path;

        combine_paths(
            directory,
            path,
            full_path
        );

        filesystem::create_directories(
            filesystem::path(
                full_path
            ).parent_path()
        );

        ofstream file(
            full_path,
            ios::binary
        );

        if (!file) {
            cerr
                << "[CLIENT] Could not create file\n";
            return;
        }

        file.write(
            message.data() + p2 + 1,
            file_size
        );

        file.close();

        cout
            << "[CLIENT] Synced file: "
            << full_path
            << '\n';
    }

    /*
       Z$path$
    */
    else if (type == 'Z') {
        size_t p =
            message.find('$', 2);

        if (p == string::npos)
            return;

        string path =
            message.substr(
                2,
                p - 2
            );

        string full_path;

        combine_paths(
            directory,
            path,
            full_path
        );

        filesystem::create_directories(
            full_path
        );

        cout
            << "[CLIENT] Directory synced: "
            << full_path
            << '\n';
    }

    /*
       D$path
    */
    else if (type == 'D') {
        string path =
            message.substr(2);

        string full_path;

        combine_paths(
            directory,
            path,
            full_path
        );

        error_code ec;

        filesystem::remove_all(
            full_path,
            ec
        );

        cout
            << "[CLIENT] Deleted: "
            << full_path
            << '\n';
    }
}

void process_data(
    SOCKET socket,
    const string& directory
) {
    while (true) {
        uint32_t message_size;

        if (
            !recv_all(
                socket,
                (char*)&message_size,
                sizeof(message_size)
            )
        )
            break;

        if (
            message_size == 0 ||
            message_size >
                100 * 1024 * 1024
        )
            break;

        string message(
            message_size,
            '\0'
        );

        if (
            !recv_all(
                socket,
                message.data(),
                message_size
            )
        )
            break;
        cout
        << "[CLIENT] Received message: ["
        << message
        << "]\n";
        process_message(
            message,
            directory
        );
    }

    cout
        << "[CLIENT] Server disconnected\n";
}

int main(
    int argc,
    char* argv[]
) {
    if (argc < 5) {
        cout
            << "Usage: "
            << argv[0]
            << " <server_ip> <port> "
               "<client_dir> <ignore_file>\n";

        return 1;
    }

    string server_ip =
        argv[1];

    int port =
        atoi(argv[2]);

    string client_dir =
        argv[3];

    string ignore_file =
        argv[4];

    filesystem::create_directories(
        client_dir
    );

    WSADATA wsa;

    WSAStartup(
        MAKEWORD(2, 2),
        &wsa
    );

    SOCKET socket_fd =
        ::socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    sockaddr_in addr{};

    addr.sin_family =
        AF_INET;

    addr.sin_port =
        htons(port);

    inet_pton(
        AF_INET,
        server_ip.c_str(),
        &addr.sin_addr
    );

    if (
        connect(
            socket_fd,
            (sockaddr*)&addr,
            sizeof(addr)
        ) < 0
    ) {
        cerr
            << "Connection failed\n";

        return 1;
    }

    cout
        << "Connected to server\n";

    send_ignore_list(
        socket_fd,
        ignore_file
    );

    cout
        << "Waiting for synchronization...\n";

    process_data(
        socket_fd,
        client_dir
    );

    closesocket(socket_fd);
    WSACleanup();

    return 0;
}
