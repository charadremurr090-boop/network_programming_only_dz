/* ДЗ 3: сервер-калькулятор.
   Протокол: одна строка = одно сообщение, граница — символ '\n', кодировка UTF-8.
   Запрос:  выражение\n          (числа, + - * /, скобки)
   Ответ:   OK результат\n   или   ERR описание\n */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT    9000
#define BUF_MAX 16384                /* максимум для одной строки запроса */

static void die(const char *what) {
    fprintf(stderr, "%s: error Winsock %d\n", what, WSAGetLastError());
    exit(1);
}

/* send() может отправить меньше, чем просили, поэтому досылаем циклом */
static int send_all(SOCKET s, const char *buf, int len) {
    int sent = 0;
    while (sent < len) {
        int n = send(s, buf + sent, len - sent, 0);
        if (n == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAEINTR) continue;
            return -1;
        }
        sent += n;
    }
    return 0;
}

/* ---------- вычисление выражения (рекурсивный разбор) ---------- */
static const char *p;                /* текущая позиция в выражении */
static int bad;                      /* 1 — ошибка в выражении, 2 — деление на ноль */

static void skip(void) { while (*p == ' ') p++; }
static double expr(void);

static double factor(void) {         /* число, (выражение) или унарный минус */
    skip();
    if (*p == '-') { p++; return -factor(); }
    if (*p == '(') {
        p++;
        double v = expr();
        skip();
        if (*p == ')') p++; else bad = 1;
        return v;
    }
    if (!isdigit((unsigned char)*p) && *p != '.') { bad = 1; return 0; }
    char *end;
    double v = strtod(p, &end);
    p = end;
    return v;
}

static double term(void) {           /* умножение и деление */
    double v = factor();
    for (;;) {
        skip();
        if (*p == '*') { p++; v *= factor(); }
        else if (*p == '/') {
            p++;
            double d = factor();
            if (d == 0) bad = 2; else v /= d;
        } else return v;
    }
}

static double expr(void) {           /* сложение и вычитание */
    double v = term();
    for (;;) {
        skip();
        if (*p == '+') { p++; v += term(); }
        else if (*p == '-') { p++; v -= term(); }
        else return v;
    }
}

/* ---------- обмен с клиентом ---------- */
/* ответить на одну строку-запрос */
static void reply(SOCKET cl, char *line) {
    size_t n = strlen(line);
    if (n > 0 && line[n - 1] == '\r') line[n - 1] = '\0';        /* telnet шлёт \r\n */

    p = line;
    bad = 0;
    double v = expr();
    skip();
    if (*p != '\0') bad = 1;                                      /* лишние символы */

    char out[128];
    int m;
    if (bad == 0) m = snprintf(out, sizeof out, "OK %.10g\n", v);
    else          m = snprintf(out, sizeof out, "ERR %s\n", bad == 2 ? "error": "divine on zero");
    send_all(cl, out, m);
    printf("  request: %.60s  ->  %.*s\n", line, m - 1, out);
}

static void handle(SOCKET cl) {
    char buf[BUF_MAX];
    int len = 0;                                                  /* сколько байт накоплено */

    for (;;) {
        int r = recv(cl, buf + len, BUF_MAX - len, 0);
        if (r == SOCKET_ERROR && WSAGetLastError() == WSAEINTR) continue;
        if (r <= 0) return;                                       /* клиент закрыл соединение или обрыв */
        len += r;

        /* в накопителе может быть несколько запросов, а может быть половина одного */
        char *start = buf, *nl;
        while ((nl = memchr(start, '\n', (size_t)(buf + len - start))) != NULL) {
            *nl = '\0';
            reply(cl, start);                                     /* одна строка = один запрос */
            start = nl + 1;
        }
        len = (int)(buf + len - start);                           /* остаток — начало следующего запроса */
        memmove(buf, start, (size_t)len);

        if (len == BUF_MAX) {                                     /* строка без \n и накопитель полон */
            const char *msg = "ERR too long request\n";
            send_all(cl, msg, (int)strlen(msg));
            return;
        }
    }
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) die("WSAStartup");
    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv == INVALID_SOCKET) die("socket");

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(srv, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) die("bind");
    if (listen(srv, 16) == SOCKET_ERROR) die("listen");
    printf("calculator is listening the port %d, to stop press: Ctrl+C\n", PORT);

    for (;;) {                                                    /* клиенты обслуживаются по очереди */
        struct sockaddr_in peer;
        int plen = sizeof peer;
        SOCKET cl = accept(srv, (struct sockaddr *)&peer, &plen);
        if (cl == INVALID_SOCKET) {
            if (WSAGetLastError() == WSAEINTR) continue;
            die("accept");
        }

        /* таймаут чтения: молчащий клиент не заблокирует сервер навсегда */
#ifdef _WIN32
        DWORD tv = 30000;                                         /* на Windows — миллисекунды */
#else
        struct timeval tv = { 30, 0 };
#endif
        setsockopt(cl, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof tv);

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof ip);
        printf("client %s:%d connected\n", ip, ntohs(peer.sin_port));
        handle(cl);
        closesocket(cl);
        printf("client %s:%d disconnected\n", ip, ntohs(peer.sin_port));
    }
}
