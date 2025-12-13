#include "common.h"

int is_composite(int n) {
    if (n <= 3) return 0;
    if (n % 2 == 0 || n % 3 == 0) return 1;
    
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 1;
    }
    return 0;
}

int process_data(const char* input, size_t input_size, char* output, size_t* output_size, int* found_invalid) {
    int number = 0;
    int is_negative = 0;
    int has_digits = 0;
    *output_size = 0;
    *found_invalid = 0;
    
    for (size_t i = 0; i < input_size; i++) {
        char c = input[i];
        if (c >= '0' && c <= '9') {
            number = number * 10 + (c - '0');
            has_digits = 1;
        } else if (c == '-' && !has_digits) {
            is_negative = 1;
        } else if ((c == '\n' || i == input_size - 1) && has_digits) {
            if (is_negative) {
                number = -number;
            }
            
            if (is_composite(number)) {
                char num_str[32];
                int len = snprintf(num_str, sizeof(num_str), "%d\n", number);
                
                if (len > 0 && *output_size + len <= BUFFER_SIZE) {
                    memcpy(output + *output_size, num_str, len);
                    *output_size += len;
                } else {
                    break;
                }
            } else {
                *found_invalid = 1;
                return 0; 
            }
            
            number = 0;
            is_negative = 0;
            has_digits = 0;
        }
    }
    
    return 1;
}

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
        char err_msg[256];
        int len = snprintf(err_msg, sizeof(err_msg), "Ошибка: Shared memory не найдена. Сначала запустите сервер.\n");
        write(STDERR_FILENO, err_msg, len);
        exit(EXIT_FAILURE);
    }
    
    shared_data_t* shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap failed");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }
    
    sem_t* sem_client = sem_open(sem_client_name, 0);
    sem_t* sem_server = sem_open(sem_server_name, 0);
    
    if (sem_client == SEM_FAILED || sem_server == SEM_FAILED) {
        perror("sem_open failed");
        munmap(shared_data, sizeof(shared_data_t));
        close(shm_fd);
        exit(EXIT_FAILURE);
    }
    
    int max_wait = 100; // 100 * 0.1s = 10 секунд
    while (!shared_data->server_ready && max_wait-- > 0) {
        usleep(100000); // 100ms
    }
    
    if (!shared_data->server_ready) {
        const char timeout_msg[] = "Ошибка: Сервер не готов спустя 10 секунд\n";
        write(STDERR_FILENO, timeout_msg, sizeof(timeout_msg) - 1);
        munmap(shared_data, sizeof(shared_data_t));
        close(shm_fd);
        sem_close(sem_client);
        sem_close(sem_server);
        exit(EXIT_FAILURE);
    }
    
    
    int file_fd = open(filename, O_RDONLY);
    if (file_fd == -1) {
        char err_msg[256];
        int len = snprintf(err_msg, sizeof(err_msg), 
                          "Ошибка: Не удалось открыть файл '%s'\n", filename);
        write(STDERR_FILENO, err_msg, len);
        
        shared_data->terminate = true;
        sem_post(sem_client);
        
        munmap(shared_data, sizeof(shared_data_t));
        close(shm_fd);
        sem_close(sem_client);
        sem_close(sem_server);
        exit(EXIT_FAILURE);
    }
    
    char buffer[BUFFER_SIZE];
    char processed_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    int should_terminate = 0;
    int found_composite_numbers = 0;
    
    while (!should_terminate && (bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        size_t output_size;
        int found_invalid;
        
        int result = process_data(buffer, bytes_read, processed_buffer, &output_size, &found_invalid);
        
        if (found_invalid) {
            should_terminate = 1;
            shared_data->should_terminate_early = true;
        }
        
        if (output_size > 0) {
            memcpy(shared_data->data, processed_buffer, output_size);
            shared_data->data_size = output_size;
            shared_data->client_ready = true;
            found_composite_numbers = 1;
            
            if (sem_post(sem_client) == -1) {
                perror("sem_post failed");
                break;
            }
            
            if (sem_wait(sem_server) == -1) {
                if (errno == EINTR) continue;
                perror("sem_wait failed");
                break;
            }
            
            shared_data->client_ready = false;
        }
        
        if (!result) {
            break;
        }
    }
    
    close(file_fd);
    
    shared_data->terminate = true;
    if (sem_post(sem_client) == -1) {
        perror("sem_post failed");
    }
    
    cleanup_resources(shm_name, sem_client_name, sem_server_name, shared_data, shm_fd, sem_client, sem_server);

    
    return EXIT_SUCCESS;
}