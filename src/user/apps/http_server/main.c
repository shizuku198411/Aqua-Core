#include "core/commonlibs.h"
#include "user_syscall.h"

#define HTTP_PORT        8080u
#define HTTP_REQ_MAX     1024
#define HTTP_404_BODY    "<html><body><p>404 not found</p></body></html>\n"
#define HTTP_FILE_CHUNK  512
#define HTTP_ROOT_DIR    "/var/www/html"

static uint16_t host_to_be16(uint16_t v) {
    return (uint16_t) ((v << 8) | (v >> 8));
}

static int local_strlen(const char *s) {
    int n = 0;
    while (s && s[n] != '\0') {
        n++;
    }
    return n;
}

static void u32_to_dec(char *dst, size_t cap, uint32_t v) {
    char tmp[16];
    int n = 0;
    if (!dst || cap == 0) {
        return;
    }
    if (v == 0u) {
        if (cap >= 2u) {
            dst[0] = '0';
            dst[1] = '\0';
        } else {
            dst[0] = '\0';
        }
        return;
    }
    while (v > 0u && n < (int) sizeof(tmp)) {
        tmp[n++] = (char) ('0' + (v % 10u));
        v /= 10u;
    }
    int pos = 0;
    while (n > 0 && (size_t) pos + 1u < cap) {
        dst[pos++] = tmp[--n];
    }
    dst[pos] = '\0';
}

static int html_append_str(char *dst, size_t cap, size_t *pos, const char *s) {
    if (!dst || !pos || !s) {
        return -1;
    }
    while (*s) {
        if (*pos + 1 >= cap) {
            return -1;
        }
        dst[*pos] = *s++;
        (*pos)++;
    }
    dst[*pos] = '\0';
    return 0;
}

static int html_append_u32(char *dst, size_t cap, size_t *pos, uint32_t v) {
    char tmp[16];
    int n = 0;
    if (v == 0u) {
        return html_append_str(dst, cap, pos, "0");
    }
    while (v > 0u && n < (int) sizeof(tmp)) {
        tmp[n++] = (char) ('0' + (v % 10u));
        v /= 10u;
    }
    while (n > 0) {
        if (*pos + 1 >= cap) {
            return -1;
        }
        dst[*pos] = tmp[--n];
        (*pos)++;
    }
    dst[*pos] = '\0';
    return 0;
}

static int send_all(int fd, const char *buf, int len) {
    int off = 0;
    while (off < len) {
        int n = send(fd, buf + off, len - off);
        if (n <= 0) {
            return -1;
        }
        off += n;
    }
    return 0;
}

