#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#define CACHE_LINE_SIZE 64

typedef struct {
    int id;
    char *text;
    char *pattern;
    int patternLength;
    int startIndex;
    int endIndex;
    long long *localCount;
} ThreadArgs;

double getRealTime() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000.0 + tv.tv_usec / 1000.0;
}

long long countPattern(const char *text, const char *pattern, int patternLen, int start, int end) {
    long long count = 0;
    
    for (int i = start; i <= end - patternLen; i++) {
        int j;
        for (j = 0; j < patternLen; j++) {
            if (text[i + j] != pattern[j]) {
                break;
            }
        }
        if (j == patternLen) {
            count++;
        }
    }
    return count;
}

// Функция потока для параллельной версии
void *parallelWork(void *_args) {
    ThreadArgs *args = (ThreadArgs *)_args;
    
    args->localCount[args->id] = countPattern(
        args->text, args->pattern, args->patternLength, 
        args->startIndex, args->endIndex
    );
    
    return NULL;
}

long long sequentialSearch(char *text, char *pattern, int patternLength, int textLength) {
    return countPattern(text, pattern, patternLength, 0, textLength);
}

long long parallelSearch(char *text, char *pattern, int patternLength, int textLength, int threadsCount) {
    pthread_t *threads = (pthread_t *)malloc(threadsCount * sizeof(pthread_t));
    ThreadArgs *threadArgs = (ThreadArgs *)malloc(threadsCount * sizeof(ThreadArgs));
    long long *localCounts = (long long *)calloc(threadsCount, sizeof(long long));
    
    if (!threads || !threadArgs || !localCounts) {
        printf("Ошибка выделения памяти\n");
        free(threads);
        free(threadArgs);
        free(localCounts);
        return -1;
    }
    
    int range = textLength / threadsCount;
    long long totalCount = 0;
    
    for (int i = 0; i < threadsCount; i++) {
        threadArgs[i].id = i;
        threadArgs[i].text = text;
        threadArgs[i].pattern = pattern;
        threadArgs[i].patternLength = patternLength;
        threadArgs[i].localCount = localCounts;
        threadArgs[i].startIndex = i * range;
        if (i == threadsCount - 1) {
            threadArgs[i].endIndex = textLength;
        } else {
            threadArgs[i].endIndex = (i + 1) * range;
        }
        
        if (threadArgs[i].endIndex > textLength - patternLength) {
            threadArgs[i].endIndex = textLength - patternLength + 1;
        }
        if (threadArgs[i].startIndex > threadArgs[i].endIndex) {
            threadArgs[i].startIndex = threadArgs[i].endIndex;
        }
        
        if (pthread_create(&threads[i], NULL, parallelWork, &threadArgs[i]) != 0) {
            printf("Ошибка создания потока %d\n", i);
            threadsCount = i;
            break;
        }
    }
    
    for (int i = 0; i < threadsCount; i++) {
        pthread_join(threads[i], NULL);
        totalCount += localCounts[i];
    }
    
    free(threads);
    free(threadArgs);
    free(localCounts);
    
    return totalCount;
}

void generateTestData(char *text, char *pattern, int textLength, int patternLength) {
    const char charset[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 ";
    int charsetLen = strlen(charset);
    
    for (int i = 0; i < textLength; i++) {
        text[i] = charset[rand() % charsetLen];
    }
    text[textLength] = '\0';
    
    for (int i = 0; i < patternLength; i++) {
        pattern[i] = charset[rand() % charsetLen];
    }
    pattern[patternLength] = '\0';
    
    int oneToAdd = (textLength / 10000) + 1;
    for (int i = 0; i < oneToAdd; i++) {
        int guarantee = rand() % (textLength - patternLength);
        memcpy(&text[guarantee], pattern, patternLength);
    }
}

int main(int argc, char **argv) {
    if (argc != 4) {
        printf("Ошибка некорректного ввода\n");
        printf("Использование: %s <длина текста> <длина шаблона> <количество потоков>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int textLength = atoi(argv[1]);
    int patternLength = atoi(argv[2]);
    int threadsCount = atoi(argv[3]);

    if (patternLength > textLength || threadsCount <= 0 || textLength <= 0) {
        printf("Ошибка некорректных параметров\n");
        return 1;
    }

    char *text = (char *)malloc((textLength + 1) * sizeof(char));
    char *pattern = (char *)malloc((patternLength + 1) * sizeof(char));

    if (!text || !pattern) {
        printf("Ошибка выделения памяти\n");
        return 1;
    }

    srand(time(NULL));
    
    generateTestData(text, pattern, textLength, patternLength);

    double startTime, endTime;

    // Последовательная версия
    startTime = getRealTime();
    long long seqResult = sequentialSearch(text, pattern, patternLength, textLength);
    endTime = getRealTime();
    double timeResult1 = endTime - startTime;

    // Параллельная версия
    startTime = getRealTime();
    long long parResult = parallelSearch(text, pattern, patternLength, textLength, threadsCount);
    endTime = getRealTime();
    double timeResult2 = endTime - startTime;

    printf("Последовательное время: %.5f мс\n", timeResult1);
    printf("Параллельное время: %.5f мс\n", timeResult2);
    
    if (timeResult2 > 0) {
        double speedup = timeResult1 / timeResult2;
        double efficiency = (speedup / threadsCount) * 100;
        printf("Ускорение: %.2f\n", speedup);
        printf("Эффективность: %.2f%%\n", efficiency);
    }

    printf("Найдено вхождений (посл): %lld\n", seqResult);
    printf("Найдено вхождений (пар): %lld\n", parResult);

    free(text);
    free(pattern);

    return 0;
}