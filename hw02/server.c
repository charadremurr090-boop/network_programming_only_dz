#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT 9000
#define MAX_CLIENTS 3

static void fail(const char *what) {
    fprintf(stderr, "%s: error Winsock %d\n", what, WSAGetLastError());
}

int main(void) {
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    SOCKET fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == INVALID_SOCKET) { fail("socket"); WSACleanup(); return 1; }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        fail("bind"); closesocket(fd); WSACleanup(); return 1;
    }
    if (listen(fd, 16) == SOCKET_ERROR) {
        fail("listen"); closesocket(fd); WSACleanup(); return 1;
    }
    printf("listening port %d\n", PORT);

    int served = 0;
    while (served < MAX_CLIENTS) {
        struct sockaddr_in peer;
        int plen = sizeof peer;

        SOCKET cfd = accept(fd, (struct sockaddr *)&peer, &plen);
        if (cfd == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEINTR) continue;
            fail("accept");
            break;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof ip);
        int port = ntohs(peer.sin_port);
        printf("client %d: %s:%d\n", served + 1, ip, port);

        char msg[64];
        int n = snprintf(msg, sizeof msg, "hello, %s:%d\n", ip, port);
        if (send(cfd, msg, n, 0) == SOCKET_ERROR) fail("send");

        char buf[256];
        for (;;) {
            int r = recv(cfd, buf, sizeof buf, 0);
            if (r == SOCKET_ERROR) {
                if (WSAGetLastError() == WSAEINTR) continue;
                fail("recv");
                break;
            }
            if (r == 0) break;      /* клиент закрыл соединение */
        }

        closesocket(cfd);           /* закрываем только клиентский */
        served++;
    }

    closesocket(fd);                /* слушающий — в конце */
    printf("served %d clients, stopping\n", served);
    WSACleanup();
    return 0;
}
