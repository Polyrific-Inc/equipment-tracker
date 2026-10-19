// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <string>
#include <chrono>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/types.h"
#include "equipment_tracker/utils/constants.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace equipment_tracker;

// ---------------------------------------------------------------------------
// Helper: build a fixed Timestamp from a time_t value so tests are
// deterministic and do not depend on wall-clock time.
// ---------------------------------------------------------------------------
static Timestamp makeTimestamp(std::time_t t)
{
    return std::chrono::system_clock::from_time_t(t);
}

// A well-known epoch value used across many tests.
static const std::time_t kEpochValue = 1700000000; // 2023-11-14T22:13:20 UTC
static const Timestamp kFixedTimestamp = makeTimestamp(kEpochValue);

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class PositionTest : public ::testing::Test
{
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// ===== Default constructor =================================================

TEST_F(PositionTest, DefaultConstructor_InitializesLatitudeToZero)
{
    Position pos;
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 0.0);
}

TEST_F(PositionTest, DefaultConstructor_InitializesLongitudeToZero)
{
    Position pos;
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 0.0);
}

TEST_F(PositionTest, DefaultConstructor_InitializesAltitudeToZero)
{
    Position pos;
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 0.0);
}

TEST_F(PositionTest, DefaultConstructor_InitializesAccuracyToDefault)
{
    Position pos;
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), DEFAULT_POSITION_ACCURACY);
}

TEST_F(PositionTest, DefaultConstructor_TimestampIsReasonable)
{
    // The default-constructed timestamp should be close to "now".
    auto before = std::chrono::system_clock::now();
    Position pos;
    auto after = std::chrono::system_clock::now();

    EXPECT_GE(pos.getTimestamp(), before - std::chrono::seconds(2));
    EXPECT_LE(pos.getTimestamp(), after + std::chrono::seconds(2));
}

// ===== Parameterized constructor ===========================================

TEST_F(PositionTest, Constructor_LatLonOnly_SetsDefaultAltitudeAccuracyTimestamp)
{
    auto before = std::chrono::system_clock::now();
    Position pos(12.34, 56.78);
    auto after = std::chrono::system_clock::now();

    EXPECT_DOUBLE_EQ(pos.getLatitude(), 12.34);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 56.78);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), DEFAULT_POSITION_ACCURACY);
    EXPECT_GE(pos.getTimestamp(), before - std::chrono::seconds(2));
    EXPECT_LE(pos.getTimestamp(), after + std::chrono::seconds(2));
}

TEST_F(PositionTest, Constructor_WithAltitude)
{
    Position pos(1.0, 2.0, 100.5);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 100.5);
}

TEST_F(PositionTest, Constructor_WithAccuracy)
{
    Position pos(1.0, 2.0, 50.0, 10.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 10.0);
}

TEST_F(PositionTest, Constructor_WithTimestamp)
{
    Position pos(1.0, 2.0, 50.0, 5.0, kFixedTimestamp);
    EXPECT_EQ(pos.getTimestamp(), kFixedTimestamp);
}

TEST_F(PositionTest, Constructor_AllParameters)
{
    Position pos(48.8566, 2.3522, 35.0, 1.5, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 48.8566);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 2.3522);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 35.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 1.5);
    EXPECT_EQ(pos.getTimestamp(), kFixedTimestamp);
}

// ===== Negative coordinate values ==========================================

TEST_F(PositionTest, Constructor_NegativeLatitude)
{
    Position pos(-33.8688, 151.2093, 0.0, 2.5, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), -33.8688);
}

TEST_F(PositionTest, Constructor_NegativeLongitude)
{
    Position pos(40.7128, -74.0060, 0.0, 2.5, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -74.0060);
}

TEST_F(PositionTest, Constructor_NegativeAltitude)
{
    Position pos(31.5, 35.5, -430.0, 2.5, kFixedTimestamp); // Dead Sea
    EXPECT_DOUBLE_EQ(pos.getAltitude(), -430.0);
}

// ===== Setters =============================================================

TEST_F(PositionTest, SetLatitude)
{
    Position pos;
    pos.setLatitude(45.0);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 45.0);
}

TEST_F(PositionTest, SetLongitude)
{
    Position pos;
    pos.setLongitude(-120.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -120.0);
}

TEST_F(PositionTest, SetAltitude)
{
    Position pos;
    pos.setAltitude(8848.86);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 8848.86);
}

TEST_F(PositionTest, SetAccuracy)
{
    Position pos;
    pos.setAccuracy(0.5);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 0.5);
}

TEST_F(PositionTest, SetTimestamp)
{
    Position pos;
    pos.setTimestamp(kFixedTimestamp);
    EXPECT_EQ(pos.getTimestamp(), kFixedTimestamp);
}

