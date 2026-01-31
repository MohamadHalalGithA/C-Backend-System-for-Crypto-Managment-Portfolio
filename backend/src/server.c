#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <ctype.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <sqlite3.h>

#include "db.h"

#define FRONTEND_ORIGIN "http://localhost:8081"

// ---------- helpers ----------
static int send_all(int sock, const char *data, size_t length) {
    size_t sent = 0;
    while (sent < length) {
        ssize_t n = send(sock, data + sent, length - sent, 0);
        if (n < 0) return -1;
        sent += (size_t)n;
    }
    return 0;
}

static char *lower_dup(const char *s) {
    size_t n = strlen(s);
    char *o = (char*)malloc(n + 1);
    if (!o) return NULL;
    for (size_t i = 0; i < n; ++i) o[i] = (char)tolower((unsigned char)s[i]);
    o[n] = '\0';
    return o;
}

static const char *find_header_value(const char *req, const char *header_name) {
    // Portable-ish: lowercase compare by building lowercase copies
    char *req_l = lower_dup(req);
    char *hn_l  = lower_dup(header_name);
    if (!req_l || !hn_l) { free(req_l); free(hn_l); return NULL; }

    char *p = strstr(req_l, hn_l);
    if (!p) { free(req_l); free(hn_l); return NULL; }

    // Map pointer back to original request
    const char *orig_p = req + (p - req_l);
    free(req_l); free(hn_l);

    orig_p = strchr(orig_p, ':');
    if (!orig_p) return NULL;
    orig_p++;
    while (*orig_p == ' ' || *orig_p == '\t') orig_p++;
    return orig_p;
}

static int parse_content_length(const char *req) {
    const char *cl = find_header_value(req, "content-length");
    if (!cl) return 0;
    return atoi(cl);
}

static void send_preflight(int sock) {
    char hdr[1024];
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 204 No Content\r\n"
        "Access-Control-Allow-Origin: %s\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Content-Length: 0\r\n"
        "Connection: close\r\n\r\n",
        FRONTEND_ORIGIN
    );
    send_all(sock, hdr, strlen(hdr));
}

static void send_json(int sock, int status, const char *status_text, const char *json_body) {
    if (!json_body) json_body = "";
    char hdr[1024];
    snprintf(hdr, sizeof(hdr),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: application/json\r\n"
        "Access-Control-Allow-Origin: %s\r\n"
        "Access-Control-Allow-Credentials: true\r\n"
        "Content-Length: %zu\r\n"
        "Connection: close\r\n\r\n",
        status, status_text, FRONTEND_ORIGIN, strlen(json_body)
    );
    send_all(sock, hdr, strlen(hdr));
    send_all(sock, json_body, strlen(json_body));
}

static char *wrap_success_raw(const char *raw_json) {
    if (!raw_json) raw_json = "null";
    size_t n = strlen(raw_json) + 64;
    char *out = (char*)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "{\"success\":true,\"data\":%s}", raw_json);
    return out;
}

static char *wrap_error_msg(const char *msg) {
    if (!msg) msg = "Error";
    size_t n = strlen(msg) + 64;
    char *out = (char*)malloc(n);
    if (!out) return NULL;
    snprintf(out, n, "{\"success\":false,\"error\":\"%s\"}", msg);
    return out;
}

// SUPER tiny JSON field extraction
static int json_get_string_field(const char *json, const char *key, char *out, size_t out_len) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p, ':'); if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    if (*p != '\"') return 0;
    p++;
    const char *e = strchr(p, '\"');
    if (!e) return 0;
    size_t n = (size_t)(e - p);
    if (n >= out_len) n = out_len - 1;
    strncpy(out, p, n);
    out[n] = '\0';
    return 1;
}

static int json_get_number_field(const char *json, const char *key, double *out) {
    char pat[128];
    snprintf(pat, sizeof(pat), "\"%s\"", key);
    const char *p = strstr(json, pat);
    if (!p) return 0;
    p = strchr(p, ':'); if (!p) return 0;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    *out = atof(p);
    return 1;
}

static int path_is(const char *path, const char *a, const char *b) {
    return (strcmp(path, a) == 0) || (b && strcmp(path, b) == 0);
}

