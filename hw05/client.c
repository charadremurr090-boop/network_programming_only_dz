/* client.c - REST-клиент.  Без аргументов: демо CRUD.  С аргументами: client METHOD /path [тело] */
#include "platform.h"

static void request(const char *m, const char *path, const char *body) {
    sock_t s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_port = htons(PORT); a.sin_addr.s_addr = inet_addr(HOST);
    if (connect(s, (struct sockaddr *)&a, sizeof a) < 0) { printf("запустите server\n"); exit(1); }

    char req[1024];
    int n = snprintf(req, sizeof req, "%s %s HTTP/1.1\r\nHost: %s\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                     m, path, HOST, (int)strlen(body), body);
    send(s, req, n, 0);

    printf(">>> %s %s %s\n", m, path, body);
    char buf[2048];
    while ((n = recv(s, buf, sizeof buf, 0)) > 0) fwrite(buf, 1, n, stdout);   /* ответ целиком */
    printf("\n\n");
    sock_close(s);
}

int main(int argc, char **argv) {
    net_init();
    if (argc >= 3) { request(argv[1], argv[2], argc > 3 ? argv[3] : ""); return 0; }
    request("GET",    "/api/items",   "");                              /* 200 */
    request("POST",   "/api/items",   "{\"title\":\"milk\"}");          /* 201 */
    request("POST",   "/api/items",   "{\"title\":\"homework\"}");      /* 201 */
    request("PUT",    "/api/items/2", "{\"title\":\"HTTP homework\"}"); /* 200 */
    request("GET",    "/api/items",   "");                              /* 200 */
    request("DELETE", "/api/items/1", "");                              /* 204 */
    request("GET",    "/api/items/1", "");                              /* 404 */
    request("POST",   "/api/items",   "not json");                      /* 400 */
    request("GET",    "/hello.txt",   "");                              /* 200, text/plain */
    request("GET",    "/nope.txt",    "");                              /* 404 */
    return 0;
}
