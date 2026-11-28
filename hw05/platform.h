/* platform.h - сокеты для Windows и Linux */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
  #include <winsock2.h>
  typedef SOCKET sock_t;
  #define sock_close closesocket
  static void net_init(void) { WSADATA w; WSAStartup(MAKEWORD(2, 2), &w); SetConsoleOutputCP(CP_UTF8); }
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int sock_t;
  #define sock_close close
  static void net_init(void) {}
#endif
#define HOST "127.0.0.1"
#define PORT 8080
