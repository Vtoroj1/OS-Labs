#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>

int is_composite(int n) {
    if (n <= 3) return 0;
    if (n % 2 == 0 || n % 3 == 0) return 1;
    
    for (int i = 5; i * i <= n; i += 6) {
        if (n % i == 0 || n % (i + 2) == 0)
            return 1;
    }
    return 0;
}

int main() {
    char buffer[1024];
    ssize_t bytes_read;
    int number = 0;
    int is_negative = 0;
    int has_digits = 0;
    
    while ((bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer))) > 0) {
        for (int i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c >= '0' && c <= '9') {
                number = number * 10 + (c - '0');
                has_digits = 1;
            } else if (c == '-' && !has_digits) {
                is_negative = 1;
            } else if (c == '\n' && has_digits) {
                
                if (is_negative) {
                    number = -number;
                }
                
                if (!is_composite(number)) {
                    exit(EXIT_SUCCESS);
                } else {
                    char output[32];
                    int len = 0;
                    int n = number;
                    char temp[32];
                    int temp_len = 0;
                    while (n > 0) {
                        temp[temp_len++] = '0' + (n % 10);
                        n /= 10;
                    }
                    for (int j = temp_len - 1; j >= 0; j--) {
                        output[len++] = temp[j];
                    }
                    output[len++] = '\n';
                    
                    write(STDOUT_FILENO, output, len);
                }
                
                number = 0;
                is_negative = 0;
                has_digits = 0;
            }
        }
    }

    if (has_digits) {
        if (is_negative) {
            number = -number;
        }
        
        if (!is_composite(number)) {
            exit(EXIT_SUCCESS);
        } else {
            char output[32];
            int len = 0;
            int n = number;
            char temp[32];
            int temp_len = 0;
            while (n > 0) {
                temp[temp_len++] = '0' + (n % 10);
                n /= 10;
            }
            for (int j = temp_len - 1; j >= 0; j--) {
                output[len++] = temp[j];
            }
            output[len++] = '\n';
            
            write(STDOUT_FILENO, output, len);
        }
    }
    
    return EXIT_SUCCESS;
}