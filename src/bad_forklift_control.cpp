#include <iostream>
#include <string>
#include <vector>
#include <sqlite3.h>

// Bad: Using snake_case for class name instead of PascalCase
class forklift_controller {
private:
    // Bad: Inconsistent naming convention (mix of snake_case and camelCase)
    int current_x;
    int current_y;
    int battery_level;
    std::string forklift_id;
    sqlite3* db;

public:
    // Bad: No input validation
    void move_forklift(int x, int y) {
        // Bad: No safety checks
        current_x = x;
        current_y = y;
        
        // Bad: Direct SQL query without prepared statements
        std::string query = "UPDATE forklift_positions SET x=" + std::to_string(x) + 
                          ", y=" + std::to_string(y) + " WHERE id='" + forklift_id + "'";
        sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
    }

    // Bad: No input validation for inventory
    void update_inventory(int item_id, int quantity) {
        // Bad: Direct SQL query without prepared statements
        std::string query = "INSERT INTO inventory VALUES (" + 
                          std::to_string(item_id) + "," + 
                          std::to_string(quantity) + ")";
        sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
    }

    // Bad: No edge case handling for zone boundaries
    bool is_in_warehouse_zone(int x, int y) {
        // Bad: Hard-coded values and no boundary checks
        if (x > 0 && x < 100 && y > 0 && y < 100) {
            return true;
        }
        return false;
    }

    // Bad: No safety checks for battery level
    void operate_forklift() {
        // Bad: No validation of battery level
        if (battery_level > 0) {
            std::cout << "Operating forklift" << std::endl;
            battery_level -= 1;
        }
    }

    // Bad: No error handling for database operations
    void connect_to_database() {
        // Bad: Direct SQL query without prepared statements
        sqlite3_open("warehouse.db", &db);
        std::string query = "CREATE TABLE IF NOT EXISTS forklift_positions (id TEXT, x INT, y INT)";
        sqlite3_exec(db, query.c_str(), nullptr, nullptr, nullptr);
    }
};

// Bad: Global variables
std::vector<int> warehouse_zones;
int MAX_BATTERY = 100;

// Bad: Function with no input validation
void process_warehouse_data(int* data, int size) {
    for (int i = 0; i < size; i++) {
        // Bad: No bounds checking
        warehouse_zones.push_back(data[i]);
    }
}

int main() {
    // Bad: No error handling
    forklift_controller controller;
    controller.connect_to_database();
    
    // Bad: No validation of coordinates
    controller.move_forklift(150, 200);
    
    // Bad: No validation of inventory quantities
    controller.update_inventory(1, -5);
    
    return 0;
} 