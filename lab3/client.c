#include "common.h"

int main(int argc, char** argv) {
    if (argc != 3) {
        const char msg[] = "Ввод должен быть вида: client <session_id> <filename>\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    const char* session_id = argv[1];
    const char* filename = argv[2];
    
    if (!is_valid_session_id(session_id)) {
        const char msg[] = "Некорректный ввод. Можно использовать буквы, цифры, ниж подчеркивание и дефис.\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    char shm_name[256];
    char sem_client_name[256];
    char sem_server_name[256];
    
    create_shm_name(shm_name, sizeof(shm_name), session_id);
    create_sem_client_name(sem_client_name, sizeof(sem_client_name), session_id);
    create_sem_server_name(sem_server_name, sizeof(sem_server_name), session_id);
    
    int shm_fd = shm_open(shm_name, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }
    
    shared_data_t* shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap failed");
        cleanup_resources(NULL, NULL, NULL, NULL, shm_fd, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    sem_t* sem_client = sem_open(sem_client_name, 0);
    sem_t* sem_server = sem_open(sem_server_name, 0);
    
    if (sem_client == SEM_FAILED || sem_server == SEM_FAILED) {
        perror("sem_open failed");
        cleanup_resources(shm_name, sem_client_name, sem_server_name, shared_data, shm_fd, sem_client, sem_server);
        exit(EXIT_FAILURE);
    }
    
    int max_wait = 100; // 100 * 0.1s = 10 секунд
    while (!shared_data->server_ready && max_wait-- > 0) {
        usleep(100000); // 100ms
    }
    
    if (!shared_data->server_ready) {
        const char msg[] = "Ошибка. Сервер не готов спустя 10 секунд\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        cleanup_resources(NULL, NULL, NULL, shared_data, shm_fd, sem_client, sem_server);
        exit(EXIT_FAILURE);
    }
    
    shared_data->client_ready = false;
    if (sem_post(sem_client) == -1) {
        perror("sem_post failed");
        cleanup_resources(NULL, NULL, NULL, shared_data, shm_fd, sem_client, sem_server);
        exit(EXIT_FAILURE);
    }
    
    if (sem_wait(sem_server) == -1) {
        perror("sem_wait failed");
        cleanup_resources(NULL, NULL, NULL, shared_data, shm_fd, sem_client, sem_server);
        exit(EXIT_FAILURE);
    }

    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1) {
        char error_msg[256];
        int len = snprintf(error_msg, sizeof(error_msg), "Error: Failed to open file '%s'\n", filename);
        write(STDERR_FILENO, error_msg, len);
        
        shared_data->terminate = true;
        sem_post(sem_client);
        
        cleanup_resources(NULL, NULL, NULL, shared_data, shm_fd, sem_client, sem_server);
        exit(EXIT_FAILURE);
    }
    
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int processed_any = 0;
    
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        size_t to_copy = bytes_read < BUFFER_SIZE ? bytes_read : BUFFER_SIZE - 1;
        memcpy(shared_data->data, buffer, to_copy);
        shared_data->data_size = to_copy;
        shared_data->client_ready = true;
        
        if (sem_post(sem_client) == -1) {
            perror("sem_post failed");
            break;
        }
        
        if (sem_wait(sem_server) == -1) {
            if (errno == EINTR) continue;
            perror("sem_wait failed");
            break;
        }
        
        if (shared_data->data_size > 0) {
            write(STDOUT_FILENO, shared_data->data, shared_data->data_size);
            processed_any = 1;
        }
        
        shared_data->client_ready = false;
    }
    
    if (!processed_any && bytes_read == 0) {
        const char msg[] = "Нет составных чисел либо файл содержит только завершающие числа.\n";
        write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    }
    
    shared_data->terminate = true;
    if (sem_post(sem_client) == -1) {
        perror("sem_post failed");
    }
    
    close(file_fd);
    
    cleanup_resources(NULL, NULL, NULL, shared_data, shm_fd, sem_client, sem_server);
    
    return EXIT_SUCCESS;
}
