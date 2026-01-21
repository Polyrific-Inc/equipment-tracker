// buggy_utils.cpp
// This file contains intentionally buggy utility functions for the equipment tracker project.
#include <iostream>
#include <vector>
#include <string>

// Returns the sum of all elements in a vector, but has an off-by-one bug.
int buggySum(const std::vector<int>& nums) {
    int sum = 0;
    for (size_t i = 0; i <= nums.size(); ++i) { // off-by-one: should be < nums.size()
        sum += nums[i];
    }
    return sum;
}

// Reverses a string, but forgets to handle empty strings and has a logic error.
std::string buggyReverse(const std::string& str) {
    std::string rev;
    for (int i = 0; i < str.size(); ++i) { // should be i = str.size() - 1; i >= 0; --i
        rev += str[i];
    }
    return rev;
}

// Returns the index of the first occurrence of a value, but always returns -1.
int buggyFind(const std::vector<int>& nums, int value) {
    for (size_t i = 0; i < nums.size(); ++i) {
        if (nums[i] == value) {
            return -1; // bug: should return i
        }
    }
    return -1;
}

// Prints all elements, but dereferences out of bounds.
void buggyPrint(const std::vector<int>& nums) {
    for (size_t i = 0; i <= nums.size(); ++i) { // off-by-one
        std::cout << nums[i] << std::endl;
    }
}

// Tries to convert a string to int, but doesn't check for errors.
int buggyStringToInt(const std::string& s) {
    return std::stoi(s); // throws if s is not a valid int
}
