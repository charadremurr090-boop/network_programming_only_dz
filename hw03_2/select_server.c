#include <stdio.h>
#include <winsock2.h>

#pragma comment(lib, "ws2_32.lib")
#define PORT 9000
#define MAX_CLIENTS 60

// Делаем сокет неблокирующим
void set_nonblock(SOCKET fd) {
    u_long mode = 1;
    ioctlsocket(fd, FIONBIO, &mode);
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = { AF_INET, htons(PORT), { INADDR_ANY } };
    
    bind(srv, (struct sockaddr*)&addr, sizeof(addr));
    listen(srv, SOMAXCONN);
    set_nonblock(srv);

    SOCKET clients[MAX_CLIENTS];
    for (int i = 0; i < MAX_CLIENTS; i++) clients[i] = INVALID_SOCKET;

    printf("Select server on port %d\n", PORT);

    while (1) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] != INVALID_SOCKET) FD_SET(clients[i], &rfds);
        }

        if (select(0, &rfds, NULL, NULL, NULL) == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEINTR) continue; // Обработка EINTR
            break;
        }

        // Новое подключение
        if (FD_ISSET(srv, &rfds)) {
            SOCKET conn = accept(srv, NULL, NULL);
            if (conn != INVALID_SOCKET) {
                set_nonblock(conn);
                for (int i = 0; i < MAX_CLIENTS; i++) {
                    if (clients[i] == INVALID_SOCKET) { clients[i] = conn; break; }
                }
            }
        }

        // Чтение данных
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i] == INVALID_SOCKET || !FD_ISSET(clients[i], &rfds)) continue;
            
            char buf[1024];
            int r = recv(clients[i], buf, sizeof(buf), 0);
            
            if (r > 0) {
                send(clients[i], buf, r, 0); // Эхо
            } else if (r == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
                closesocket(clients[i]); // Закрываем без утечек
                clients[i] = INVALID_SOCKET;
            }
        }
    }
    closesocket(srv);
    WSACleanup();
    return 0;
}