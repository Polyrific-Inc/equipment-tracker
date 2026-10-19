// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>

#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace equipment_tracker;

// ============================================================================
// Helper utilities
// ============================================================================

// Create a fixed timestamp for deterministic tests
static Timestamp makeTimestamp(int year, int month, int day,
                               int hour = 0, int minute = 0, int second = 0)
{
    std::tm tm = {};
    tm.tm_year = year - 1900;
    tm.tm_mon  = month - 1;
    tm.tm_mday = day;
    tm.tm_hour = hour;
    tm.tm_min  = minute;
    tm.tm_sec  = second;
    tm.tm_isdst = -1;
    auto time_t_val = std::mktime(&tm);
    return std::chrono::system_clock::from_time_t(time_t_val);
}

// ============================================================================
// Default constructor tests
// ============================================================================

class PositionDefaultConstructorTest : public ::testing::Test {};

TEST_F(PositionDefaultConstructorTest, DefaultLatitudeIsZero)
{
    Position pos;
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 0.0);
}

TEST_F(PositionDefaultConstructorTest, DefaultLongitudeIsZero)
{
    Position pos;
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 0.0);
}

TEST_F(PositionDefaultConstructorTest, DefaultAltitudeIsZero)
{
    Position pos;
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 0.0);
}

TEST_F(PositionDefaultConstructorTest, DefaultAccuracyIsConstant)
{
    Position pos;
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), DEFAULT_POSITION_ACCURACY);
}

// ============================================================================
// Parameterized constructor tests
// ============================================================================

class PositionConstructorTest : public ::testing::Test {};

TEST_F(PositionConstructorTest, ConstructWithLatLon)
{
    Position pos(40.7128, -74.0060);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 40.7128);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -74.0060);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), DEFAULT_POSITION_ACCURACY);
}

TEST_F(PositionConstructorTest, ConstructWithLatLonAlt)
{
    Position pos(34.0522, -118.2437, 100.5);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 34.0522);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -118.2437);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 100.5);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), DEFAULT_POSITION_ACCURACY);
}

TEST_F(PositionConstructorTest, ConstructWithLatLonAltAccuracy)
{
    Position pos(51.5074, -0.1278, 50.0, 5.0);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 51.5074);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -0.1278);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 50.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 5.0);
}

TEST_F(PositionConstructorTest, ConstructWithAllParameters)
{
    auto ts = makeTimestamp(2025, 6, 15, 12, 30, 0);
    Position pos(48.8566, 2.3522, 35.0, 1.5, ts);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 48.8566);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 2.3522);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 35.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 1.5);
    EXPECT_EQ(pos.getTimestamp(), ts);
}

TEST_F(PositionConstructorTest, ConstructWithNegativeLatitude)
{
    Position pos(-33.8688, 151.2093);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), -33.8688);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 151.2093);
}

TEST_F(PositionConstructorTest, ConstructWithNegativeLongitude)
{
    Position pos(40.7128, -74.0060);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -74.0060);
}

TEST_F(PositionConstructorTest, ConstructWithNegativeAltitude)
{
    // Dead Sea is ~-430m below sea level
    Position pos(31.5, 35.5, -430.0);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), -430.0);
}

TEST_F(PositionConstructorTest, ConstructWithZeroAccuracy)
{
    Position pos(0.0, 0.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 0.0);
}

// ============================================================================
// Setter tests
// ============================================================================

class PositionSetterTest : public ::testing::Test
{
protected:
    Position pos;
};

TEST_F(PositionSetterTest, SetLatitude)
{
    pos.setLatitude(45.0);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 45.0);
}

TEST_F(PositionSetterTest, SetLongitude)
{
    pos.setLongitude(-120.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -120.0);
}

TEST_F(PositionSetterTest, SetAltitude)
{
    pos.setAltitude(8848.86);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 8848.86);
}

TEST_F(PositionSetterTest, SetAccuracy)
{
    pos.setAccuracy(10.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 10.0);
}

