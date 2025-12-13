#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <semaphore.h>
#include <errno.h>

#define BUFFER_SIZE 4096
#define SHM_NAME_PREFIX "/shm_ipc_"
#define SEM_CLIENT_PREFIX "/sem_client_"
#define SEM_SERVER_PREFIX "/sem_server_"

typedef struct {
    char data[BUFFER_SIZE];
    size_t data_size;
    bool client_ready;
    bool server_ready;
    bool terminate;
    bool should_terminate_early;
} shared_data_t;


static inline void generate_shared_name(char* buffer, size_t size, const char* prefix, const char* session_id) {
    if (buffer && prefix && session_id) {
        int written = snprintf(buffer, size, "%s%s", prefix, session_id);
        if (written >= (int)size || written < 0) {
            buffer[size - 1] = '\0';
        }
    }
}

static inline bool is_valid_session_id(const char* session_id) {
    if (!session_id || strlen(session_id) == 0) {
        return false;
    }
    
    for (size_t i = 0; session_id[i] != '\0'; i++) {
        char c = session_id[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-')) {
            return false;
        }
    }
    
    return true;
}

static inline void init_shared_data(shared_data_t* shared_data) {
    if (shared_data) {
        memset(shared_data, 0, sizeof(shared_data_t));
        shared_data->server_ready = true;
    }
}

static inline void create_shm_name(char* buffer, size_t size, const char* session_id) {
    generate_shared_name(buffer, size, SHM_NAME_PREFIX, session_id);
}

static inline void create_sem_client_name(char* buffer, size_t size, const char* session_id) {
    generate_shared_name(buffer, size, SEM_CLIENT_PREFIX, session_id);
}

static inline void create_sem_server_name(char* buffer, size_t size, const char* session_id) {
    generate_shared_name(buffer, size, SEM_SERVER_PREFIX, session_id);
}

static inline void cleanup_resources(const char* shm_name, const char* sem_client_name, const char* sem_server_name, void* mapped_data,
                                   int shm_fd, void* sem_client, void* sem_server) {
    if (sem_client) {
        sem_close((sem_t*)sem_client);
    }
    if (sem_server) {
        sem_close((sem_t*)sem_server);
    }
    
    if (sem_client_name) {
        sem_unlink(sem_client_name);
    }
    if (sem_server_name) {
        sem_unlink(sem_server_name);
    }
    
    if (mapped_data && mapped_data != MAP_FAILED) {
        munmap(mapped_data, sizeof(shared_data_t));
    }
    
    if (shm_fd >= 0) {
        close(shm_fd);
    }
    
    if (shm_name) {
        shm_unlink(shm_name);
    }
}

#endif
