#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <ctype.h>
#include "db.h"

static sqlite3 *db = NULL;

// -------- tiny helpers --------
static char* json_escape(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    char *esc = (char*)malloc(len * 2 + 1);
    if (!esc) return NULL;
    size_t j = 0;
    for (size_t i = 0; i < len; ++i) {
        if (s[i] == '\"' || s[i] == '\\') esc[j++] = '\\';
        esc[j++] = s[i];
    }
    esc[j] = '\0';
    return esc;
}

static int append_str(char **buf, int *cap, int *len, const char *s) {
    int sl = (int)strlen(s);
    if (*len + sl + 1 > *cap) {
        int newcap = (*cap) * 2;
        if (newcap < *len + sl + 1) newcap = *len + sl + 1;
        char *nb = (char*)realloc(*buf, newcap);
        if (!nb) return -1;
        *buf = nb; *cap = newcap;
    }
    memcpy(*buf + *len, s, sl);
    *len += sl;
    (*buf)[*len] = '\0';
    return 0;
}

sqlite3* db_get_handle(void) { return db; }

void db_close(void) {
    if (db) { sqlite3_close(db); db = NULL; }
}

int db_init(const char *db_path) {
    if (sqlite3_open(db_path, &db) != SQLITE_OK) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return 1;
    }

    sqlite3_exec(db, "PRAGMA foreign_keys = ON;", NULL, NULL, NULL);

    const char *sql_users =
        "CREATE TABLE IF NOT EXISTS users ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " email TEXT UNIQUE,"
        " username TEXT,"
        " password TEXT,"
        " created_at TEXT DEFAULT CURRENT_TIMESTAMP"
        ");";

    const char *sql_assets =
        "CREATE TABLE IF NOT EXISTS assets ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " user_id INTEGER NOT NULL,"
        " symbol TEXT NOT NULL,"
        " name TEXT NOT NULL,"
        " category TEXT DEFAULT 'Other',"
        " quantity REAL DEFAULT 0,"
        " avg_cost REAL DEFAULT 0,"
        " current_price REAL DEFAULT 0,"
        " FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
        ");";

    // IMPORTANT: timestamp is TEXT ISO string (matches frontend payload)
    const char *sql_transactions =
        "CREATE TABLE IF NOT EXISTS transactions ("
        " id INTEGER PRIMARY KEY AUTOINCREMENT,"
        " user_id INTEGER NOT NULL,"
        " type TEXT NOT NULL,"          // buy/sell
        " asset TEXT NOT NULL,"         // symbol
        " amount REAL NOT NULL,"
        " price REAL NOT NULL,"
        " total REAL NOT NULL,"
        " status TEXT DEFAULT 'pending',"
        " timestamp TEXT NOT NULL,"     // ISO string
        " FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
        ");";

    const char *sql_sessions =
        "CREATE TABLE IF NOT EXISTS sessions ("
        " token TEXT PRIMARY KEY,"
        " user_id INTEGER NOT NULL,"
        " created_at TEXT DEFAULT CURRENT_TIMESTAMP,"
        " FOREIGN KEY(user_id) REFERENCES users(id) ON DELETE CASCADE"
        ");";

    char *err = NULL;
    if (sqlite3_exec(db, sql_users, NULL, NULL, &err) != SQLITE_OK) { fprintf(stderr, "users: %s\n", err); sqlite3_free(err); return 1; }
    if (sqlite3_exec(db, sql_assets, NULL, NULL, &err) != SQLITE_OK) { fprintf(stderr, "assets: %s\n", err); sqlite3_free(err); return 1; }
    if (sqlite3_exec(db, sql_transactions, NULL, NULL, &err) != SQLITE_OK) { fprintf(stderr, "transactions: %s\n", err); sqlite3_free(err); return 1; }
    if (sqlite3_exec(db, sql_sessions, NULL, NULL, &err) != SQLITE_OK) { fprintf(stderr, "sessions: %s\n", err); sqlite3_free(err); return 1; }

    return 0;
}