TEST_F(PositionSetterTest, SetTimestamp)
{
    auto ts = makeTimestamp(2024, 1, 1, 0, 0, 0);
    pos.setTimestamp(ts);
    EXPECT_EQ(pos.getTimestamp(), ts);
}

TEST_F(PositionSetterTest, SetLatitudeNegative)
{
    pos.setLatitude(-90.0);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), -90.0);
}

TEST_F(PositionSetterTest, SetLongitudePositive180)
{
    pos.setLongitude(180.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 180.0);
}

TEST_F(PositionSetterTest, SetLongitudeNegative180)
{
    pos.setLongitude(-180.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -180.0);
}

TEST_F(PositionSetterTest, SetAltitudeNegative)
{
    pos.setAltitude(-100.0);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), -100.0);
}

// ============================================================================
// Boundary / edge-case coordinate tests
// ============================================================================

class PositionBoundaryTest : public ::testing::Test {};

TEST_F(PositionBoundaryTest, NorthPole)
{
    Position pos(90.0, 0.0);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 90.0);
}

TEST_F(PositionBoundaryTest, SouthPole)
{
    Position pos(-90.0, 0.0);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), -90.0);
}

TEST_F(PositionBoundaryTest, InternationalDateLinePositive)
{
    Position pos(0.0, 180.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 180.0);
}

TEST_F(PositionBoundaryTest, InternationalDateLineNegative)
{
    Position pos(0.0, -180.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -180.0);
}

TEST_F(PositionBoundaryTest, Origin)
{
    Position pos(0.0, 0.0, 0.0, 0.0);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 0.0);
}

TEST_F(PositionBoundaryTest, VeryLargeAltitude)
{
    // ISS orbit altitude
    Position pos(0.0, 0.0, 408000.0);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 408000.0);
}

TEST_F(PositionBoundaryTest, VerySmallAccuracy)
{
    Position pos(0.0, 0.0, 0.0, 0.001);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 0.001);
}

TEST_F(PositionBoundaryTest, VeryLargeAccuracy)
{
    Position pos(0.0, 0.0, 0.0, 1000000.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 1000000.0);
}

// ============================================================================
// distanceTo tests (Haversine formula)
// ============================================================================

class PositionDistanceTest : public ::testing::Test {};

TEST_F(PositionDistanceTest, SamePositionDistanceIsZero)
{
    Position pos1(40.7128, -74.0060);
    Position pos2(40.7128, -74.0060);
    EXPECT_NEAR(pos1.distanceTo(pos2), 0.0, 1e-6);
}

TEST_F(PositionDistanceTest, DistanceIsSymmetric)
{
    Position pos1(40.7128, -74.0060);  // New York
    Position pos2(51.5074, -0.1278);   // London
    double d1 = pos1.distanceTo(pos2);
    double d2 = pos2.distanceTo(pos1);
    EXPECT_NEAR(d1, d2, 1e-6);
}

TEST_F(PositionDistanceTest, NewYorkToLondon)
{
    // Known distance: ~5570 km
    Position nyc(40.7128, -74.0060);
    Position london(51.5074, -0.1278);
    double distance = nyc.distanceTo(london);
    EXPECT_NEAR(distance, 5570000.0, 30000.0);  // within 30 km tolerance
}

TEST_F(PositionDistanceTest, EquatorShortDistance)
{
    // Two points on the equator, 1 degree apart
    // At the equator, 1 degree of longitude ≈ 111,320 meters
    Position pos1(0.0, 0.0);
    Position pos2(0.0, 1.0);
    double distance = pos1.distanceTo(pos2);
    EXPECT_NEAR(distance, 111320.0, 500.0);
}

TEST_F(PositionDistanceTest, NorthPoleToSouthPole)
{
    // Half the circumference of Earth ≈ 20,015 km
    Position north(90.0, 0.0);
    Position south(-90.0, 0.0);
    double distance = north.distanceTo(south);
    EXPECT_NEAR(distance, M_PI * EARTH_RADIUS_METERS, 100.0);
}

