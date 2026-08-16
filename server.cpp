#include <bits/stdc++.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

using namespace std;

#define BUFFER_SIZE 1024
#define MAX_IGNORE_ENTRIES 100

struct Client {
    SOCKET socket;
    vector<string> ignore;
};

vector<Client> clients;
mutex clients_lock;

string sync_dir;
int max_clients;

bool send_all(SOCKET s, const char* data, uint32_t size) {
    uint32_t sent = 0;

    while (sent < size) {
        int n = send(s, data + sent, size - sent, 0);
        if (n <= 0) return false;
        sent += n;
    }

    return true;
}

string relative_path(const string& path) {
    string r = path;

    if (r.rfind(sync_dir, 0) == 0)
        r = r.substr(sync_dir.size());

    while (!r.empty() &&
           (r[0] == '\\' || r[0] == '/'))
        r.erase(0, 1);

    replace(r.begin(), r.end(), '\\', '/');

    return r;
}

bool checkignore(
    const string& path,
    const Client& client
) {
    size_t dot = path.find_last_of('.');

    if (dot == string::npos)
        return true;

    string ext = path.substr(dot + 1);

    for (auto& x : client.ignore)
        if (x == ext)
            return false;

    return true;
}

bool send_event(
    SOCKET socket,
    const string& path,
    bool is_directory,
    char type
) {
    string rel = relative_path(path);
    string message;

    if (type == 'C') {
        ifstream file(path, ios::binary);

        if (!file)
            return false;

        file.seekg(0, ios::end);

        uint32_t size =
            (uint32_t)file.tellg();

        file.seekg(0, ios::beg);

        string data(size, '\0');

        if (size)
            file.read(data.data(), size);

        message =
            "C$" +
            rel +
            "$" +
            to_string(size) +
            "$" +
            data;
    }

    else if (type == 'Z') {
        message =
            "Z$" +
            rel +
            "$";
    }

    else if (type == 'D') {
        message =
            "D$" +
            rel;
    }

    uint32_t length =
        (uint32_t)message.size();

    if (!send_all(
        socket,
        (char*)&length,
        sizeof(length)
    ))
        return false;

    return send_all(
        socket,
        message.data(),
        length
    );
}

void broadcast(
    const string& path,
    bool is_directory,
    char type
) {
    lock_guard<mutex> guard(clients_lock);

    for (auto& client : clients) {
        if (
            type == 'C' &&
            !is_directory &&
            !checkignore(path, client)
        )
            continue;

        send_event(
            client.socket,
            path,
            is_directory,
            type
        );
    }
}

void receive_ignore_list(
    SOCKET socket,
    Client& client
) {
    string data;
    char ch;

    while (true) {
        int n = recv(
            socket,
            &ch,
            1,
            0
        );

        if (n <= 0)
            return;

        if (ch == '#')
            break;

        data += ch;
    }

    if (
        data.rfind(
            "IGNORE$",
            0
        ) == 0
    )
        data = data.substr(7);

    stringstream ss(data);
    string ext;

    while (
        getline(ss, ext, ',') &&
        client.ignore.size() < MAX_IGNORE_ENTRIES
    ) {
        while (
            !ext.empty() &&
            isspace(
                (unsigned char)ext.back()
            )
        )
            ext.pop_back();

        if (!ext.empty())
            client.ignore.push_back(ext);
    }
}

void send_initial_sync(Client& client) {
    vector<string> dirs;
    vector<string> files;

    error_code ec;

    for (
        auto& entry :
        filesystem::recursive_directory_iterator(
            sync_dir,
            ec
        )
    ) {
        if (ec)
            break;

        string path =
            entry.path().string();

        if (entry.is_directory(ec))
            dirs.push_back(path);
        else if (entry.is_regular_file(ec))
            files.push_back(path);
    }

    sort(
        dirs.begin(),
        dirs.end(),
        [](const string& a, const string& b) {
            return a.size() < b.size();
        }
    );

    for (auto& path : dirs)
        send_event(
            client.socket,
            path,
            true,
            'Z'
        );

    for (auto& path : files) {
        if (checkignore(path, client))
            send_event(
                client.socket,
                path,
                false,
                'C'
            );
    }

    cout
        << "[SERVER] Initial sync sent\n";
}

void client_thread(SOCKET socket) {
    char buffer[BUFFER_SIZE];

    while (true) {
        int n = recv(
            socket,
            buffer,
            sizeof(buffer),
            0
        );

        if (n <= 0)
            break;
    }

    lock_guard<mutex> guard(clients_lock);

    clients.erase(
        remove_if(
            clients.begin(),
            clients.end(),
            [socket](const Client& c) {
                return c.socket == socket;
            }
        ),
        clients.end()
    );

    closesocket(socket);

    cout
        << "[SERVER] Client disconnected\n";
}

void send_directory_contents(
    const string& dir
) {
    vector<string> dirs;
    vector<string> files;

    error_code ec;

    for (
        auto& entry :
        filesystem::recursive_directory_iterator(
            dir,
            ec
        )
    ) {
        if (ec)
            break;

        string path =
            entry.path().string();

        if (entry.is_directory(ec))
            dirs.push_back(path);
        else if (entry.is_regular_file(ec))
            files.push_back(path);
    }

    sort(
        dirs.begin(),
        dirs.end(),
        [](const string& a, const string& b) {
            return a.size() < b.size();
        }
    );

    for (auto& path : dirs)
        broadcast(
            path,
            true,
            'Z'
        );

    for (auto& path : files)
        broadcast(
            path,
            false,
            'C'
        );
}