// -------- users --------
int db_check_user(const char *email, const char *password, int *out_user_id, char *out_username, int username_bufsize) {
    char lower_email[128];
    strncpy(lower_email, email, sizeof(lower_email) - 1);
    lower_email[sizeof(lower_email) - 1] = '\0';
    for (size_t i = 0; lower_email[i]; i++) lower_email[i] = (char)tolower((unsigned char)lower_email[i]);

    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT id, username FROM users WHERE email = ? AND password = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, lower_email, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_TRANSIENT);

    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (out_user_id) *out_user_id = sqlite3_column_int(stmt, 0);
        if (out_username && username_bufsize > 0) {
            const unsigned char *u = sqlite3_column_text(stmt, 1);
            snprintf(out_username, (size_t)username_bufsize, "%s", u ? (const char*)u : "");
        }
        sqlite3_finalize(stmt);
        return 1;
    }
    sqlite3_finalize(stmt);
    return 0;
}

int db_create_user(const char *email, const char *password, const char *username, int *out_user_id) {
    char lower_email[128];
    strncpy(lower_email, email, sizeof(lower_email) - 1);
    lower_email[sizeof(lower_email) - 1] = '\0';
    for (size_t i = 0; lower_email[i]; i++) lower_email[i] = (char)tolower((unsigned char)lower_email[i]);

    sqlite3_stmt *stmt = NULL;

    // email exists?
    if (sqlite3_prepare_v2(db, "SELECT id FROM users WHERE email = ?;", -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, lower_email, -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) { sqlite3_finalize(stmt); return -1; }
        sqlite3_finalize(stmt);
    }

    if (sqlite3_prepare_v2(db, "INSERT INTO users(email,password,username) VALUES(?,?,?);", -1, &stmt, NULL) != SQLITE_OK) {
        return -2;
    }
    sqlite3_bind_text(stmt, 1, lower_email, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, password, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, username, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return -2; }
    sqlite3_finalize(stmt);

    if (out_user_id) *out_user_id = (int)sqlite3_last_insert_rowid(db);
    return 0;
}

int db_init_portfolio(int user_id) {
    // simple seeded assets (optional)
    int aid = -1;
    db_create_asset(user_id, "BTC", "Bitcoin", 0.5, 30000, 20000, "Crypto", &aid);
    db_create_asset(user_id, "ETH", "Ethereum", 5.0, 1800, 1500, "Crypto", &aid);
    db_create_asset(user_id, "DOGE", "Dogecoin", 10000, 0.07, 0.05, "Crypto", &aid);
    return 0;
}

char* db_get_user_json(int user_id) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "SELECT email, username FROM users WHERE id=?;", -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, user_id);

    char *out = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *email = (const char*)sqlite3_column_text(stmt, 0);
        const char *username = (const char*)sqlite3_column_text(stmt, 1);
        char *e1 = json_escape(email);
        char *e2 = json_escape(username);
        if (!e1 || !e2) { free(e1); free(e2); sqlite3_finalize(stmt); return NULL; }
        out = (char*)malloc(strlen(e1) + strlen(e2) + 64);
        if (out) snprintf(out, strlen(e1) + strlen(e2) + 64, "{\"id\":\"%d\",\"email\":\"%s\",\"username\":\"%s\"}", user_id, e1, e2);
        free(e1); free(e2);
    }
    sqlite3_finalize(stmt);
    return out;
}

// -------- assets --------
int db_create_asset(int user_id, const char *symbol, const char *name,
                    double quantity, double current_price, double avg_cost,
                    const char *category, int *out_asset_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO assets(user_id,symbol,name,category,quantity,current_price,avg_cost) "
        "VALUES(?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, symbol, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, category ? category : "Other", -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 5, quantity);
    sqlite3_bind_double(stmt, 6, current_price);
    sqlite3_bind_double(stmt, 7, avg_cost);

    if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return -1; }
    sqlite3_finalize(stmt);

    if (out_asset_id) *out_asset_id = (int)sqlite3_last_insert_rowid(db);
    return 0;
}

