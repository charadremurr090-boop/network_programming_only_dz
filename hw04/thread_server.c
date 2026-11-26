#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT 9000
#define BUF 1024

int active = 0;               // Общий ресурс
CRITICAL_SECTION stats_m;     // Аналог мьютекса в Windows

// Функция потока (сигнатура для Windows)
DWORD WINAPI worker(LPVOID arg) {
    int fd = *(int *)arg;
    free(arg);                // Освобождаем память, как на слайде 3

    // Защита счётчика (аналог lock/unlock)
    EnterCriticalSection(&stats_m);
    active++;
    printf("client connected, active: %d\n", active);
    LeaveCriticalSection(&stats_m);

    char buf[BUF];
    while (1) {
        int n = recv(fd, buf, BUF, 0);
        if (n <= 0) break;    // Клиент отключился
        send(fd, buf, n, 0);  // Эхо
    }

    closesocket(fd);

    EnterCriticalSection(&stats_m);
    active--;
    printf("client left, active: %d\n", active);
    LeaveCriticalSection(&stats_m);

    return 0;
}

int main() {
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    
    // Инициализация "мьютекса"
    InitializeCriticalSection(&stats_m);

    SOCKET srv = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);

    bind(srv, (struct sockaddr *)&addr, sizeof(addr));
    listen(srv, 10);
    printf("C Thread server on port %d\n", PORT);

    while (1) {
        SOCKET client = accept(srv, NULL, NULL);
        if (client == INVALID_SOCKET) continue;

        // Выделяем память в куче, чтобы поток не читал мусор
        int *pfd = malloc(sizeof(int));
        *pfd = (int)client;

        // Создаем поток (аналог pthread_create) и сразу отпускаем (detach)
        HANDLE hThread = CreateThread(NULL, 0, worker, pfd, 0, NULL);
        if (hThread) {
            CloseHandle(hThread); // Аналог pthread_detach
        } else {
            free(pfd);
            closesocket(client);
        }
    }

    closesocket(srv);
    DeleteCriticalSection(&stats_m);
    WSACleanup();
    return 0;
}