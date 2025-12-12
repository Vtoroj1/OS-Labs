#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <errno.h>

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

void process_data(const char* input, size_t input_size, char* output, size_t* output_size) {
    int number = 0;
    int is_negative = 0;
    int has_digits = 0;
    *output_size = 0;
    
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
            
            if (!is_composite(number)) {
                break;
            } else {
                char num_str[32];
                int len = 0;
                int n = number;
                char temp[32];
                int temp_len = 0;
                while (n > 0) {
                    temp[temp_len++] = '0' + (n % 10);
                    n /= 10;
                }
                for (int j = temp_len - 1; j >= 0; j--) {
                    num_str[len++] = temp[j];
                }
                num_str[len++] = '\n';
                
                if (*output_size + len <= BUFFER_SIZE) {
                    memcpy(output + *output_size, num_str, len);
                    *output_size += len;
                } else {
                    break;
                }
            }
            
            number = 0;
            is_negative = 0;
            has_digits = 0;
        }
    }
}

int main(int argc, char** argv) {
    if (argc != 2) {
        const char msg[] = "Ввод должен быть вида: server <session_id>\n";
        write(STDERR_FILENO, msg, sizeof(msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    const char* session_id = argv[1];
    
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
    
    int shm_fd = shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open failed");
        exit(EXIT_FAILURE);
    }
    
    if (ftruncate(shm_fd, sizeof(shared_data_t)) == -1) {
        perror("ftruncate failed");
        cleanup_resources(shm_name, sem_client_name, sem_server_name, NULL, shm_fd, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    shared_data_t* shared_data = mmap(NULL, sizeof(shared_data_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_data == MAP_FAILED) {
        perror("mmap failed");
        cleanup_resources(shm_name, sem_client_name, sem_server_name, NULL, shm_fd, NULL, NULL);
        exit(EXIT_FAILURE);
    }
    
    init_shared_data(shared_data);
    
    sem_t* sem_client = sem_open(sem_client_name, O_CREAT, 0666, 0);
    sem_t* sem_server = sem_open(sem_server_name, O_CREAT, 0666, 0);
    
    if (sem_client == SEM_FAILED || sem_server == SEM_FAILED) {
        perror("sem_open failed");
        cleanup_resources(shm_name, sem_client_name, sem_server_name, shared_data, shm_fd, sem_client, sem_server);
        exit(EXIT_FAILURE);
    }
    
    char info_msg[256];
    int info_len = snprintf(info_msg, sizeof(info_msg), "Session_id: %s\n" "Shared memory: %s\n" "Ожидание клиента...\n", session_id, shm_name);
    write(STDOUT_FILENO, info_msg, info_len);
    
    while (!shared_data->terminate) {
        if (sem_wait(sem_client) == -1) {
            if (errno == EINTR) continue; // Прервано сигналом
            perror("sem_wait failed");
            break;
        }
        
        if (shared_data->terminate) {
            break;
        }

        if (!shared_data->client_ready) {
            if (sem_post(sem_server) == -1) {
                perror("sem_post failed");
                break;
            }
            continue;
        }
        
        if (shared_data->data_size > 0) {
            char output[BUFFER_SIZE];
            size_t output_size;
            
            process_data(shared_data->data, shared_data->data_size, output, &output_size);
            
            if (output_size > 0) {
                memcpy(shared_data->data, output, output_size);
                shared_data->data_size = output_size;
            } else {
                shared_data->data_size = 0;
            }
        }
        
        shared_data->client_ready = false;

        if (sem_post(sem_server) == -1) {
            perror("sem_post failed");
            break;
        }
    }
    
    cleanup_resources(shm_name, sem_client_name, sem_server_name, shared_data, shm_fd, sem_client, sem_server);
    
    write(STDOUT_FILENO, "Server terminated\n", 19);
    return EXIT_SUCCESS;
}