int db_update_asset(int user_id, int asset_id, const char *symbol, const char *name,
                    double quantity, double current_price, double avg_cost,
                    const char *category) {
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "UPDATE assets SET symbol=?, name=?, category=?, quantity=?, current_price=?, avg_cost=? "
        "WHERE id=? AND user_id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_text(stmt, 1, symbol, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, category ? category : "Other", -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, quantity);
    sqlite3_bind_double(stmt, 5, current_price);
    sqlite3_bind_double(stmt, 6, avg_cost);
    sqlite3_bind_int(stmt, 7, asset_id);
    sqlite3_bind_int(stmt, 8, user_id);

    if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return -1; }
    int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    return (changed > 0) ? 0 : -1;
}

int db_delete_asset(int user_id, int asset_id) {
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(db, "DELETE FROM assets WHERE id=? AND user_id=?;", -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_int(stmt, 1, asset_id);
    sqlite3_bind_int(stmt, 2, user_id);
    if (sqlite3_step(stmt) != SQLITE_DONE) { sqlite3_finalize(stmt); return -1; }
    int changed = sqlite3_changes(db);
    sqlite3_finalize(stmt);
    return (changed > 0) ? 0 : -1;
}

char* db_get_asset_json(int user_id, int asset_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id,symbol,name,category,quantity,avg_cost,current_price "
        "FROM assets WHERE id=? AND user_id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, asset_id);
    sqlite3_bind_int(stmt, 2, user_id);

    char *out = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *sym = (const char*)sqlite3_column_text(stmt, 1);
        const char *name = (const char*)sqlite3_column_text(stmt, 2);
        const char *cat = (const char*)sqlite3_column_text(stmt, 3);
        double qty = sqlite3_column_double(stmt, 4);
        double avg = sqlite3_column_double(stmt, 5);
        double cur = sqlite3_column_double(stmt, 6);

        char *es = json_escape(sym);
        char *en = json_escape(name);
        char *ec = json_escape(cat);
        if (!es || !en || !ec) { free(es); free(en); free(ec); sqlite3_finalize(stmt); return NULL; }

        out = (char*)malloc(strlen(es) + strlen(en) + strlen(ec) + 128);
        if (out) {
            snprintf(out, strlen(es) + strlen(en) + strlen(ec) + 128,
                "{\"id\":%d,\"symbol\":\"%s\",\"name\":\"%s\",\"category\":\"%s\",\"quantity\":%.6f,\"avg_cost\":%.2f,\"current_price\":%.2f}",
                id, es, en, ec, qty, avg, cur
            );
        }
        free(es); free(en); free(ec);
    }
    sqlite3_finalize(stmt);
    return out;
}

char* db_get_assets_json(int user_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id,symbol,name,category,quantity,avg_cost,current_price "
        "FROM assets WHERE user_id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, user_id);

    int cap = 512, len = 0;
    char *json = (char*)malloc(cap);
    if (!json) { sqlite3_finalize(stmt); return NULL; }

    append_str(&json, &cap, &len, "[");
    int first = 1;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *sym = (const char*)sqlite3_column_text(stmt, 1);
        const char *name = (const char*)sqlite3_column_text(stmt, 2);
        const char *cat = (const char*)sqlite3_column_text(stmt, 3);
        double qty = sqlite3_column_double(stmt, 4);
        double avg = sqlite3_column_double(stmt, 5);
        double cur = sqlite3_column_double(stmt, 6);

        char *es = json_escape(sym);
        char *en = json_escape(name);
        char *ec = json_escape(cat);
        if (!es || !en || !ec) { free(es); free(en); free(ec); free(json); sqlite3_finalize(stmt); return NULL; }

        char item[512];
        snprintf(item, sizeof(item),
            "%s{\"id\":%d,\"symbol\":\"%s\",\"name\":\"%s\",\"category\":\"%s\",\"quantity\":%.6f,\"avg_cost\":%.2f,\"current_price\":%.2f}",
            first ? "" : ",", id, es, en, ec, qty, avg, cur
        );
        first = 0;

        free(es); free(en); free(ec);

        if (append_str(&json, &cap, &len, item) < 0) { free(json); sqlite3_finalize(stmt); return NULL; }
    }

    sqlite3_finalize(stmt);
    append_str(&json, &cap, &len, "]");
    return json;
}