TEST_F(PositionDistanceTest, SameLatitudeDifferentLongitude)
{
    Position pos1(45.0, 10.0);
    Position pos2(45.0, 11.0);
    double distance = pos1.distanceTo(pos2);
    // At 45° latitude, 1 degree of longitude ≈ 78,710 m
    EXPECT_NEAR(distance, 78710.0, 500.0);
}

TEST_F(PositionDistanceTest, SameLongitudeDifferentLatitude)
{
    Position pos1(10.0, 0.0);
    Position pos2(11.0, 0.0);
    double distance = pos1.distanceTo(pos2);
    // 1 degree of latitude ≈ 111,195 m
    EXPECT_NEAR(distance, 111195.0, 500.0);
}

TEST_F(PositionDistanceTest, VeryClosePoints)
{
    // Two points ~11 meters apart
    Position pos1(0.0, 0.0);
    Position pos2(0.0001, 0.0);
    double distance = pos1.distanceTo(pos2);
    EXPECT_NEAR(distance, 11.1, 1.0);
}

TEST_F(PositionDistanceTest, AntipodalPoints)
{
    // Diametrically opposite points on the equator
    Position pos1(0.0, 0.0);
    Position pos2(0.0, 180.0);
    double distance = pos1.distanceTo(pos2);
    EXPECT_NEAR(distance, M_PI * EARTH_RADIUS_METERS, 100.0);
}

TEST_F(PositionDistanceTest, CrossingDateLine)
{
    // Points on either side of the International Date Line
    Position pos1(0.0, 179.0);
    Position pos2(0.0, -179.0);
    double distance = pos1.distanceTo(pos2);
    // Should be ~222 km (2 degrees at equator)
    EXPECT_NEAR(distance, 222640.0, 1000.0);
}

TEST_F(PositionDistanceTest, DistanceToSelfIsZeroForDefaultPosition)
{
    Position pos;
    EXPECT_NEAR(pos.distanceTo(pos), 0.0, 1e-6);
}

TEST_F(PositionDistanceTest, DistanceIgnoresAltitude)
{
    // Haversine formula does not account for altitude
    Position pos1(40.7128, -74.0060, 0.0);
    Position pos2(40.7128, -74.0060, 10000.0);
    EXPECT_NEAR(pos1.distanceTo(pos2), 0.0, 1e-6);
}

TEST_F(PositionDistanceTest, DistanceIgnoresAccuracy)
{
    Position pos1(40.7128, -74.0060, 0.0, 1.0);
    Position pos2(40.7128, -74.0060, 0.0, 100.0);
    EXPECT_NEAR(pos1.distanceTo(pos2), 0.0, 1e-6);
}

TEST_F(PositionDistanceTest, DistanceIsNonNegative)
{
    Position pos1(40.7128, -74.0060);
    Position pos2(-33.8688, 151.2093);
    EXPECT_GE(pos1.distanceTo(pos2), 0.0);
}

TEST_F(PositionDistanceTest, TriangleInequality)
{
    Position a(0.0, 0.0);
    Position b(10.0, 10.0);
    Position c(20.0, 0.0);
    double ab = a.distanceTo(b);
    double bc = b.distanceTo(c);
    double ac = a.distanceTo(c);
    EXPECT_LE(ac, ab + bc + 1e-6);
}

// ============================================================================
// toString tests
// ============================================================================

class PositionToStringTest : public ::testing::Test {};

TEST_F(PositionToStringTest, ContainsLatitude)
{
    Position pos(40.712800, -74.006000);
    std::string str = pos.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("lat=40.712800"));
}

TEST_F(PositionToStringTest, ContainsLongitude)
{
    Position pos(40.712800, -74.006000);
    std::string str = pos.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("lon=-74.006000"));
}

TEST_F(PositionToStringTest, ContainsAltitude)
{
    Position pos(40.712800, -74.006000, 100.50);
    std::string str = pos.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("alt=100.50"));
}

