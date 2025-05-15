#include <iostream>
#include <cstring>
#include <string>

// Buffer overflow vulnerability
void bufferOverflow() {
    char buffer[10];
    std::cout << "Enter a string: ";
    std::cin >> buffer;  // No bounds checking
    std::cout << "Buffer contains: " << buffer << std::endl;
}

// Integer overflow vulnerability
void integerOverflow() {
    int maxInt = 2147483647;  // INT_MAX
    maxInt += 1;  // This will cause integer overflow
    std::cout << "After overflow: " << maxInt << std::endl;
}

// Use-after-free vulnerability
class VulnerableClass {
public:
    int* data;
    VulnerableClass() {
        data = new int(42);
    }
    ~VulnerableClass() {
        delete data;
    }
};

void useAfterFree() {
    VulnerableClass* obj = new VulnerableClass();
    delete obj;
    std::cout << "Accessing deleted memory: " << *obj->data << std::endl;  // Use-after-free
}

// Unsafe string handling
void unsafeStringHandling() {
    char* str = new char[5];
    strcpy(str, "This is a very long string that will cause buffer overflow");  // No bounds checking
    std::cout << str << std::endl;
    delete[] str;
}

// Format string vulnerability
void formatStringVuln() {
    char userInput[100];
    std::cout << "Enter a string: ";
    std::cin >> userInput;
    printf(userInput);  // Format string vulnerability
}

// Command injection vulnerability
void commandInjection() {
    std::string userInput;
    std::cout << "Enter a filename: ";
    std::cin >> userInput;
    std::string command = "cat " + userInput;  // Command injection possible
    system(command.c_str());
}

int main() {
    std::cout << "Demonstrating various security vulnerabilities:" << std::endl;
    
    // Uncomment any of these to demonstrate the vulnerabilities
    // bufferOverflow();
    // integerOverflow();
    // useAfterFree();
    // unsafeStringHandling();
    // formatStringVuln();
    // commandInjection();
    
    return 0;
} 