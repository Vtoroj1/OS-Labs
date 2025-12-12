#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "func.h"

int main() {
    char buffer[256];
    int n;
    
    while (1) {
        n = read(STDIN_FILENO, buffer, sizeof(buffer) - 1);
        if (n <= 0) break;
        
        buffer[n] = '\0';
        
        if (buffer[n-1] == '\n') {
            buffer[n-1] = '\0';
        }
        
        if (strcmp(buffer, "exit") == 0) {
            break;
        }
        
        if (buffer[0] == '1') {
            int x;
            if (sscanf(buffer + 1, "%d", &x) == 1) {
                if (x > 0) {
                    float result = e(x);
                    char output[256];
                    int len = snprintf(output, sizeof(output), "e(%d) = %f\n", x, result);
                    write(STDOUT_FILENO, output, len);
                } else {
                    const char msg[] = "Ошибка. x должен быть положительным\n";
                    write(STDERR_FILENO, msg, sizeof(msg));
                }
            } else {
                const char msg[] = "Неверный формат команды. Используйте: 1 x\n";
                write(STDERR_FILENO, msg, sizeof(msg));
            }
        } else if (buffer[0] == '2') {
            float a, b;
            if (sscanf(buffer + 1, "%f %f", &a, &b) == 2) {
                if (a > 0 && b > 0) {
                    float result = area(a, b);
                    char output[256];
                    int len = snprintf(output, sizeof(output), "area(%.2f, %.2f) = %.2f\n", a, b, result);
                    write(STDOUT_FILENO, output, len);
                } else {
                    const char msg[] = "Ошибка. a и b должны быть положительными\n";
                    write(STDERR_FILENO, msg, sizeof(msg));
                }
            } else {
                const char msg[] = "Неверный формат команды. Используйте: 2 a b\n";
                write(STDERR_FILENO, msg, sizeof(msg));
            }
        } else {
            const char msg[] = "Неизвестная команда\n";
            write(STDERR_FILENO, msg, sizeof(msg));
        }
    }
    
    return 0;
}