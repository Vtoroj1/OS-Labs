//Сумма ряда 1/n!
float e(int x) {
    float result = 0.0;
    float factorial = 1.0;
    
    for (int n = 0; n <= x; n++) {
        if (n > 0) {
            factorial *= n;
        }
        result += 1.0 / factorial;
    }
    return result;
}

//Площадь прямоугольного треугольника
float area(float a, float b) {
    return 0.5 * a * b;
}