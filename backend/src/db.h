#ifndef DB_H
#define DB_H

#include "sqlite3.h"

// Init/close
int db_init(const char *db_path);
void db_close(void);
sqlite3* db_get_handle(void);

// Users / auth helpers
int db_check_user(const char *email, const char *password, int *out_user_id, char *out_username, int username_bufsize);
int db_create_user(const char *email, const char *password, const char *username, int *out_user_id);
int db_init_portfolio(int user_id);
char* db_get_user_json(int user_id);

// Assets
char* db_get_assets_json(int user_id);
int db_create_asset(int user_id, const char *symbol, const char *name,
                    double quantity, double current_price, double avg_cost,
                    const char *category, int *out_asset_id);
int db_update_asset(int user_id, int asset_id, const char *symbol, const char *name,
                    double quantity, double current_price, double avg_cost,
                    const char *category);
int db_delete_asset(int user_id, int asset_id);
char* db_get_asset_json(int user_id, int asset_id);

// Transactions
char* db_get_transactions_json(int user_id);
int db_create_transaction(int user_id, const char *type, const char *asset,
                          double amount, double price, double total,
                          const char *status, const char *timestamp_iso,
                          int *out_tx_id);
char* db_get_transaction_json(int user_id, int tx_id);

// Portfolio
char* db_get_chart_json(int user_id);
char* db_get_allocation_json(int user_id);

// Sessions
int db_create_session(int user_id, const char *token);
int db_get_user_id_from_session(const char *token, int *out_user_id);
int db_delete_session(const char *token);

#endif