static int build_html(char *html, size_t cap, size_t *p) {
    struct kernel_info info;
    memset(&info, 0, sizeof(info));
    (void) kernel_info(&info);

    if (html_append_str(html, cap, p, "<!doctype html><html><head><meta charset=\"utf-8\">") < 0 ||
        html_append_str(html, cap, p, "<title>AquaCore HTTP Server</title>") < 0 ||
        html_append_str(html, cap, p, "<link rel=\"stylesheet\" href=\"/style.css\">") < 0 ||
        html_append_str(html, cap, p, "</head><body>") < 0 ||
        html_append_str(html, cap, p, "<main class=\"wrap\">") < 0 ||
        html_append_str(html, cap, p, "<h1>AquaCore</h1>") < 0 ||
        html_append_str(html, cap, p, "<p class=\"lead\">Minimal RISC-V kernel and userland running in QEMU.</p>") < 0 ||
        html_append_str(html, cap, p, "<section class=\"panel\"><h2>Quick Links</h2><ul>") < 0 ||
        html_append_str(html, cap, p, "<li><a href=\"/manual.html\">AquaCore Command Manual</a></li>") < 0 ||
        html_append_str(html, cap, p, "<li><a href=\"https://github.com/shizuku198411/Aqua-Core\" target=\"_blank\" rel=\"noopener\">Project on GitHub</a></li>") < 0 ||
        html_append_str(html, cap, p, "</ul></section>") < 0 ||
        html_append_str(html, cap, p, "<section class=\"panel\"><h2>Kernel Information</h2><ul>") < 0 ||
        html_append_str(html, cap, p, "<li><strong>version:</strong> ") < 0 ||
        html_append_str(html, cap, p, info.version) < 0 ||
        html_append_str(html, cap, p, "</li><li><strong>boot time (UTC):</strong> ") < 0 ||
        html_append_str(html, cap, p, info.time) < 0 ||
        html_append_str(html, cap, p, "</li><li><strong>total pages:</strong> ") < 0 ||
        html_append_u32(html, cap, p, info.total_pages) < 0 ||
        html_append_str(html, cap, p, "</li><li><strong>page size:</strong> ") < 0 ||
        html_append_u32(html, cap, p, info.page_size) < 0 ||
        html_append_str(html, cap, p, "</li><li><strong>proc max:</strong> ") < 0 ||
        html_append_u32(html, cap, p, info.proc_max) < 0 ||
        html_append_str(html, cap, p, "</li><li><strong>timer interval ms:</strong> ") < 0 ||
        html_append_u32(html, cap, p, info.timer_interval_ms) < 0 ||
        html_append_str(html, cap, p, "</li></ul></section>") < 0 ||
        html_append_str(html, cap, p, "</main>") < 0 ||
        html_append_str(html, cap, p, "</body></html>\n") < 0) {
        return -1;
    }
    return 0;
}

static int build_manual_html(char *html, size_t cap, size_t *p) {
    if (html_append_str(html, cap, p, "<!doctype html><html><head><meta charset=\"utf-8\">") < 0 ||
        html_append_str(html, cap, p, "<title>AquaCore Command Manual</title>") < 0 ||
        html_append_str(html, cap, p, "<link rel=\"stylesheet\" href=\"/style.css\">") < 0 ||
        html_append_str(html, cap, p, "</head><body><main class=\"wrap\">") < 0 ||
        html_append_str(html, cap, p, "<h1>AquaCore Command Manual</h1>") < 0 ||
        html_append_str(html, cap, p, "<p class=\"lead\">All currently implemented user apps in /bin.</p>") < 0 ||
        html_append_str(html, cap, p, "<section class=\"panel\"><h2>Shell and Process</h2><ul>") < 0 ||
        html_append_str(html, cap, p, "<li><code>shell</code>: Launch the interactive shell.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>ps</code>: Show process list.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>kill &lt;pid&gt;</code>: Send termination signal to a process.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>ipc_rx</code>: Wait and receive an IPC message.</li>") < 0 ||
        html_append_str(html, cap, p, "</ul></section>") < 0 ||
        html_append_str(html, cap, p, "<section class=\"panel\"><h2>System</h2><ul>") < 0 ||
        html_append_str(html, cap, p, "<li><code>kernel_info</code>: Show kernel parameters.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>date</code>: Show current UTC date/time.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>bitmap</code>: Show memory bitmap pages.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>echo &lt;text&gt;</code>: Print text to stdout.</li>") < 0 ||
        html_append_str(html, cap, p, "</ul></section>") < 0 ||
        html_append_str(html, cap, p, "<section class=\"panel\"><h2>File System</h2><ul>") < 0 ||
        html_append_str(html, cap, p, "<li><code>ls [path]</code>: List directory entries.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>mkdir &lt;path&gt;</code>: Create a directory.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>rmdir &lt;path&gt;</code>: Remove an empty directory.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>touch &lt;path&gt;</code>: Create an empty file.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>write &lt;path&gt; &lt;text&gt;</code>: Write text to a file.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>edit &lt;path&gt;</code>: Edit file content.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>cat &lt;path&gt;</code>: Print file content.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>rm &lt;path&gt;</code>: Remove a file.</li>") < 0 ||
        html_append_str(html, cap, p, "</ul></section>") < 0 ||
        html_append_str(html, cap, p, "<section class=\"panel\"><h2>Networking</h2><ul>") < 0 ||
        html_append_str(html, cap, p, "<li><code>ping &lt;ip&gt;</code>: Send ICMP echo request.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>nslookup &lt;host&gt;</code>: Resolve hostname via DNS.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>udp_send &lt;ip&gt; &lt;port&gt; &lt;message&gt;</code>: Send UDP payload.</li>") < 0 ||
        html_append_str(html, cap, p, "<li><code>curl &lt;url&gt;</code>: Fetch HTTP content.</li>") < 0 ||
        html_append_str(html, cap, p, "</ul></section>") < 0 ||
        html_append_str(html, cap, p, "<p><a href=\"/\">&larr; Back to home</a></p>") < 0 ||
        html_append_str(html, cap, p, "</main></body></html>\n") < 0) {
        return -1;
    }
    return 0;
}

