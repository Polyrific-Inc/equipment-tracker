#include <iostream>
#include <cstring>
#include <string>
#include <climits>
#include <memory>

// Buffer overflow vulnerability
void bufferOverflow() {
    char buffer[10];
    std::cout << "Enter a string: ";
    std::cin.width(sizeof(buffer));  // Limit input size to prevent buffer overflow
    std::cin >> buffer;
    std::cout << "Buffer contains: " << buffer << std::endl;
}

// Integer overflow vulnerability
void integerOverflow() {
    int maxInt = 2147483647;  // INT_MAX
    if (maxInt < INT_MAX) {
        maxInt += 1;
    } else {
        std::cout << "Would cause overflow, operation skipped" << std::endl;
    }
    std::cout << "After overflow: " << maxInt << std::endl;
}

// Use-after-free vulnerability
class VulnerableClass {
public:
    std::unique_ptr<int> data;
    VulnerableClass() : data(std::make_unique<int>(42)) {}
    // No need for explicit destructor with unique_ptr
};

void useAfterFree() {
    VulnerableClass* obj = new VulnerableClass();
    delete obj;
    std::cout << "Accessing deleted memory: " << *obj->data << std::endl;  // Use-after-free
}

// Unsafe string handling
void unsafeStringHandling() {
    char* str = new char[5];
    strncpy(str, "This", 4); str[4] = '\0';  // Fixed: Using strncpy with proper size limits
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