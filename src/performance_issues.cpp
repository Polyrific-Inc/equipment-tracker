#include <iostream>
#include <vector>
#include <string>
#include <memory>
#include <chrono>
#include <thread>

// Global variables - bad practice
std::vector<int> globalVector;
std::string globalString;

// Inefficient class with unnecessary copying
class InefficientClass {
private:
    std::vector<int> data;
    std::string name;

public:
    // Inefficient constructor with unnecessary copying
    InefficientClass(std::vector<int> inputData, std::string inputName) {
        data = inputData;  // Unnecessary copy
        name = inputName;  // Unnecessary copy
    }

    // Inefficient getter with unnecessary copying
    std::vector<int> getData() {
        return data;  // Returns a copy instead of reference
    }

    // Inefficient string concatenation
    std::string getFullName() {
        std::string result;
        for (int i = 0; i < 1000; i++) {
            result += name + " " + std::to_string(i);  // Inefficient string concatenation
        }
        return result;
    }
};

// Function with memory leak - fixed with smart pointer
void createMemoryLeak() {
    std::unique_ptr<int[]> ptr = std::make_unique<int[]>(1000);
    // Memory automatically freed when ptr goes out of scope
}

// Inefficient sorting function
void inefficientSort(std::vector<int>& arr) {
    // Using bubble sort for large arrays - very inefficient
    for (size_t i = 0; i < arr.size(); i++) {
        for (size_t j = 0; j < arr.size() - 1; j++) {
            if (arr[j] > arr[j + 1]) {
                std::swap(arr[j], arr[j + 1]);
            }
        }
    }
}

// Function with unnecessary object creation
void processData() {
    std::vector<int> temp;
    temp.reserve(1000000);  // Pre-allocate memory to avoid reallocations
    for (int i = 0; i < 1000000; i++) {
        temp.push_back(i);
    }
    // Alternative: use constructor for even better performance than before
    // std::vector<int> temp(1000000);
    // std::iota(temp.begin(), temp.end(), 0);
}

// Main function demonstrating the issues
int main() {
    // Unnecessary object creation
    std::vector<int> largeVector;
    for (int i = 0; i < 1000000; i++) {
        largeVector.push_back(i);  // Frequent reallocations
    }

    // Inefficient string operations
    std::string result;
    for (int i = 0; i < 10000; i++) {
        result += std::to_string(i);  // Inefficient string concatenation
    }

    // Unnecessary copying
    std::vector<int> copyVector = largeVector;  // Unnecessary deep copy

    // Inefficient sorting
    inefficientSort(largeVector);

    // Memory leak
    createMemoryLeak();

    // Unnecessary thread creation
    for (int i = 0; i < 100; i++) {
        std::thread t([]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        });
        t.join();  // Creating and destroying threads frequently
    }

    return 0;
}