static int build_css(char *css, size_t cap, size_t *p) {
    if (html_append_str(css, cap, p, ":root{--bg:#0d1117;--panel:#161b22;--panel-border:#30363d;--text:#c9d1d9;--muted:#8b949e;--link:#58a6ff;--head:#f0f6fc;}") < 0 ||
        html_append_str(css, cap, p, "*{box-sizing:border-box;}") < 0 ||
        html_append_str(css, cap, p, "body{margin:0;background:var(--bg);color:var(--text);font:16px/1.6 ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;}") < 0 ||
        html_append_str(css, cap, p, ".wrap{max-width:920px;margin:0 auto;padding:28px 18px 40px;}") < 0 ||
        html_append_str(css, cap, p, "h1,h2{color:var(--head);margin:0 0 12px;}h1{font-size:30px;}h2{font-size:20px;}") < 0 ||
        html_append_str(css, cap, p, ".lead{color:var(--muted);margin:0 0 18px;}") < 0 ||
        html_append_str(css, cap, p, ".panel{background:var(--panel);border:1px solid var(--panel-border);border-radius:12px;padding:14px 16px;margin:0 0 14px;}") < 0 ||
        html_append_str(css, cap, p, "ul{margin:8px 0 0 18px;padding:0;}li{margin:4px 0;}") < 0 ||
        html_append_str(css, cap, p, "a{color:var(--link);text-decoration:none;}a:hover{text-decoration:underline;}") < 0 ||
        html_append_str(css, cap, p, "code{background:#21262d;border:1px solid #30363d;border-radius:6px;padding:1px 5px;color:#f0f6fc;}") < 0) {
        return -1;
    }
    return 0;
}

static int ensure_file_with_builder(const char *path,
                                    int (*builder)(char *, size_t, size_t *),
                                    size_t cap,
                                    int force_rebuild) {
    if (!path || !builder) {
        return -1;
    }
    int need_rebuild = force_rebuild ? 1 : 0;
    int fd = fs_open(path, O_RDONLY);
    if (fd >= 0) {
        if (!force_rebuild) {
            char probe[8];
            int n = fs_read(fd, probe, sizeof(probe));
            if (n > 0) {
                // Corrupted bootstrap asset example observed in practice:
                // file content accidentally starts with an embedded HTTP response.
                int looks_like_http = (n >= 5 &&
                                       probe[0] == 'H' &&
                                       probe[1] == 'T' &&
                                       probe[2] == 'T' &&
                                       probe[3] == 'P' &&
                                       probe[4] == '/');
                need_rebuild = looks_like_http;
            } else {
                need_rebuild = 1;
            }
        } else {
            need_rebuild = 1;
        }
        fs_close(fd);
    } else {
        need_rebuild = 1;
    }

    if (!need_rebuild) {
        return 0;
    }

    char html[FS_FILE_MAX_SIZE];
    if (cap > sizeof(html)) {
        cap = sizeof(html);
    }
    size_t p = 0;
    html[0] = '\0';
    if (builder(html, cap, &p) < 0) {
        return -1;
    }

    fd = fs_open(path, O_CREAT | O_TRUNC | O_WRONLY);
    if (fd < 0) {
        return -1;
    }
    int w = fs_write(fd, html, (int) p);
    fs_close(fd);
    if (w < 0 || w != (int) p) {
        return -1;
    }
    return 0;
}

