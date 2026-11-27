#include <stdio.h>
#include <winsock2.h>
#include "wepoll.h"

#pragma comment(lib, "ws2_32.lib")
#define PORT 9001
#define MAX_EVENTS 64

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

    HANDLE ep = epoll_create1(0);
    struct epoll_event ev = { .events = EPOLLIN, .data.fd = (int)srv };
    epoll_ctl(ep, EPOLL_CTL_ADD, (int)srv, &ev);

    printf("Epoll (wepoll) server on port %d\n", PORT);
    struct epoll_event events[MAX_EVENTS];

    while (1) {
        int n = epoll_wait(ep, events, MAX_EVENTS, -1);
        if (n < 0) {
            if (GetLastError() == WSAEINTR) continue; // Обработка EINTR
            break;
        }

        for (int i = 0; i < n; i++) {
            int fd = (int)events[i].data.fd;

            if (fd == (int)srv) {
                // Новое подключение
                SOCKET conn = accept(srv, NULL, NULL);
                if (conn != INVALID_SOCKET) {
                    set_nonblock(conn);
                    struct epoll_event cev = { .events = EPOLLIN, .data.fd = (int)conn };
                    epoll_ctl(ep, EPOLL_CTL_ADD, (int)conn, &cev);
                }
            } else {
                // Чтение данных
                char buf[1024];
                int r = recv((SOCKET)fd, buf, sizeof(buf), 0);
                
                if (r > 0) {
                    send((SOCKET)fd, buf, r, 0); // Эхо
                } else if (r == 0 || WSAGetLastError() != WSAEWOULDBLOCK) {
                    epoll_ctl(ep, EPOLL_CTL_DEL, fd, NULL);
                    closesocket((SOCKET)fd); // Закрываем без утечек
                }
            }
        }
    }
    epoll_close(ep);
    closesocket(srv);
    WSACleanup();
    return 0;
}