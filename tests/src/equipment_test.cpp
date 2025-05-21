// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <optional>
#include <cmath>

// Mock or include the actual classes and enums used in the code
namespace equipment_tracker {

enum class EquipmentStatus { Inactive, Active, Maintenance, Unknown };
enum class EquipmentType { Forklift, Crane, Bulldozer, Excavator, Truck, Other };

struct Position {
    double x;
    double y;
    std::chrono::system_clock::time_point timestamp;

    Position(double x_val, double y_val, std::chrono::system_clock::time_point ts)
        : x(x_val), y(y_val), timestamp(ts) {}

    std::chrono::system_clock::time_point getTimestamp() const {
        return timestamp;
    }

    double distanceTo(const Position &other) const {
        return std::sqrt((x - other.x)*(x - other.x) + (y - other.y)*(y - other.y));
    }
};

const double MOVEMENT_SPEED_THRESHOLD = 0.5; // meters per second

class Equipment {
public:
    using EquipmentId = int;

    Equipment(EquipmentId id, EquipmentType type, std::string name);
    Equipment(Equipment &&other) noexcept;
    Equipment &operator=(Equipment &&other) noexcept;

    std::optional<Position> getLastPosition() const;
    void setStatus(EquipmentStatus status);
    void setName(const std::string &name);
    void setLastPosition(const Position &position);
    void recordPosition(const Position &position);
    std::vector<Position> getPositionHistory() const;
    void clearPositionHistory();
    bool isMoving() const;
    std::string toString() const;

private:
    EquipmentId id_;
    EquipmentType type_;
    std::string name_;
    mutable std::mutex mutex_;
    EquipmentStatus status_;
    Position last_position_{0,0,std::chrono::system_clock::now()};
    std::vector<Position> position_history_;
    size_t max_history_size_ = 10;
};

} // namespace equipment_tracker

// Begin test code
// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <mutex>
#include <vector>
#include <string>
#include <chrono>
#include <optional>
#include <cmath>

using namespace equipment_tracker;

class EquipmentTest : public ::testing::Test {
protected:
    Equipment::EquipmentId test_id = 1;
    EquipmentType test_type = EquipmentType::Forklift;
    std::string test_name = "Test Equipment";

    Equipment createEquipment() {
        return Equipment(test_id, test_type, test_name);
    }

    Position createPosition(double x, double y, int seconds_offset) {
        auto now = std::chrono::system_clock::now();
        return Position(x, y, now + std::chrono::seconds(seconds_offset));
    }
};

TEST_F(EquipmentTest, ConstructorInitializesCorrectly) {
    Equipment eq = createEquipment();
    EXPECT_EQ(eq.getLastPosition().has_value(), true);
    EXPECT_EQ(eq.toString().find("Equipment(id=1") != std::string::npos, true);
}

TEST_F(EquipmentTest, SettersUpdateFields) {
    Equipment eq = createEquipment();
    eq.setStatus(EquipmentStatus::Active);
    eq.setName("New Name");
    Position pos = createPosition(10.0, 20.0, 0);
    eq.setLastPosition(pos);
    auto lastPosOpt = eq.getLastPosition();
    ASSERT_TRUE(lastPosOpt.has_value());
    EXPECT_DOUBLE_EQ(lastPosOpt->x, 10.0);
    EXPECT_DOUBLE_EQ(lastPosOpt->y, 20.0);
}

TEST_F(EquipmentTest, RecordPositionAddsToHistoryAndUpdatesLastPosition) {
    Equipment eq = createEquipment();
    Position pos1 = createPosition(1.0, 1.0, 0);
    Position pos2 = createPosition(2.0, 2.0, 1);
    eq.recordPosition(pos1);
    eq.recordPosition(pos2);
    auto history = eq.getPositionHistory();
    EXPECT_EQ(history.size(), 2);
    EXPECT_DOUBLE_EQ(history.back().x, 2.0);
    auto lastPosOpt = eq.getLastPosition();
    ASSERT_TRUE(lastPosOpt.has_value());
    EXPECT_DOUBLE_EQ(lastPosOpt->x, 2.0);
}