TEST_F(PositionToStringTest, ContainsAccuracy)
{
    Position pos(40.712800, -74.006000, 0.0, 5.25);
    std::string str = pos.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("acc=5.25"));
}

TEST_F(PositionToStringTest, StartsWithPositionPrefix)
{
    Position pos(0.0, 0.0);
    std::string str = pos.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("Position("));
}

TEST_F(PositionToStringTest, EndsWithClosingParen)
{
    Position pos(0.0, 0.0);
    std::string str = pos.toString();
    EXPECT_EQ(str.back(), ')');
}

TEST_F(PositionToStringTest, ContainsTimeField)
{
    Position pos(0.0, 0.0);
    std::string str = pos.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("time="));
}

TEST_F(PositionToStringTest, NegativeCoordinatesFormatted)
{
    Position pos(-33.868800, -151.209300);
    std::string str = pos.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("lat=-33.868800"));
    EXPECT_THAT(str, ::testing::HasSubstr("lon=-151.209300"));
}

TEST_F(PositionToStringTest, ZeroAltitudeFormatted)
{
    Position pos(0.0, 0.0, 0.0);
    std::string str = pos.toString();
    EXPECT_THAT(str, ::testing::HasSubstr("alt=0.00"));
}

TEST_F(PositionToStringTest, DefaultAccuracyFormatted)
{
    Position pos(0.0, 0.0);
    std::string str = pos.toString();
    std::stringstream expected;
    expected << "acc=" << std::fixed << std::setprecision(2) << DEFAULT_POSITION_ACCURACY;
    EXPECT_THAT(str, ::testing::HasSubstr(expected.str()));
}

// ============================================================================
// Builder tests
// ============================================================================

class PositionBuilderTest : public ::testing::Test {};

TEST_F(PositionBuilderTest, BuildDefaultPosition)
{
    Position pos = Position::builder().build();
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), DEFAULT_POSITION_ACCURACY);
}

TEST_F(PositionBuilderTest, BuildWithLatitude)
{
    Position pos = Position::builder()
                       .withLatitude(40.7128)
                       .build();
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 40.7128);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 0.0);
}

TEST_F(PositionBuilderTest, BuildWithLongitude)
{
    Position pos = Position::builder()
                       .withLongitude(-74.0060)
                       .build();
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -74.0060);
}

TEST_F(PositionBuilderTest, BuildWithAltitude)
{
    Position pos = Position::builder()
                       .withAltitude(500.0)
                       .build();
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 500.0);
}

TEST_F(PositionBuilderTest, BuildWithAccuracy)
{
    Position pos = Position::builder()
                       .withAccuracy(10.0)
                       .build();
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 10.0);
}

TEST_F(PositionBuilderTest, BuildWithTimestamp)
{
    auto ts = makeTimestamp(2025, 3, 15, 10, 30, 0);
    Position pos = Position::builder()
                       .withTimestamp(ts)
                       .build();
    EXPECT_EQ(pos.getTimestamp(), ts);
}

TEST_F(PositionBuilderTest, BuildWithAllFields)
{
    auto ts = makeTimestamp(2025, 6, 1, 8, 0, 0);
    Position pos = Position::builder()
                       .withLatitude(48.8566)
                       .withLongitude(2.3522)
                       .withAltitude(35.0)
                       .withAccuracy(1.5)
                       .withTimestamp(ts)
                       .build();
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 48.8566);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 2.3522);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 35.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 1.5);
    EXPECT_EQ(pos.getTimestamp(), ts);
}