static const char *content_type_from_path(const char *path) {
    if (!path) return "application/octet-stream";
    int n = local_strlen(path);
    if (n >= 5 && path[n - 5] == '.' && path[n - 4] == 'h' && path[n - 3] == 't' && path[n - 2] == 'm' && path[n - 1] == 'l') {
        return "text/html";
    }
    if (n >= 4 && path[n - 4] == '.' && path[n - 3] == 'c' && path[n - 2] == 's' && path[n - 1] == 's') {
        return "text/css";
    }
    if (n >= 3 && path[n - 3] == '.' && path[n - 2] == 'j' && path[n - 1] == 's') {
        return "application/javascript";
    }
    return "application/octet-stream";
}

static int parse_request_path(const char *req, char *out, size_t cap) {
    if (!req || !out || cap < 2u) {
        return -1;
    }
    out[0] = '/';
    out[1] = '\0';

    // "GET /path HTTP/1.1"
    if (!(req[0] == 'G' && req[1] == 'E' && req[2] == 'T' && req[3] == ' ')) {
        return -1;
    }

    int i = 4;
    if (req[i] != '/') {
        return -1;
    }

    int pos = 0;
    while (req[i] != '\0' &&
           req[i] != ' ' &&
           req[i] != '\r' &&
           req[i] != '\n' &&
           req[i] != '?' &&
           req[i] != '#') {
        if ((size_t) pos + 1u >= cap) {
            return -1;
        }
        out[pos++] = req[i++];
    }
    out[pos] = '\0';
    return 0;
}

static int is_safe_path(const char *path) {
    if (!path || path[0] != '/') {
        return 0;
    }
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '\\') {
            return 0;
        }
        if (path[i] == '.' && path[i + 1] == '.') {
            return 0;
        }
    }
    return 1;
}

static int build_fs_path(const char *req_path, char *out, size_t cap) {
    if (!req_path || !out || cap == 0u) {
        return -1;
    }
    if (!is_safe_path(req_path)) {
        return -1;
    }
    out[0] = '\0';
    strcat_s(out, cap, HTTP_ROOT_DIR);
    if (strcmp(req_path, "/") == 0) {
        strcat_s(out, cap, "/index.html");
    } else {
        strcat_s(out, cap, req_path);
    }
    return 0;
}

static int ensure_default_asset(const char *req_path, int force_rebuild) {
    if (!req_path) {
        return -1;
    }
    if (strcmp(req_path, "/") == 0 || strcmp(req_path, "/index.html") == 0) {
        if (ensure_file_with_builder("/var/www/html/index.html", build_html, 1600, force_rebuild) < 0) {
            return -1;
        }
        return ensure_file_with_builder("/var/www/html/manual.html", build_manual_html, FS_FILE_MAX_SIZE, force_rebuild);
    }
    if (strcmp(req_path, "/style.css") == 0) {
        return ensure_file_with_builder("/var/www/html/style.css", build_css, 1600, 1);
    }
    if (strcmp(req_path, "/manual.html") == 0) {
        return ensure_file_with_builder("/var/www/html/manual.html", build_manual_html, FS_FILE_MAX_SIZE, force_rebuild);
    }
    return 0;
}