TEST_F(EquipmentTest, PositionHistoryRespectsMaxSize) {
    Equipment eq = createEquipment();
    eq.max_history_size_ = 3; // set small max size
    for (int i = 0; i < 5; ++i) {
        eq.recordPosition(createPosition(i, i, i));
    }
    auto history = eq.getPositionHistory();
    EXPECT_LE(history.size(), 3);
}

TEST_F(EquipmentTest, ClearPositionHistoryEmptiesHistory) {
    Equipment eq = createEquipment();
    eq.recordPosition(createPosition(1, 1, 0));
    eq.recordPosition(createPosition(2, 2, 1));
    eq.clearPositionHistory();
    EXPECT_TRUE(eq.getPositionHistory().empty());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWithLessThanTwoPositions) {
    Equipment eq = createEquipment();
    eq.recordPosition(createPosition(0, 0, 0));
    EXPECT_FALSE(eq.isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWhenTimeDiffLessThanOneSecond) {
    Equipment eq = createEquipment();
    auto now = std::chrono::system_clock::now();
    Position pos1(0, 0, now);
    Position pos2(1, 1, now + std::chrono::milliseconds(500));
    eq.recordPosition(pos1);
    eq.recordPosition(pos2);
    EXPECT_FALSE(eq.isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsTrueWhenSpeedAboveThreshold) {
    Equipment eq = createEquipment();
    auto now = std::chrono::system_clock::now();
    Position pos1(0, 0, now);
    // Distance ~1.414, time 1 sec, speed ~1.414 > 0.5
    Position pos2(1.0, 1.0, now + std::chrono::seconds(1));
    eq.recordPosition(pos1);
    eq.recordPosition(pos2);
    EXPECT_TRUE(eq.isMoving());
}

TEST_F(EquipmentTest, IsMovingReturnsFalseWhenSpeedBelowThreshold) {
    Equipment eq = createEquipment();
    auto now = std::chrono::system_clock::now();
    Position pos1(0, 0, now);
    // Distance ~0.1414, time 1 sec, speed ~0.1414 < 0.5
    Position pos2(0.1, 0.1, now + std::chrono::seconds(1));
    eq.recordPosition(pos1);
    eq.recordPosition(pos2);
    EXPECT_FALSE(eq.isMoving());
}

TEST_F(EquipmentTest, ToStringReturnsCorrectString) {
    Equipment eq = createEquipment();
    eq.setStatus(EquipmentStatus::Maintenance);
    std::string str = eq.toString();
    EXPECT_NE(str.find("Equipment(id=1"), std::string::npos);
    EXPECT_NE(str.find("name=Test Equipment"), std::string::npos);
    EXPECT_NE(str.find("status=Maintenance"), std::string::npos);
}

TEST_F(EquipmentTest, MoveConstructorTransfersDataAndResetsSource) {
    Equipment eq1 = createEquipment();
    eq1.setStatus(EquipmentStatus::Active);
    eq1.recordPosition(createPosition(5.0, 5.0, 0));
    Equipment eq2(std::move(eq1));
    EXPECT_EQ(eq2.getLastPosition()->x, 5.0);
    EXPECT_EQ(eq2.toString().find("Active") != std::string::npos, true);
    // eq1 should be reset to Unknown status
    auto status_str = eq1.toString();
    EXPECT_NE(status_str.find("Unknown"), std::string::npos);
}

TEST_F(EquipmentTest, MoveAssignmentTransfersDataAndResetsSource) {
    Equipment eq1 = createEquipment();
    eq1.setStatus(EquipmentStatus::Active);
    eq1.recordPosition(createPosition(5.0, 5.0, 0));
    Equipment eq2 = createEquipment();
    eq2 = std::move(eq1);
    EXPECT_EQ(eq2.getLastPosition()->x, 5.0);
    EXPECT_NE(eq2.toString().find("Active"), std::string::npos);
    auto status_str = eq1.toString();
    EXPECT_NE(status_str.find("Unknown"), std::string::npos);
}
// </test_code>