// ===== PositionBuilder =====================================================

TEST_F(PositionTest, Builder_DefaultBuild)
{
    Position pos = Position::builder().build();
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 0.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), DEFAULT_POSITION_ACCURACY);
}

TEST_F(PositionTest, Builder_WithLatitude)
{
    Position pos = Position::builder().withLatitude(10.0).build();
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 10.0);
}

TEST_F(PositionTest, Builder_WithLongitude)
{
    Position pos = Position::builder().withLongitude(20.0).build();
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 20.0);
}

TEST_F(PositionTest, Builder_WithAltitude)
{
    Position pos = Position::builder().withAltitude(300.0).build();
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 300.0);
}

TEST_F(PositionTest, Builder_WithAccuracy)
{
    Position pos = Position::builder().withAccuracy(0.1).build();
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 0.1);
}

TEST_F(PositionTest, Builder_WithTimestamp)
{
    Position pos = Position::builder().withTimestamp(kFixedTimestamp).build();
    EXPECT_EQ(pos.getTimestamp(), kFixedTimestamp);
}

TEST_F(PositionTest, Builder_FluentChaining)
{
    Position pos = Position::builder()
                       .withLatitude(51.5074)
                       .withLongitude(-0.1278)
                       .withAltitude(11.0)
                       .withAccuracy(3.0)
                       .withTimestamp(kFixedTimestamp)
                       .build();

    EXPECT_DOUBLE_EQ(pos.getLatitude(), 51.5074);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -0.1278);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 11.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 3.0);
    EXPECT_EQ(pos.getTimestamp(), kFixedTimestamp);
}

TEST_F(PositionTest, Builder_LastSetWins)
{
    Position pos = Position::builder()
                       .withLatitude(1.0)
                       .withLatitude(2.0)
                       .withLatitude(3.0)
                       .build();
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 3.0);
}

// ===== distanceTo ==========================================================

TEST_F(PositionTest, DistanceTo_SamePoint_IsZero)
{
    Position a(48.8566, 2.3522, 0.0, 2.5, kFixedTimestamp);
    EXPECT_NEAR(a.distanceTo(a), 0.0, 1e-6);
}

TEST_F(PositionTest, DistanceTo_IdenticalPositions_IsZero)
{
    Position a(48.8566, 2.3522, 0.0, 2.5, kFixedTimestamp);
    Position b(48.8566, 2.3522, 0.0, 2.5, kFixedTimestamp);
    EXPECT_NEAR(a.distanceTo(b), 0.0, 1e-6);
}

TEST_F(PositionTest, DistanceTo_IsSymmetric)
{
    Position a(48.8566, 2.3522, 0.0, 2.5, kFixedTimestamp);  // Paris
    Position b(51.5074, -0.1278, 0.0, 2.5, kFixedTimestamp); // London
    EXPECT_NEAR(a.distanceTo(b), b.distanceTo(a), 1e-6);
}

TEST_F(PositionTest, DistanceTo_ParisToLondon)
{
    // Known distance Paris → London ≈ 343.5 km (great-circle)
    Position paris(48.8566, 2.3522, 0.0, 2.5, kFixedTimestamp);
    Position london(51.5074, -0.1278, 0.0, 2.5, kFixedTimestamp);

    double dist = paris.distanceTo(london);
    EXPECT_NEAR(dist, 343556.0, 500.0); // within 500 m tolerance
}

TEST_F(PositionTest, DistanceTo_NewYorkToLosAngeles)
{
    // Known distance ≈ 3,944 km
    Position nyc(40.7128, -74.0060, 0.0, 2.5, kFixedTimestamp);
    Position la(34.0522, -118.2437, 0.0, 2.5, kFixedTimestamp);

    double dist = nyc.distanceTo(la);
    EXPECT_NEAR(dist, 3944000.0, 5000.0); // within 5 km tolerance
}