// ---------- client thread ----------
void* handle_client(void *arg) {
    int client_sock = *(int*)arg;
    free(arg);

    char buf[16384];
    int received = 0;
    int header_end_index = -1;

    while (received < (int)sizeof(buf) - 1) {
        ssize_t n = recv(client_sock, buf + received, sizeof(buf) - 1 - received, 0);
        if (n <= 0) { close(client_sock); return NULL; }
        received += (int)n;
        buf[received] = '\0';
        char *hdr_end = strstr(buf, "\r\n\r\n");
        if (hdr_end) { header_end_index = (int)(hdr_end - buf + 4); break; }
    }
    if (header_end_index == -1) { close(client_sock); return NULL; }

    char method[8], path[256];
    method[0] = path[0] = '\0';
    sscanf(buf, "%7s %255s", method, path);

    if (strcmp(method, "OPTIONS") == 0) {
        send_preflight(client_sock);
        close(client_sock);
        return NULL;
    }

    int content_length = parse_content_length(buf);
    int body_start = header_end_index;
    int already = received - body_start;
    int remaining = content_length - already;

    while (remaining > 0 && received < (int)sizeof(buf) - 1) {
        ssize_t n = recv(client_sock, buf + received, sizeof(buf) - 1 - received, 0);
        if (n <= 0) break;
        received += (int)n;
        remaining -= (int)n;
    }
    buf[received] = '\0';

    const char *body_json = (content_length > 0 && body_start < received) ? (buf + body_start) : NULL;

    // ---------- ROUTES ----------
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/health") == 0) {
        char *ok = wrap_success_raw("{\"status\":\"ok\"}");
        send_json(client_sock, 200, "OK", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // GET /api/transactions (accept trailing slash too)
    if (strcmp(method, "GET") == 0 && path_is(path, "/api/transactions", "/api/transactions/")) {
        // For now: single-user demo mode (user_id=1).
        // If you want real auth, we can wire cookies later.
        int user_id = 1;

        char *txs = db_get_transactions_json(user_id);
        if (!txs) txs = strdup("[]");

        char *ok = wrap_success_raw(txs);
        free(txs);

        send_json(client_sock, 200, "OK", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // POST /api/transactions
    if (strcmp(method, "POST") == 0 && path_is(path, "/api/transactions", "/api/transactions/")) {
        if (!body_json) {
            char *err = wrap_error_msg("Request body required");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }

        int user_id = 1;

        // payload example:
        // {type:"buy", asset:"ETH", amount:2, price:2650, total:5300, status:"pending", timestamp:"2025-...Z"}
        char type[16] = "", asset[64] = "", status[32] = "pending", timestamp[64] = "";
        double amount = 0, price = 0, total = 0;

        json_get_string_field(body_json, "type", type, sizeof(type));
        json_get_string_field(body_json, "asset", asset, sizeof(asset));
        json_get_string_field(body_json, "status", status, sizeof(status));
        json_get_string_field(body_json, "timestamp", timestamp, sizeof(timestamp));
        json_get_number_field(body_json, "amount", &amount);
        json_get_number_field(body_json, "price", &price);
        json_get_number_field(body_json, "total", &total);

        if (strcmp(type, "buy") != 0 && strcmp(type, "sell") != 0) {
            char *err = wrap_error_msg("type must be buy or sell");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        if (asset[0] == '\0' || amount <= 0) {
            char *err = wrap_error_msg("asset and positive amount required");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        if (timestamp[0] == '\0') {
            // fallback timestamp
            snprintf(timestamp, sizeof(timestamp), "1970-01-01T00:00:00.000Z");
        }

        int tx_id = -1;
        if (db_create_transaction(user_id, type, asset, amount, price, total, status, timestamp, &tx_id) != 0) {
            char *err = wrap_error_msg("Failed to create transaction (sell too much? asset missing?)");
            send_json(client_sock, 500, "Internal Server Error", err);
            free(err);
            close(client_sock);
            return NULL;
        }

        char *one = db_get_transaction_json(user_id, tx_id);
        if (!one) one = strdup("null");

        char *ok = wrap_success_raw(one);
        free(one);

        send_json(client_sock, 201, "Created", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // GET /api/me
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/me") == 0) {
        char *user = "{\"id\":\"1\",\"email\":\"demo@demo.com\",\"username\":\"demo\"}";
        char *ok = wrap_success_raw(user);
        send_json(client_sock, 200, "OK", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // GET /api/assets
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/assets") == 0) {
        int user_id = 1;
        char *assets = db_get_assets_json(user_id);
        if (!assets) assets = "[]";
        char *ok = wrap_success_raw(assets);
        free(assets);
        send_json(client_sock, 200, "OK", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // POST /api/assets
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/assets") == 0) {
        if (!body_json) {
            char *err = wrap_error_msg("Request body required");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        int user_id = 1;
        char symbol[64] = "", name[128] = "", category[64] = "Crypto";
        double quantity = 0, current_price = 0, avg_cost = 0;
        json_get_string_field(body_json, "symbol", symbol, sizeof(symbol));
        json_get_string_field(body_json, "name", name, sizeof(name));
        json_get_string_field(body_json, "category", category, sizeof(category));
        json_get_number_field(body_json, "quantity", &quantity);
        json_get_number_field(body_json, "current_price", &current_price);
        json_get_number_field(body_json, "avg_cost", &avg_cost);
        if (symbol[0] == '\0' || name[0] == '\0' || quantity <= 0) {
            char *err = wrap_error_msg("symbol, name, and positive quantity required");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        int asset_id = -1;
        if (db_create_asset(user_id, symbol, name, quantity, current_price, avg_cost, category, &asset_id) != 0) {
            char *err = wrap_error_msg("Failed to create asset");
            send_json(client_sock, 500, "Internal Server Error", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        char *one = db_get_asset_json(user_id, asset_id);
        if (!one) one = "null";
        char *ok = wrap_success_raw(one);
        free(one);
        send_json(client_sock, 201, "Created", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // GET /api/portfolio/chart
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/portfolio/chart") == 0) {
        int user_id = 1;
        char *chart = db_get_chart_json(user_id);
        if (!chart) chart = "[]";
        char *ok = wrap_success_raw(chart);
        free(chart);
        send_json(client_sock, 200, "OK", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // GET /api/portfolio/allocation
    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/portfolio/allocation") == 0) {
        int user_id = 1;
        char *alloc = db_get_allocation_json(user_id);
        if (!alloc) alloc = "[]";
        char *ok = wrap_success_raw(alloc);
        free(alloc);
        send_json(client_sock, 200, "OK", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // POST /api/signup
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/signup") == 0) {
        if (!body_json) {
            char *err = wrap_error_msg("Request body required");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        char email[128] = "", password[128] = "", username[64] = "";
        json_get_string_field(body_json, "email", email, sizeof(email));
        json_get_string_field(body_json, "password", password, sizeof(password));
        json_get_string_field(body_json, "username", username, sizeof(username));
        // Make email case insensitive
        for (size_t i = 0; email[i]; i++) email[i] = (char)tolower((unsigned char)email[i]);
        if (email[0] == '\0' || password[0] == '\0' || username[0] == '\0') {
            char *err = wrap_error_msg("email, password, username required");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        int user_id = -1;
        if (db_create_user(email, password, username, &user_id) != 0) {
            char *err = wrap_error_msg("User creation failed (email exists?)");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        char *user_json = db_get_user_json(user_id);
        if (!user_json) user_json = "{\"id\":-1,\"email\":\"\",\"username\":\"\"}";
        char *ok = wrap_success_raw(user_json);
        free(user_json);
        send_json(client_sock, 201, "Created", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // POST /api/login
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/login") == 0) {
        if (!body_json) {
            char *err = wrap_error_msg("Request body required");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        char email[128] = "", password[128] = "";
        json_get_string_field(body_json, "email", email, sizeof(email));
        json_get_string_field(body_json, "password", password, sizeof(password));
        // Make email case insensitive
        for (size_t i = 0; email[i]; i++) email[i] = (char)tolower((unsigned char)email[i]);
        if (email[0] == '\0' || password[0] == '\0') {
            char *err = wrap_error_msg("email and password required");
            send_json(client_sock, 400, "Bad Request", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        int user_id = -1;
        char username[64] = "";
        if (db_check_user(email, password, &user_id, username, sizeof(username)) == 0) {
            char *err = wrap_error_msg("Invalid credentials");
            send_json(client_sock, 401, "Unauthorized", err);
            free(err);
            close(client_sock);
            return NULL;
        }
        char user_json[256];
        snprintf(user_json, sizeof(user_json), "{\"id\":\"%d\",\"email\":\"%s\",\"username\":\"%s\"}", user_id, email, username);
        char *ok = wrap_success_raw(user_json);
        send_json(client_sock, 200, "OK", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // POST /api/logout
    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/logout") == 0) {
        char *ok = wrap_success_raw("{\"message\":\"logged out\"}");
        send_json(client_sock, 200, "OK", ok);
        free(ok);
        close(client_sock);
        return NULL;
    }

    // fallback
    {
        char *err = wrap_error_msg("Not found");
        send_json(client_sock, 404, "Not Found", err);
        free(err);
        close(client_sock);
        return NULL;
    }
}
