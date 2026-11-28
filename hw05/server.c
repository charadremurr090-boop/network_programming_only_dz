/* server.c - HTTP-сервер: файлы из текущей папки и REST API /api/items */
#include "platform.h"

#define JSON "application/json"
#define TEXT "text/plain; charset=utf-8"

typedef struct { int id; char title[64]; } Item;
static Item items[100];
static int count, next_id = 1;

static void send_resp(sock_t c, int code, const char *type, const char *body, int len, const char *extra) {
    const char *text = code == 200 ? "OK" : code == 201 ? "Created" : code == 204 ? "No Content"
                     : code == 400 ? "Bad Request" : "Not Found";
    char head[400];
    int n = snprintf(head, sizeof head, "HTTP/1.1 %d %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n%sConnection: close\r\n\r\n",
                     code, text, type, len, extra);
    send(c, head, n, 0);
    send(c, body, len, 0);
    printf("  -> %d\n", code);
}
#define reply(c, code, type, s, extra) send_resp(c, code, type, s, (int)strlen(s), extra)
#define fail(c, code) reply(c, code, TEXT, "error", "")

static int item_json(char *o, const Item *it) { return sprintf(o, "{\"id\":%d,\"title\":\"%s\"}", it->id, it->title); }

/* берёт значение title из тела {"title":"..."} */
static int get_title(const char *body, char *out) {
    const char *p = strstr(body, "\"title\"");
    if (!p || !(p = strchr(p + 7, ':')) || !(p = strchr(p, '"'))) return 0;
    const char *e = strchr(++p, '"');
    if (!e || e == p || e - p > 63) return 0;
    memcpy(out, p, e - p); out[e - p] = 0;
    return 1;
}

static void api(sock_t c, const char *m, const char *p, const char *body) {
    char out[12000], t[64];
    int id, i, n;
    if (!strcmp(p, "/api/items")) {
        if (!strcmp(m, "GET")) {                                   /* список */
            n = sprintf(out, "[");
            for (i = 0; i < count; i++) { if (i) out[n++] = ','; n += item_json(out + n, &items[i]); }
            strcpy(out + n, "]");
            reply(c, 200, JSON, out, "");
        } else if (!strcmp(m, "POST")) {                           /* создание */
            if (!get_title(body, t) || count >= 100) { fail(c, 400); return; }
            Item *it = &items[count++];
            it->id = next_id++; strcpy(it->title, t);
            char loc[64]; sprintf(loc, "Location: /api/items/%d\r\n", it->id);
            item_json(out, it);
            reply(c, 201, JSON, out, loc);
        } else fail(c, 404);
    } else if (sscanf(p, "/api/items/%d", &id) == 1) {
        for (i = 0; i < count && items[i].id != id; i++);
        if (i == count) { fail(c, 404); return; }
        if (!strcmp(m, "GET")) {                                   /* один элемент */
            item_json(out, &items[i]);
            reply(c, 200, JSON, out, "");
        } else if (!strcmp(m, "PUT")) {                            /* изменение */
            if (!get_title(body, t)) { fail(c, 400); return; }
            strcpy(items[i].title, t);
            item_json(out, &items[i]);
            reply(c, 200, JSON, out, "");
        } else if (!strcmp(m, "DELETE")) {                         /* удаление */
            memmove(&items[i], &items[i + 1], (count - i - 1) * sizeof(Item));
            count--;
            send_resp(c, 204, TEXT, "", 0, "");
        } else fail(c, 404);
    } else fail(c, 404);
}

static void serve_file(sock_t c, const char *path) {
    static const char *types[][2] = {
        {".html", "text/html; charset=utf-8"}, {".css", "text/css; charset=utf-8"},
        {".js", "application/javascript"}, {".json", JSON}, {".txt", TEXT},
        {".png", "image/png"}, {".jpg", "image/jpeg"} };
    static char data[1 << 20];
    if (strstr(path, "..")) { fail(c, 400); return; }
    FILE *f = fopen(path + 1, "rb");               /* "/hello.txt" -> "hello.txt" */
    if (!f) { fail(c, 404); return; }
    int len = (int)fread(data, 1, sizeof data, f);
    fclose(f);
    const char *type = "application/octet-stream", *ext = strrchr(path, '.');
    for (int i = 0; ext && i < 7; i++) if (!strcmp(ext, types[i][0])) type = types[i][1];
    send_resp(c, 200, type, data, len, "");
}

static void handle(sock_t c) {
    char buf[4096], m[16], p[256];
    int n = 0;
    while (n < (int)sizeof buf - 1) {                      /* читаем заголовки и тело */
        int r = recv(c, buf + n, (int)sizeof buf - 1 - n, 0);
        if (r <= 0) break;
        buf[n += r] = 0;
        char *end = strstr(buf, "\r\n\r\n"), *cl = strstr(buf, "Content-Length:");
        if (end && n - (end + 4 - buf) >= (cl ? atoi(cl + 15) : 0)) break;
    }
    if (sscanf(buf, "%15s %255s", m, p) != 2) return;
    char *body = strstr(buf, "\r\n\r\n");
    body = body ? body + 4 : "";
    printf("%s %s\n", m, p);
    if (!strncmp(p, "/api/", 5)) api(c, m, p, body);
    else if (!strcmp(m, "GET")) serve_file(c, p);
    else fail(c, 404);
}

int main(void) {
    net_init();
    sock_t ls = socket(AF_INET, SOCK_STREAM, 0);
    int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, (const char *)&one, sizeof one);
    struct sockaddr_in a = {0};
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(INADDR_LOOPBACK); a.sin_port = htons(PORT);
    if (bind(ls, (struct sockaddr *)&a, sizeof a) < 0 || listen(ls, 8) < 0) { perror("bind"); return 1; }
    printf("http://%s:%d/\n", HOST, PORT);
    fflush(stdout);
    for (;;) {
        sock_t c = accept(ls, NULL, NULL);
        handle(c);
        sock_close(c);
        fflush(stdout);
    }
}