// -------- transactions + asset updates --------
static int get_asset_row(int user_id, const char *symbol, int *asset_id, double *qty, double *avg_cost, double *cur_price, char *name_out, size_t name_len, char *cat_out, size_t cat_len) {
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id,quantity,avg_cost,current_price,name,category "
        "FROM assets WHERE user_id=? AND symbol=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return 0;
    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, symbol, -1, SQLITE_TRANSIENT);

    int ok = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        if (asset_id) *asset_id = sqlite3_column_int(stmt, 0);
        if (qty) *qty = sqlite3_column_double(stmt, 1);
        if (avg_cost) *avg_cost = sqlite3_column_double(stmt, 2);
        if (cur_price) *cur_price = sqlite3_column_double(stmt, 3);

        const char *nm = (const char*)sqlite3_column_text(stmt, 4);
        const char *ct = (const char*)sqlite3_column_text(stmt, 5);
        if (name_out && name_len) snprintf(name_out, name_len, "%s", nm ? nm : symbol);
        if (cat_out && cat_len) snprintf(cat_out, cat_len, "%s", ct ? ct : "Other");
        ok = 1;
    }
    sqlite3_finalize(stmt);
    return ok;
}

int db_create_transaction(int user_id, const char *type, const char *asset,
                          double amount, double price, double total,
                          const char *status, const char *timestamp_iso,
                          int *out_tx_id) {
    if (!type || !asset || amount <= 0) return -1;
    if (price <= 0 && total > 0) price = total / amount;
    if (total <= 0 && price > 0) total = price * amount;
    if (!status) status = "pending";
    if (!timestamp_iso) timestamp_iso = "1970-01-01T00:00:00.000Z";

    // Begin transaction
    sqlite3_exec(db, "BEGIN;", NULL, NULL, NULL);

    // Insert tx
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "INSERT INTO transactions(user_id,type,asset,amount,price,total,status,timestamp) "
        "VALUES(?,?,?,?,?,?,?,?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) { sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL); return -1; }

    sqlite3_bind_int(stmt, 1, user_id);
    sqlite3_bind_text(stmt, 2, type, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, asset, -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 4, amount);
    sqlite3_bind_double(stmt, 5, price);
    sqlite3_bind_double(stmt, 6, total);
    sqlite3_bind_text(stmt, 7, status, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, timestamp_iso, -1, SQLITE_TRANSIENT);

    if (sqlite3_step(stmt) != SQLITE_DONE) {
        sqlite3_finalize(stmt);
        sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }
    sqlite3_finalize(stmt);

    int tx_id = (int)sqlite3_last_insert_rowid(db);
    if (out_tx_id) *out_tx_id = tx_id;

    // Update assets
    int asset_id = -1;
    double qty = 0, avg = 0, cur = 0;
    char nm[128] = "", cat[64] = "Crypto";
    int exists = get_asset_row(user_id, asset, &asset_id, &qty, &avg, &cur, nm, sizeof(nm), cat, sizeof(cat));

    if (!exists) {
        // create missing asset row on first buy
        if (strcmp(type, "buy") == 0) {
            double new_qty = amount;
            double new_avg = price;
            int new_asset_id = -1;
            if (db_create_asset(user_id, asset, asset, new_qty, price, new_avg, "Crypto", &new_asset_id) != 0) {
                sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
                return -1;
            }
        } else {
            // selling something you don't own -> reject
            sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
            return -1;
        }
    } else {
        if (strcmp(type, "buy") == 0) {
            double old_cost = qty * avg;
            double add_cost = amount * price;
            double new_qty = qty + amount;
            double new_avg = (new_qty > 0) ? (old_cost + add_cost) / new_qty : avg;
            // set current price to trade price
            if (db_update_asset(user_id, asset_id, asset, nm, new_qty, price, new_avg, cat) != 0) {
                sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
                return -1;
            }
        } else { // sell
            if (qty < amount) {
                sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
                return -1;
            }
            double new_qty = qty - amount;
            // keep avg_cost same on sell
            if (db_update_asset(user_id, asset_id, asset, nm, new_qty, price, avg, cat) != 0) {
                sqlite3_exec(db, "ROLLBACK;", NULL, NULL, NULL);
                return -1;
            }
        }
    }

    sqlite3_exec(db, "COMMIT;", NULL, NULL, NULL);
    return 0;
}

