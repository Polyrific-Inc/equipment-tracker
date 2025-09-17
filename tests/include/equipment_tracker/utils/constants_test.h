// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "equipment_tracker_config.h" // Assuming this is the name of the main file

namespace {

class EquipmentTrackerConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        // No setup needed for this test
    }

    void TearDown() override {
        // No teardown needed for this test
    }
};

TEST_F(EquipmentTrackerConfigTest, DefaultUpdateIntervalIsCorrect) {
    EXPECT_EQ(equipment_tracker::DEFAULT_UPDATE_INTERVAL_MS, 5000);
}

TEST_F(EquipmentTrackerConfigTest, DefaultConnectionTimeoutIsCorrect) {
    EXPECT_EQ(equipment_tracker::DEFAULT_CONNECTION_TIMEOUT_MS, 10000);
}

TEST_F(EquipmentTrackerConfigTest, DefaultPositionAccuracyIsCorrect) {
    EXPECT_DOUBLE_EQ(equipment_tracker::DEFAULT_POSITION_ACCURACY, 2.5);
}

TEST_F(EquipmentTrackerConfigTest, DefaultMaxHistorySizeIsCorrect) {
    EXPECT_EQ(equipment_tracker::DEFAULT_MAX_HISTORY_SIZE, 100);
}

TEST_F(EquipmentTrackerConfigTest, EarthRadiusIsCorrect) {
    EXPECT_DOUBLE_EQ(equipment_tracker::EARTH_RADIUS_METERS, 6371000.0);
}

TEST_F(EquipmentTrackerConfigTest, MovementSpeedThresholdIsCorrect) {
    EXPECT_DOUBLE_EQ(equipment_tracker::MOVEMENT_SPEED_THRESHOLD, 0.5);
}

TEST_F(EquipmentTrackerConfigTest, DefaultDbPathIsCorrect) {
    EXPECT_STREQ(equipment_tracker::DEFAULT_DB_PATH, "equipment_tracker.db");
}

TEST_F(EquipmentTrackerConfigTest, DefaultServerUrlIsCorrect) {
    EXPECT_STREQ(equipment_tracker::DEFAULT_SERVER_URL, "https://tracking.example.com/api");
}

TEST_F(EquipmentTrackerConfigTest, DefaultServerPortIsCorrect) {
    EXPECT_EQ(equipment_tracker::DEFAULT_SERVER_PORT, 8080);
}

TEST_F(EquipmentTrackerConfigTest, ConstantsAreConsistentWithRequirements) {
    // Test that update interval is less than connection timeout
    EXPECT_LT(equipment_tracker::DEFAULT_UPDATE_INTERVAL_MS, 
              equipment_tracker::DEFAULT_CONNECTION_TIMEOUT_MS);
    
    // Test that position accuracy is a reasonable value
    EXPECT_GT(equipment_tracker::DEFAULT_POSITION_ACCURACY, 0.0);
    EXPECT_LT(equipment_tracker::DEFAULT_POSITION_ACCURACY, 10.0); // Reasonable upper bound
    
    // Test that max history size is reasonable
    EXPECT_GT(equipment_tracker::DEFAULT_MAX_HISTORY_SIZE, 0);
    
    // Test that movement speed threshold is reasonable
    EXPECT_GT(equipment_tracker::MOVEMENT_SPEED_THRESHOLD, 0.0);
}

} // namespace
// </test_code>