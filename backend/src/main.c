#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <sys/stat.h>

#include "db.h"

void* handle_client(void *arg);

static void ensure_data_dir(void) {
    struct stat st;
    if (stat("data", &st) == -1) {
        mkdir("data", 0755);
    }
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));
    ensure_data_dir();

    int port = 8080;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0) port = 8080;
    }

    if (db_init("data/portfolio.db") != 0) {
        fprintf(stderr, "db_init failed\n");
        return 1;
    }

    // Demo-mode: make sure user_id=1 exists
    int demo_id = -1;
    if (db_create_user("demo@demo.com", "demo", "demo", &demo_id) == 0) {
        // db_init_portfolio(demo_id); // Commented out to start with no stocks
    }

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) { perror("socket"); return 1; }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    if (listen(server_fd, 64) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }

    printf("Backend server is running on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_sock = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_sock < 0) continue;

        int *pclient = (int*)malloc(sizeof(int));
        if (!pclient) { close(client_sock); continue; }
        *pclient = client_sock;

        pthread_t tid;
        pthread_create(&tid, NULL, handle_client, pclient);
        pthread_detach(tid);
    }

    db_close();
    close(server_fd);
    return 0;
}