char* db_get_transaction_json(int user_id, int tx_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id,type,asset,amount,price,total,status,timestamp "
        "FROM transactions WHERE id=? AND user_id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, tx_id);
    sqlite3_bind_int(stmt, 2, user_id);

    char *out = NULL;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *type = (const char*)sqlite3_column_text(stmt, 1);
        const char *asset = (const char*)sqlite3_column_text(stmt, 2);
        double amount = sqlite3_column_double(stmt, 3);
        double price = sqlite3_column_double(stmt, 4);
        double total = sqlite3_column_double(stmt, 5);
        const char *status = (const char*)sqlite3_column_text(stmt, 6);
        const char *ts = (const char*)sqlite3_column_text(stmt, 7);

        char *et = json_escape(type);
        char *ea = json_escape(asset);
        char *es = json_escape(status);
        char *eTs = json_escape(ts);
        if (!et || !ea || !es || !eTs) { free(et); free(ea); free(es); free(eTs); sqlite3_finalize(stmt); return NULL; }

        out = (char*)malloc(strlen(et) + strlen(ea) + strlen(es) + strlen(eTs) + 256);
        if (out) {
            snprintf(out, strlen(et) + strlen(ea) + strlen(es) + strlen(eTs) + 256,
                "{\"id\":%d,\"type\":\"%s\",\"asset\":\"%s\",\"amount\":%.6f,\"price\":%.2f,\"total\":%.2f,\"status\":\"%s\",\"timestamp\":\"%s\"}",
                id, et, ea, amount, price, total, es, eTs
            );
        }
        free(et); free(ea); free(es); free(eTs);
    }
    sqlite3_finalize(stmt);
    return out;
}

char* db_get_transactions_json(int user_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql =
        "SELECT id,type,asset,amount,price,total,status,timestamp "
        "FROM transactions WHERE user_id=? ORDER BY id DESC;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, user_id);

    int cap = 512, len = 0;
    char *json = (char*)malloc(cap);
    if (!json) { sqlite3_finalize(stmt); return NULL; }

    append_str(&json, &cap, &len, "[");
    int first = 1;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        int id = sqlite3_column_int(stmt, 0);
        const char *type = (const char*)sqlite3_column_text(stmt, 1);
        const char *asset = (const char*)sqlite3_column_text(stmt, 2);
        double amount = sqlite3_column_double(stmt, 3);
        double price = sqlite3_column_double(stmt, 4);
        double total = sqlite3_column_double(stmt, 5);
        const char *status = (const char*)sqlite3_column_text(stmt, 6);
        const char *ts = (const char*)sqlite3_column_text(stmt, 7);

        char *et = json_escape(type);
        char *ea = json_escape(asset);
        char *es = json_escape(status);
        char *eTs = json_escape(ts);
        if (!et || !ea || !es || !eTs) { free(et); free(ea); free(es); free(eTs); free(json); sqlite3_finalize(stmt); return NULL; }

        char item[512];
        snprintf(item, sizeof(item),
            "%s{\"id\":%d,\"type\":\"%s\",\"asset\":\"%s\",\"amount\":%.6f,\"price\":%.2f,\"total\":%.2f,\"status\":\"%s\",\"timestamp\":\"%s\"}",
            first ? "" : ",", id, et, ea, amount, price, total, es, eTs
        );
        first = 0;

        free(et); free(ea); free(es); free(eTs);

        if (append_str(&json, &cap, &len, item) < 0) { free(json); sqlite3_finalize(stmt); return NULL; }
    }

    sqlite3_finalize(stmt);
    append_str(&json, &cap, &len, "]");
    return json;
}

