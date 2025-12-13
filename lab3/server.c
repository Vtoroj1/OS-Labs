#include "common.h"

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
    
    char info_msg[512];
    int msg_len = snprintf(info_msg, sizeof(info_msg), "Session_id: %s\n" "Shared memory: %s\n" "Ожидание клиента...\n", session_id, shm_name);
    if (msg_len > 0) {
        write(STDOUT_FILENO, info_msg, msg_len);
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        perror("fork failed");
        cleanup_resources(shm_name, sem_client_name, sem_server_name, shared_data, shm_fd, sem_client, sem_server);
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        close(shm_fd);
        
        const char client_header[] = "\nКлиент (дочерний процесс)\n";
        const char prompt[] = "Введите имя файла с числами: ";
        
        write(STDOUT_FILENO, client_header, sizeof(client_header) - 1);
        write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);
        
        char filename[256];
        ssize_t bytes_read = read(STDIN_FILENO, filename, sizeof(filename) - 1);
        
        if (bytes_read <= 0) {
            const char err_msg[] = "Ошибка чтения имени файла\n";
            write(STDERR_FILENO, err_msg, sizeof(err_msg) - 1);
            exit(EXIT_FAILURE);
        }
        
        filename[bytes_read - 1] = '\0';
        
        execl("./client", "client", session_id, filename, NULL);
        
        const char execl_err[] = "execl failed\n";
        write(STDERR_FILENO, execl_err, sizeof(execl_err) - 1);
        exit(EXIT_FAILURE);
    } else {                
        int has_output = 0;
        int early_termination = 0;
        
        while (!shared_data->terminate && !early_termination) {
            if (sem_wait(sem_client) == -1) {
                if (errno == EINTR) continue;
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
            
            if (shared_data->should_terminate_early) {
                early_termination = 1;
            }
            
            if (shared_data->data_size > 0) {
                if (!has_output) {
                    const char received_msg[] = "Найдены составные числа:\n";
                    write(STDOUT_FILENO, received_msg, sizeof(received_msg) - 1);
                    has_output = 1;
                }
                write(STDOUT_FILENO, shared_data->data, shared_data->data_size);
            }
            
            shared_data->client_ready = false;

            if (sem_post(sem_server) == -1) {
                perror("sem_post failed");
                break;
            }
        }
        
        wait(NULL);
        const char server_done[] = "Сервер завершил работу.\n";
        write(STDOUT_FILENO, server_done, sizeof(server_done) - 1);
    }
    
    cleanup_resources(shm_name, sem_client_name, sem_server_name, shared_data, shm_fd, sem_client, sem_server);
    
    return EXIT_SUCCESS;
}