void watch_directory() {
    HANDLE h =
        CreateFileA(
            sync_dir.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ |
            FILE_SHARE_WRITE |
            FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS,
            nullptr
        );

    if (
        h ==
        INVALID_HANDLE_VALUE
    ) {
        cerr
            << "Could not watch directory\n";
        return;
    }

    char buffer[64 * 1024];

    string old_move;

    while (true) {
        DWORD bytes = 0;

        if (
            !ReadDirectoryChangesW(
                h,
                buffer,
                sizeof(buffer),
                TRUE,
                FILE_NOTIFY_CHANGE_FILE_NAME |
                FILE_NOTIFY_CHANGE_DIR_NAME |
                FILE_NOTIFY_CHANGE_LAST_WRITE |
                FILE_NOTIFY_CHANGE_SIZE,
                &bytes,
                nullptr,
                nullptr
            )
        )
            break;

        DWORD offset = 0;

        while (offset < bytes) {
            auto* event =
                (FILE_NOTIFY_INFORMATION*)
                (buffer + offset);

            wstring wname(
                event->FileName,
                event->FileNameLength /
                sizeof(wchar_t)
            );

            string name(
                wname.begin(),
                wname.end()
            );

            string path =
                sync_dir +
                "\\" +
                name;

            if (
                event->Action ==
                FILE_ACTION_RENAMED_OLD_NAME
            ) {
                old_move = path;

                cout
                    << "[SERVER] Rename old: "
                    << old_move
                    << '\n';
            }

            else if (
                event->Action ==
                FILE_ACTION_RENAMED_NEW_NAME
            ) {
                if (!old_move.empty()) {
                    cout
                        << "[SERVER] Renamed: "
                        << old_move
                        << " -> "
                        << path
                        << '\n';

                    // Rename = delete old
                    broadcast(
                        old_move,
                        false,
                        'D'
                    );

                    Sleep(100);

                    // Rename = create new
                    bool is_dir =
                        filesystem::is_directory(
                            path
                        );

                    broadcast(
                        path,
                        is_dir,
                        is_dir ? 'Z' : 'C'
                    );

                    if (is_dir)
                        send_directory_contents(
                            path
                        );

                    old_move.clear();
                }
            }

            else if (
                event->Action ==
                FILE_ACTION_ADDED
            ) {
                Sleep(100);

                bool is_dir =
                    filesystem::is_directory(
                        path
                    );

                cout
                    << "[SERVER] Created: "
                    << path
                    << '\n';

                broadcast(
                    path,
                    is_dir,
                    is_dir ? 'Z' : 'C'
                );

                if (is_dir)
                    send_directory_contents(
                        path
                    );
            }

            else if (
                event->Action ==
                FILE_ACTION_REMOVED
            ) {
                cout
                    << "[SERVER] Deleted: "
                    << path
                    << '\n';

                broadcast(
                    path,
                    false,
                    'D'
                );
            }

            else if (
                event->Action ==
                FILE_ACTION_MODIFIED
            ) {
                if (
                    filesystem::is_regular_file(
                        path
                    )
                ) {
                    cout
                        << "[SERVER] Modified: "
                        << path
                        << '\n';

                    broadcast(
                        path,
                        false,
                        'C'
                    );
                }
            }

            if (
                event->NextEntryOffset == 0
            )
                break;

            offset +=
                event->NextEntryOffset;
        }
    }

    CloseHandle(h);
}

int main(
    int argc,
    char* argv[]
) {
    if (argc < 4) {
        cout
            << "Usage: "
            << argv[0]
            << " <server_dir> <port> <max_clients>\n";

        return 1;
    }

    sync_dir =
        filesystem::absolute(
            argv[1]
        ).string();

    int port =
        atoi(argv[2]);

    max_clients =
        atoi(argv[3]);

    filesystem::create_directories(
        sync_dir
    );

    WSADATA wsa;

    WSAStartup(
        MAKEWORD(2, 2),
        &wsa
    );

    SOCKET server =
        socket(
            AF_INET,
            SOCK_STREAM,
            0
        );

    int opt = 1;

    setsockopt(
        server,
        SOL_SOCKET,
        SO_REUSEADDR,
        (char*)&opt,
        sizeof(opt)
    );

    sockaddr_in addr{};

    addr.sin_family =
        AF_INET;

    addr.sin_addr.s_addr =
        INADDR_ANY;

    addr.sin_port =
        htons(port);

    if (
        bind(
            server,
            (sockaddr*)&addr,
            sizeof(addr)
        ) < 0
    ) {
        cerr
            << "Bind failed\n";

        return 1;
    }

    listen(
        server,
        max_clients
    );

    cout
        << "Server watching: "
        << sync_dir
        << '\n';

    cout
        << "Listening on port "
        << port
        << '\n';

    thread(
        watch_directory
    ).detach();

    while (true) {
        sockaddr_in client_addr{};

        int len =
            sizeof(client_addr);

        SOCKET client_socket =
            accept(
                server,
                (sockaddr*)&client_addr,
                &len
            );

        if (
            client_socket ==
            INVALID_SOCKET
        )
            continue;

        Client client;

        client.socket =
            client_socket;

        receive_ignore_list(
            client_socket,
            client
        );

        lock_guard<mutex> guard(
            clients_lock
        );

        if (
            (int)clients.size()
            >= max_clients
        ) {
            closesocket(
                client_socket
            );
            continue;
        }

        clients.push_back(
            move(client)
        );

        send_initial_sync(
            clients.back()
        );

        cout
            << "[SERVER] Client connected\n";

        thread(
            client_thread,
            client_socket
        ).detach();
    }

    closesocket(server);
    WSACleanup();

    return 0;
}