// -------- portfolio (simple, uses asset totals) --------
char* db_get_chart_json(int user_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT SUM(quantity*current_price) FROM assets WHERE user_id=?;";
    double totalValue = 0.0;

    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, user_id);
        if (sqlite3_step(stmt) == SQLITE_ROW && sqlite3_column_type(stmt, 0) != SQLITE_NULL) {
            totalValue = sqlite3_column_double(stmt, 0);
        }
        sqlite3_finalize(stmt);
    }

    // 10 points
    int days = 10;
    time_t now = time(NULL);
    int cap = 512, len = 0;
    char *json = (char*)malloc(cap);
    if (!json) return NULL;

    append_str(&json, &cap, &len, "[");
    for (int i = 0; i < days; ++i) {
        time_t t = now - (time_t)((days - 1 - i) * 86400);
        struct tm tmv;
        localtime_r(&t, &tmv);
        char datebuf[32];
        strftime(datebuf, sizeof(datebuf), "%Y-%m-%d", &tmv);

        double val = totalValue * (0.9 + 0.02 * i); // simple ramp
        char item[128];
        snprintf(item, sizeof(item), "%s{\"date\":\"%s\",\"value\":%.2f}", (i==0?"":","), datebuf, val);
        if (append_str(&json, &cap, &len, item) < 0) { free(json); return NULL; }
    }
    append_str(&json, &cap, &len, "]");
    return json;
}

char* db_get_allocation_json(int user_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT name,(quantity*current_price) FROM assets WHERE user_id=?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return NULL;
    sqlite3_bind_int(stmt, 1, user_id);

    // sum
    double sum = 0.0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        if (sqlite3_column_type(stmt, 1) != SQLITE_NULL) sum += sqlite3_column_double(stmt, 1);
    }
    sqlite3_reset(stmt);

    const char* colors[] = {"#FF6384","#36A2EB","#FFCE56","#4BC0C0","#9966FF","#00A36C"};
    int ci = 0;

    int cap = 256, len = 0;
    char *json = (char*)malloc(cap);
    if (!json) { sqlite3_finalize(stmt); return NULL; }

    append_str(&json, &cap, &len, "[");
    int first = 1;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *name = (const char*)sqlite3_column_text(stmt, 0);
        double val = sqlite3_column_double(stmt, 1);
        double pct = (sum > 0) ? (val / sum * 100.0) : 0.0;

        char *en = json_escape(name);
        if (!en) { free(json); sqlite3_finalize(stmt); return NULL; }

        char item[256];
        snprintf(item, sizeof(item),
            "%s{\"name\":\"%s\",\"value\":%.2f,\"color\":\"%s\"}",
            first ? "" : ",", en, pct, colors[ci % 6]
        );
        first = 0; ci++;
        free(en);

        if (append_str(&json, &cap, &len, item) < 0) { free(json); sqlite3_finalize(stmt); return NULL; }
    }

    sqlite3_finalize(stmt);
    append_str(&json, &cap, &len, "]");
    return json;
}

// -------- sessions --------
int db_create_session(int user_id, const char *token) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "INSERT OR REPLACE INTO sessions(token, user_id) VALUES(?, ?);";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 2, user_id);
    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_get_user_id_from_session(const char *token, int *out_user_id) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "SELECT user_id FROM sessions WHERE token = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        if (out_user_id) *out_user_id = sqlite3_column_int(stmt, 0);
        sqlite3_finalize(stmt);
        return 0;
    }
    sqlite3_finalize(stmt);
    return -1;
}

int db_delete_session(const char *token) {
    sqlite3_stmt *stmt = NULL;
    const char *sql = "DELETE FROM sessions WHERE token = ?;";
    if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;
    sqlite3_bind_text(stmt, 1, token, -1, SQLITE_TRANSIENT);
    sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return 0;
}