TEST_F(PositionBuilderTest, BuilderChainingOrder)
{
    // Verify that the order of chaining does not matter
    auto ts = makeTimestamp(2025, 1, 1);
    Position pos1 = Position::builder()
                        .withLatitude(10.0)
                        .withLongitude(20.0)
                        .withAltitude(30.0)
                        .withAccuracy(40.0)
                        .withTimestamp(ts)
                        .build();
    Position pos2 = Position::builder()
                        .withTimestamp(ts)
                        .withAccuracy(40.0)
                        .withAltitude(30.0)
                        .withLongitude(20.0)
                        .withLatitude(10.0)
                        .build();
    EXPECT_DOUBLE_EQ(pos1.getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(pos1.getLongitude(), pos2.getLongitude());
    EXPECT_DOUBLE_EQ(pos1.getAltitude(), pos2.getAltitude());
    EXPECT_DOUBLE_EQ(pos1.getAccuracy(), pos2.getAccuracy());
    EXPECT_EQ(pos1.getTimestamp(), pos2.getTimestamp());
}

TEST_F(PositionBuilderTest, BuilderLastSetWins)
{
    Position pos = Position::builder()
                       .withLatitude(10.0)
                       .withLatitude(20.0)
                       .build();
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 20.0);
}

TEST_F(PositionBuilderTest, BuilderProducesEquivalentToConstructor)
{
    auto ts = makeTimestamp(2025, 7, 4, 12, 0, 0);
    Position fromCtor(40.7128, -74.0060, 100.0, 3.0, ts);
    Position fromBuilder = Position::builder()
                               .withLatitude(40.7128)
                               .withLongitude(-74.0060)
                               .withAltitude(100.0)
                               .withAccuracy(3.0)
                               .withTimestamp(ts)
                               .build();
    EXPECT_DOUBLE_EQ(fromCtor.getLatitude(), fromBuilder.getLatitude());
    EXPECT_DOUBLE_EQ(fromCtor.getLongitude(), fromBuilder.getLongitude());
    EXPECT_DOUBLE_EQ(fromCtor.getAltitude(), fromBuilder.getAltitude());
    EXPECT_DOUBLE_EQ(fromCtor.getAccuracy(), fromBuilder.getAccuracy());
    EXPECT_EQ(fromCtor.getTimestamp(), fromBuilder.getTimestamp());
}

TEST_F(PositionBuilderTest, BuilderCanBuildMultiplePositions)
{
    auto builder = Position::builder()
                       .withLatitude(10.0)
                       .withLongitude(20.0);
    Position pos1 = builder.build();
    Position pos2 = builder.build();
    EXPECT_DOUBLE_EQ(pos1.getLatitude(), pos2.getLatitude());
    EXPECT_DOUBLE_EQ(pos1.getLongitude(), pos2.getLongitude());
}

// ============================================================================
// Copy / assignment semantics
// ============================================================================

class PositionCopyTest : public ::testing::Test {};

TEST_F(PositionCopyTest, CopyConstructor)
{
    auto ts = makeTimestamp(2025, 5, 20, 14, 0, 0);
    Position original(40.7128, -74.0060, 100.0, 3.0, ts);
    Position copy(original);
    EXPECT_DOUBLE_EQ(copy.getLatitude(), original.getLatitude());
    EXPECT_DOUBLE_EQ(copy.getLongitude(), original.getLongitude());
    EXPECT_DOUBLE_EQ(copy.getAltitude(), original.getAltitude());
    EXPECT_DOUBLE_EQ(copy.getAccuracy(), original.getAccuracy());
    EXPECT_EQ(copy.getTimestamp(), original.getTimestamp());
}

TEST_F(PositionCopyTest, CopyAssignment)
{
    auto ts = makeTimestamp(2025, 5, 20, 14, 0, 0);
    Position original(40.7128, -74.0060, 100.0, 3.0, ts);
    Position copy;
    copy = original;
    EXPECT_DOUBLE_EQ(copy.getLatitude(), original.getLatitude());
    EXPECT_DOUBLE_EQ(copy.getLongitude(), original.getLongitude());
    EXPECT_DOUBLE_EQ(copy.getAltitude(), original.getAltitude());
    EXPECT_DOUBLE_EQ(copy.getAccuracy(), original.getAccuracy());
    EXPECT_EQ(copy.getTimestamp(), original.getTimestamp());
}

TEST_F(PositionCopyTest, CopyIsIndependent)
{
    Position original(40.7128, -74.0060);
    Position copy(original);
    copy.setLatitude(0.0);
    EXPECT_DOUBLE_EQ(original.getLatitude(), 40.7128);
    EXPECT_DOUBLE_EQ(copy.getLatitude(), 0.0);
}

