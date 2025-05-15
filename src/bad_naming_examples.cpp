#include <iostream>
#include <string>
#include <vector>

// Bad class name - should be PascalCase
class bad_class_name {
private:
    // Bad member variable names - should be camelCase
    int Bad_Variable;
    string USER_NAME;
    double _price;
    vector<int> list_of_numbers;

public:
    // Bad method name - should be camelCase
    void Bad_Method() {
        // Bad local variable names - should be camelCase
        int TEMP_VAR = 10;
        string First_Name = "John";
        double _total = 0.0;
        
        // Bad parameter names in lambda
        auto bad_lambda = [](int X, int Y) {
            return X + Y;
        };
        
        // Bad loop variable names
        for(int I = 0; I < 10; I++) {
            int RESULT = I * 2;
            std::cout << RESULT << std::endl;
        }
    }
    
    // Bad constructor name - should match class name
    bad_class_name() {
        Bad_Variable = 0;
        USER_NAME = "default";
        _price = 0.0;
    }
};

int main() {
    // Bad object name - should be camelCase
    bad_class_name BAD_OBJECT;
    
    // Bad local variable names
    int COUNTER = 0;
    string MESSAGE = "Hello";
    double _value = 3.14;
    
    return 0;
} 