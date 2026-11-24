/* ДЗ 3: клиент-калькулятор.  calc_client.exe [IP] [порт] [выражение ...]
   Без выражений — интерактивный режим (Enter — отправить, /quit — выйти).
   С выражениями — все уходят ОДНИМ send(), затем читаются ответы по одному.
   --long вместо выражений — отправить очень длинное выражение (1+1+...+1). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define BUF_MAX 16384

static SOCKET sock;
static char rbuf[BUF_MAX];           /* накопитель принятых байт */
static int rlen = 0;

/* send() может отправить меньше, чем просили, поэтому досылаем циклом */
static int send_all(const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(sock, buf + sent, len - sent, 0);
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEINTR) continue;
            return -1;
        }
        sent += n;
    }
    return 0;
}

/* прочитать ОДНО сообщение (до '\n'); остальные байты остаются в накопителе.
   0 — готово; 1 — сервер закрыл соединение раньше конца ответа; 2 — ответ слишком длинный; -1 — ошибка recv */
static int read_line(char *out, int size) {
    for (;;) {
        char *nl = memchr(rbuf, '\n', (size_t)rlen);
        if (nl != NULL) {
            int n = (int)(nl - rbuf), used = n + 1;
            if (n >= size) n = size - 1;
            memcpy(out, rbuf, (size_t)n);
            out[n] = '\0';
            memmove(rbuf, rbuf + used, (size_t)(rlen - used));
            rlen -= used;
            return 0;
        }
        if (rlen == BUF_MAX) return 2;
        int r = recv(sock, rbuf + rlen, BUF_MAX - rlen, 0);
        if (r == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEINTR) continue;
            return -1;
        }
        if (r == 0) return 1;
        rlen += r;
    }
}

/* прочитать и показать один ответ; 0 — всё хорошо */
static int show_answer(void) {
    char ans[256];
    int rc = read_line(ans, sizeof ans);
    if (rc == 0) {
        if (ans[0] == '\0') printf("server send empty answear\n");
        else                printf("answear: %s\n", ans);
        return 0;
    }
    if (rc == 1)      printf("server closed connection, no answear\n");
    else if (rc == 2) printf("answear too long, the edge of the msg isn't found\n");
    else if (WSAGetLastError() == WSAETIMEDOUT) printf("server doesn't respond after 5 sec\n");
    else if (WSAGetLastError() == WSAECONNRESET) printf("connection to the server has been lost\n");
    else              printf("reading error of Winsock code %d\n", WSAGetLastError());
    return -1;
}

int main(int argc, char **argv) {
    const char *ip = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 9000;

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) { fprintf(stderr, "WSAStartup failed\n"); return 1; }
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) { fprintf(stderr, "socket: error %d\n", WSAGetLastError()); return 1; }

#ifdef _WIN32
    DWORD tv = 5000;                                              /* на Windows — миллисекунды */
#else
    struct timeval tv = { 5, 0 };
#endif
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        int e = WSAGetLastError();
        if (e == WSAECONNREFUSED) printf("unable to connect: on the port %d nobody is listening the port.\n", port);
        else if (e == WSAETIMEDOUT) printf("unable to connect: no answear from %s\n", ip);
        else                       printf("connect: error Winsock %d\n", e);
        return 1;
    }
    printf("connected to %s:%d\n", ip, port);

    if (argc > 3) {                                               /* пакетный режим */
        char batch[BUF_MAX];
        int n = 0, count = 0;
        if (strcmp(argv[3], "--long") == 0) {
            for (int i = 0; i < 3000; i++) n += snprintf(batch + n, sizeof batch - (size_t)n, i ? "+1" : "1");
            n += snprintf(batch + n, sizeof batch - (size_t)n, "\n");
            count = 1;
        } else {
            for (int i = 3; i < argc; i++, count++) {
                n += snprintf(batch + n, sizeof batch - (size_t)n, "%s\n", argv[i]);
                if (n >= (int)sizeof batch) { printf("too long request\n"); return 1; }
            }
        }
        if (send_all(batch, n) < 0) { printf("lost while sending, code %d\n", WSAGetLastError()); return 1; }
        printf("sended %d with one send(), requests: %d\n", n, count);
        for (int i = 1; i <= count; i++) {
            printf("[%d] ", i);
            if (show_answer() < 0) break;
        }
    } else {                                                      /* интерактивный режим */
        char line[BUF_MAX];
        while (fgets(line, sizeof line, stdin)) {
            int n = (int)strcspn(line, "\r\n");
            if (n == 0) continue;
            line[n] = '\0';
            if (strcmp(line, "/quit") == 0) break;
            line[n] = '\n';                                       /* граница сообщения */
            if (send_all(line, n + 1) < 0) { printf("lost whilе sending, code %d\n", WSAGetLastError()); break; }
            if (show_answer() < 0) break;
        }
    }

    closesocket(sock);
    WSACleanup();
    return 0;
}
