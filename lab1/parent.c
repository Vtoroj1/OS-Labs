#include <unistd.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <fcntl.h>

int main() {
    char filename[256];
    ssize_t bytes_read;
    
    const char prompt[] = "Enter filename: ";
    write(STDOUT_FILENO, prompt, sizeof(prompt) - 1);
    
    bytes_read = read(STDIN_FILENO, filename, sizeof(filename) - 1);
    if (bytes_read <= 0) {
        const char error_msg[] = "Error reading filename\n";
        write(STDERR_FILENO, error_msg, sizeof(error_msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    if (bytes_read > 0 && filename[bytes_read - 1] == '\n') {
        filename[bytes_read - 1] = '\0';
    } else {
        filename[bytes_read] = '\0';
    }
    
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        const char error_msg[] = "Pipe creation failed\n";
        write(STDERR_FILENO, error_msg, sizeof(error_msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    pid_t pid = fork();
    
    if (pid == -1) {
        const char error_msg[] = "Fork failed\n";
        write(STDERR_FILENO, error_msg, sizeof(error_msg) - 1);
        exit(EXIT_FAILURE);
    }
    
    if (pid == 0) {
        close(pipefd[0]);
        
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
        
        int file_fd = open(filename, O_RDONLY);
        if (file_fd == -1) {
            const char error_msg[] = "Error opening file\n";
            write(STDERR_FILENO, error_msg, sizeof(error_msg) - 1);
            exit(EXIT_FAILURE);
        }
        
        dup2(file_fd, STDIN_FILENO);
        close(file_fd);
        
        execl("./child", "child", NULL);
        
        const char error_msg[] = "Exec failed\n";
        write(STDERR_FILENO, error_msg, sizeof(error_msg) - 1);
        exit(EXIT_FAILURE);
        
    } else {
        close(pipefd[1]);
        
        char buffer[1024];
        ssize_t bytes;
        
        while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) > 0) {
            write(STDOUT_FILENO, buffer, bytes);
        }
        
        close(pipefd[0]);
        wait(NULL);
    }
    
    return EXIT_SUCCESS;
}