// ============================================================================
// Integration: distance after mutation
// ============================================================================

class PositionMutationDistanceTest : public ::testing::Test {};

TEST_F(PositionMutationDistanceTest, DistanceAfterSettingCoordinates)
{
    Position pos1;
    Position pos2;
    pos1.setLatitude(0.0);
    pos1.setLongitude(0.0);
    pos2.setLatitude(0.0);
    pos2.setLongitude(1.0);
    double distance = pos1.distanceTo(pos2);
    EXPECT_NEAR(distance, 111320.0, 500.0);
}

TEST_F(PositionMutationDistanceTest, DistanceAfterMovingToSamePoint)
{
    Position pos1(10.0, 20.0);
    Position pos2(30.0, 40.0);
    pos2.setLatitude(10.0);
    pos2.setLongitude(20.0);
    EXPECT_NEAR(pos1.distanceTo(pos2), 0.0, 1e-6);
}

// ============================================================================
// Builder-built positions used in distance calculations
// ============================================================================

class PositionBuilderDistanceTest : public ::testing::Test {};

TEST_F(PositionBuilderDistanceTest, DistanceBetweenBuilderPositions)
{
    Position pos1 = Position::builder()
                        .withLatitude(0.0)
                        .withLongitude(0.0)
                        .build();
    Position pos2 = Position::builder()
                        .withLatitude(0.0)
                        .withLongitude(1.0)
                        .build();
    double distance = pos1.distanceTo(pos2);
    EXPECT_NEAR(distance, 111320.0, 500.0);
}

// ============================================================================
// Timestamp-related tests
// ============================================================================

class PositionTimestampTest : public ::testing::Test {};

TEST_F(PositionTimestampTest, TimestampPreservedThroughBuilder)
{
    auto ts = makeTimestamp(2020, 1, 1, 0, 0, 0);
    Position pos = Position::builder().withTimestamp(ts).build();
    EXPECT_EQ(pos.getTimestamp(), ts);
}

TEST_F(PositionTimestampTest, TimestampPreservedThroughConstructor)
{
    auto ts = makeTimestamp(2020, 12, 31, 23, 59, 59);
    Position pos(0.0, 0.0, 0.0, DEFAULT_POSITION_ACCURACY, ts);
    EXPECT_EQ(pos.getTimestamp(), ts);
}

TEST_F(PositionTimestampTest, TimestampCanBeUpdated)
{
    auto ts1 = makeTimestamp(2020, 1, 1);
    auto ts2 = makeTimestamp(2025, 6, 15);
    Position pos(0.0, 0.0, 0.0, DEFAULT_POSITION_ACCURACY, ts1);
    pos.setTimestamp(ts2);
    EXPECT_EQ(pos.getTimestamp(), ts2);
}

TEST_F(PositionTimestampTest, ToStringContainsTimestampInfo)
{
    auto ts = makeTimestamp(2025, 3, 15, 10, 30, 0);
    Position pos(0.0, 0.0, 0.0, DEFAULT_POSITION_ACCURACY, ts);
    std::string str = pos.toString();
    // The toString uses localtime, so just verify the time= field is present
    EXPECT_THAT(str, ::testing::HasSubstr("time="));
    // The year should appear in the formatted output
    EXPECT_THAT(str, ::testing::HasSubstr("2025"));
}
// </test_code>

---

**Confidence:** High — The test file covers all public API surface of the `Position` and `PositionBuilder` classes: default and parameterized constructors, all getters/setters, the `distanceTo()` Haversine calculation (including symmetry, identity, boundary coordinates, triangle inequality, date-line crossing, and known-distance checks), `toString()` formatting, builder fluent interface (chaining order, last-set-wins, equivalence to constructor, reusability), copy semantics, and timestamp handling. All tests use deterministic fixed timestamps and well-known geographic reference points with appropriate tolerances for floating-point comparisons.