static int send_file_response(int cfd, const char *path) {
    if (!path) {
        return -1;
    }

    int fd = fs_open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }
    char probe[5];
    int probe_n = fs_read(fd, probe, sizeof(probe));
    fs_close(fd);
    if (probe_n >= 5 &&
        probe[0] == 'H' &&
        probe[1] == 'T' &&
        probe[2] == 'T' &&
        probe[3] == 'P' &&
        probe[4] == '/') {
        // File content looks like an embedded HTTP response.
        return -2;
    }
    fd = fs_open(path, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    char header[128];
    header[0] = '\0';
    strcat_s(header, sizeof(header), "HTTP/1.0 200 OK\r\n");
    strcat_s(header, sizeof(header), "Content-Type: ");
    strcat_s(header, sizeof(header), content_type_from_path(path));
    strcat_s(header, sizeof(header), "\r\n");
    strcat_s(header, sizeof(header), "Server: AquaCore-HTTPServer\r\n");
    strcat_s(header, sizeof(header), "Connection: close\r\n\r\n");

    if (send_all(cfd, header, local_strlen(header)) < 0) {
        fs_close(fd);
        return -1;
    }

    char buf[HTTP_FILE_CHUNK];
    while (1) {
        int n = fs_read(fd, buf, sizeof(buf));
        if (n < 0) {
            fs_close(fd);
            return -1;
        }
        if (n == 0) {
            break;
        }
        if (send_all(cfd, buf, n) < 0) {
            fs_close(fd);
            return -1;
        }
    }
    fs_close(fd);
    return 0;
}

int main(int argc, char **argv) {
    (void) argv;
    if (argc != 1) {
        printf("usage: http_server\n");
        return -1;
    }

    int s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
        printf("socket failed\n");
        return -1;
    }

    struct socket_addr_in local;
    local.sin_family = AF_INET;
    local.sin_port = host_to_be16(HTTP_PORT);
    local.sin_addr = 0;
    if (bind(s, &local, sizeof(local)) < 0) {
        printf("bind failed\n");
        fs_close(s);
        return -1;
    }
    if (listen(s, 1) < 0) {
        printf("listen failed\n");
        fs_close(s);
        return -1;
    }

    while (1) {
        struct socket_addr_in peer;
        uint32_t peer_len = sizeof(peer);
        int cfd = accept(s, &peer, &peer_len);
        if (cfd < 0) {
            sleep(10);
            continue;
        }

        char req[HTTP_REQ_MAX + 1];
        int n = recv(cfd, req, HTTP_REQ_MAX);
        if (n < 0) {
            fs_close(cfd);
            continue;
        }
        if (n > 0) {
            req[n] = '\0';
        } else {
            req[0] = '\0';
        }

        char req_path[FS_PATH_MAX];
        char fs_path[FS_PATH_MAX];
        int valid_req = (parse_request_path(req, req_path, sizeof(req_path)) == 0);
        if (valid_req) {
            if (ensure_default_asset(req_path, 0) < 0) {
                valid_req = 0;
            }
            valid_req = (build_fs_path(req_path, fs_path, sizeof(fs_path)) == 0);
        }

        int sent = -1;
        if (valid_req) {
            sent = send_file_response(cfd, fs_path);
            if (sent == -2 && ensure_default_asset(req_path, 1) == 0) {
                sent = send_file_response(cfd, fs_path);
            }
        }

        if (!valid_req || sent < 0) {
            int body_len = local_strlen(HTTP_404_BODY);
            char body_len_s[16];
            u32_to_dec(body_len_s, sizeof(body_len_s), (uint32_t) body_len);

            char header[256];
            header[0] = '\0';
            strcat_s(header, sizeof(header), "HTTP/1.1 404 Not Found\r\n");
            strcat_s(header, sizeof(header), "Content-Type: text/html\r\n");
            strcat_s(header, sizeof(header), "Connection: close\r\n");
            strcat_s(header, sizeof(header), "Server: AquaCore-HTTPServer\r\n");
            strcat_s(header, sizeof(header), "Content-Length: ");
            strcat_s(header, sizeof(header), body_len_s);
            strcat_s(header, sizeof(header), "\r\n\r\n");

            if (send_all(cfd, header, local_strlen(header)) == 0) {
                (void) send_all(cfd, HTTP_404_BODY, body_len);
            }
        }

        fs_close(cfd);
    }
}
