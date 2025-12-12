#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <unistd.h> 
#include <string.h>

typedef float e_func(int x);
typedef float area_func(float a, float b);

int main() {
    void *lib;
    e_func *e;
    area_func *area;
    
    int current_lib = 1;

    if (current_lib == 1) {
        lib = dlopen("./libvar1.so", RTLD_NOW);
    } else {
        lib = dlopen("./libvar2.so", RTLD_NOW);
    }
    
    if (!lib) {
        const char msg[] = "Ошибка загрузки библиотеки\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        return 1;
    }
    
    e = (e_func *)dlsym(lib, "e");
    area = (area_func *)dlsym(lib, "area");
    
    if (!e || !area) {
        const char msg[] = "Ошибка загрузки функций\n";
        write(STDERR_FILENO, msg, sizeof(msg));
        dlclose(lib);
        return 1;
    }
    
    char buffer[256];
    int n;
    
    const char msg[] = "Текущая библиотека - 1\n";
    write(STDOUT_FILENO, msg, sizeof(msg));

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
        
        if (strcmp(buffer, "0") == 0) {
            dlclose(lib);
            current_lib = (current_lib == 1) ? 2 : 1;
            
            if (current_lib == 1) {
                lib = dlopen("./libvar1.so", RTLD_LAZY);
            } else {
                lib = dlopen("./libvar2.so", RTLD_LAZY);
            }
            
            if (!lib) {
                const char msg[] = "Ошибка загрузки библиотеки\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                return 1;
            }
            
            e = (e_func *)dlsym(lib, "e");
            area = (area_func *)dlsym(lib, "area");
            
            if (!e || !area) {
                const char msg[] = "Ошибка загрузки функций\n";
                write(STDERR_FILENO, msg, sizeof(msg));
                dlclose(lib);
                return 1;
            }
            
            char output[256];
            int len = snprintf(output, sizeof(output), "Переключение на библиотеку %d\n", current_lib);
            write(STDOUT_FILENO, output, len);

        } else if (buffer[0] == '1') {
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
    
    if (lib) {
        dlclose(lib);
    }
    return 0;
}