TEST_F(PositionTest, DistanceTo_AntipodialPoints)
{
    // North pole to south pole ≈ half circumference ≈ π * R ≈ 20015 km
    Position north(90.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    Position south(-90.0, 0.0, 0.0, 2.5, kFixedTimestamp);

    double dist = north.distanceTo(south);
    double expected = M_PI * EARTH_RADIUS_METERS;
    EXPECT_NEAR(dist, expected, 1.0);
}

TEST_F(PositionTest, DistanceTo_AlongEquator_90Degrees)
{
    // 90 degrees along equator ≈ π/2 * R
    Position a(0.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    Position b(0.0, 90.0, 0.0, 2.5, kFixedTimestamp);

    double dist = a.distanceTo(b);
    double expected = (M_PI / 2.0) * EARTH_RADIUS_METERS;
    EXPECT_NEAR(dist, expected, 1.0);
}

TEST_F(PositionTest, DistanceTo_ShortDistance)
{
    // Two points very close together (≈ 111 m apart along a meridian)
    Position a(0.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    Position b(0.001, 0.0, 0.0, 2.5, kFixedTimestamp);

    double dist = a.distanceTo(b);
    EXPECT_NEAR(dist, 111.19, 1.0); // ~111 m per 0.001 degree at equator
}

TEST_F(PositionTest, DistanceTo_CrossesDateLine)
{
    // Points on either side of the 180° meridian
    Position a(0.0, 179.0, 0.0, 2.5, kFixedTimestamp);
    Position b(0.0, -179.0, 0.0, 2.5, kFixedTimestamp);

    double dist = a.distanceTo(b);
    // Should be ~222 km (2 degrees at equator), not ~39,800 km (358 degrees)
    EXPECT_NEAR(dist, 222390.0, 500.0);
}

TEST_F(PositionTest, DistanceTo_CrossesPrimeMeridian)
{
    Position a(51.5, -1.0, 0.0, 2.5, kFixedTimestamp);
    Position b(51.5, 1.0, 0.0, 2.5, kFixedTimestamp);

    double dist = a.distanceTo(b);
    // 2 degrees of longitude at lat 51.5 ≈ 2 * cos(51.5°) * 111320 ≈ 138.6 km
    EXPECT_NEAR(dist, 138600.0, 500.0);
}

TEST_F(PositionTest, DistanceTo_IgnoresAltitude)
{
    // distanceTo uses Haversine (2-D surface distance), altitude should not matter
    Position a(0.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    Position b(0.0, 1.0, 10000.0, 2.5, kFixedTimestamp);
    Position c(0.0, 1.0, 0.0, 2.5, kFixedTimestamp);

    EXPECT_NEAR(a.distanceTo(b), a.distanceTo(c), 1e-6);
}

TEST_F(PositionTest, DistanceTo_TriangleInequality)
{
    Position a(0.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    Position b(10.0, 10.0, 0.0, 2.5, kFixedTimestamp);
    Position c(20.0, 0.0, 0.0, 2.5, kFixedTimestamp);

    double ab = a.distanceTo(b);
    double bc = b.distanceTo(c);
    double ac = a.distanceTo(c);

    EXPECT_LE(ac, ab + bc + 1e-6);
}

// ===== toString ============================================================

TEST_F(PositionTest, ToString_ContainsLatitude)
{
    Position pos(12.345678, 98.765432, 100.0, 5.0, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("lat=12.345678"));
}

TEST_F(PositionTest, ToString_ContainsLongitude)
{
    Position pos(12.345678, 98.765432, 100.0, 5.0, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("lon=98.765432"));
}

TEST_F(PositionTest, ToString_ContainsAltitude)
{
    Position pos(12.345678, 98.765432, 100.0, 5.0, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("alt=100.00"));
}

TEST_F(PositionTest, ToString_ContainsAccuracy)
{
    Position pos(12.345678, 98.765432, 100.0, 5.0, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("acc=5.00"));
}

TEST_F(PositionTest, ToString_ContainsPositionPrefix)
{
    Position pos(0.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("Position("));
}

TEST_F(PositionTest, ToString_ContainsTimeLabel)
{
    Position pos(0.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("time="));
}

TEST_F(PositionTest, ToString_NegativeCoordinates)
{
    Position pos(-33.868800, -151.209300, 0.0, 2.5, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("lat=-33.868800"));
    EXPECT_THAT(s, ::testing::HasSubstr("lon=-151.209300"));
}

TEST_F(PositionTest, ToString_ZeroValues)
{
    Position pos(0.0, 0.0, 0.0, 0.0, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_THAT(s, ::testing::HasSubstr("lat=0.000000"));
    EXPECT_THAT(s, ::testing::HasSubstr("lon=0.000000"));
    EXPECT_THAT(s, ::testing::HasSubstr("alt=0.00"));
    EXPECT_THAT(s, ::testing::HasSubstr("acc=0.00"));
}

TEST_F(PositionTest, ToString_EndsWithClosingParen)
{
    Position pos(0.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    std::string s = pos.toString();
    EXPECT_EQ(s.back(), ')');
}

// ===== Edge cases ==========================================================

TEST_F(PositionTest, BoundaryLatitude_NorthPole)
{
    Position pos(90.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), 90.0);
}

TEST_F(PositionTest, BoundaryLatitude_SouthPole)
{
    Position pos(-90.0, 0.0, 0.0, 2.5, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getLatitude(), -90.0);
}

TEST_F(PositionTest, BoundaryLongitude_DateLinePositive)
{
    Position pos(0.0, 180.0, 0.0, 2.5, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 180.0);
}

TEST_F(PositionTest, BoundaryLongitude_DateLineNegative)
{
    Position pos(0.0, -180.0, 0.0, 2.5, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), -180.0);
}

TEST_F(PositionTest, ZeroAccuracy)
{
    Position pos(0.0, 0.0, 0.0, 0.0, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 0.0);
}

TEST_F(PositionTest, VeryLargeAltitude)
{
    Position pos(0.0, 0.0, 408000.0, 2.5, kFixedTimestamp); // ISS altitude
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 408000.0);
}

TEST_F(PositionTest, VerySmallAccuracy)
{
    Position pos(0.0, 0.0, 0.0, 0.001, kFixedTimestamp);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 0.001);
}

// ===== Copy semantics ======================================================

TEST_F(PositionTest, CopyConstructor)
{
    Position original(48.8566, 2.3522, 35.0, 1.5, kFixedTimestamp);
    Position copy(original);

    EXPECT_DOUBLE_EQ(copy.getLatitude(), original.getLatitude());
    EXPECT_DOUBLE_EQ(copy.getLongitude(), original.getLongitude());
    EXPECT_DOUBLE_EQ(copy.getAltitude(), original.getAltitude());
    EXPECT_DOUBLE_EQ(copy.getAccuracy(), original.getAccuracy());
    EXPECT_EQ(copy.getTimestamp(), original.getTimestamp());
}

TEST_F(PositionTest, CopyAssignment)
{
    Position original(48.8566, 2.3522, 35.0, 1.5, kFixedTimestamp);
    Position copy;
    copy = original;

    EXPECT_DOUBLE_EQ(copy.getLatitude(), original.getLatitude());
    EXPECT_DOUBLE_EQ(copy.getLongitude(), original.getLongitude());
    EXPECT_DOUBLE_EQ(copy.getAltitude(), original.getAltitude());
    EXPECT_DOUBLE_EQ(copy.getAccuracy(), original.getAccuracy());
    EXPECT_EQ(copy.getTimestamp(), original.getTimestamp());
}

// ===== Setter then getter round-trip =======================================

TEST_F(PositionTest, SettersOverrideConstructorValues)
{
    Position pos(1.0, 2.0, 3.0, 4.0, kFixedTimestamp);
    Timestamp newTs = makeTimestamp(kEpochValue + 3600);

    pos.setLatitude(10.0);
    pos.setLongitude(20.0);
    pos.setAltitude(30.0);
    pos.setAccuracy(40.0);
    pos.setTimestamp(newTs);

    EXPECT_DOUBLE_EQ(pos.getLatitude(), 10.0);
    EXPECT_DOUBLE_EQ(pos.getLongitude(), 20.0);
    EXPECT_DOUBLE_EQ(pos.getAltitude(), 30.0);
    EXPECT_DOUBLE_EQ(pos.getAccuracy(), 40.0);
    EXPECT_EQ(pos.getTimestamp(), newTs);
}

// ===== Builder produces same result as constructor =========================

TEST_F(PositionTest, Builder_ProducesSameResultAsConstructor)
{
    Position fromCtor(48.8566, 2.3522, 35.0, 1.5, kFixedTimestamp);
    Position fromBuilder = Position::builder()
                               .withLatitude(48.8566)
                               .withLongitude(2.3522)
                               .withAltitude(35.0)
                               .withAccuracy(1.5)
                               .withTimestamp(kFixedTimestamp)
                               .build();

    EXPECT_DOUBLE_EQ(fromCtor.getLatitude(), fromBuilder.getLatitude());
    EXPECT_DOUBLE_EQ(fromCtor.getLongitude(), fromBuilder.getLongitude());
    EXPECT_DOUBLE_EQ(fromCtor.getAltitude(), fromBuilder.getAltitude());
    EXPECT_DOUBLE_EQ(fromCtor.getAccuracy(), fromBuilder.getAccuracy());
    EXPECT_EQ(fromCtor.getTimestamp(), fromBuilder.getTimestamp());
}

// ===== distanceTo after mutation ===========================================

TEST_F(PositionTest, DistanceTo_AfterSetters)
{
    Position a;
    a.setLatitude(0.0);
    a.setLongitude(0.0);

    Position b;
    b.setLatitude(0.0);
    b.setLongitude(1.0);

    // 1 degree of longitude at equator ≈ 111,320 m
    double dist = a.distanceTo(b);
    EXPECT_NEAR(dist, 111320.0, 100.0);
}
// </test_code>