#include <math.h>

//(1 + 1/x)^x
float e(int x) {
    return powf(1.0 + (1.0 / (float)x), (float)x);
}

//Площадь прямоугольника
float area(float a, float b) {
    return a * b;
}