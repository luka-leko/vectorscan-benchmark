#include <iostream>
using namespace std;

void test_pointer_wrong(int *p) {
    cout << "\tInside before: " << p << endl;
    p = p + 1;
    cout << "\tInside after:  " << p << endl;
}

int main() {
    int x = 10;
    cout << "Outside before:  " << x << endl;
    test_pointer_wrong(&x);
    cout << "Outside after:  " << x << endl;
}