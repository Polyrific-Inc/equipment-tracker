// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <string>
#include "equipment_tracker/position.h"
#include "equipment_tracker/utils/constants.h"
#include "equipment_tracker/utils/types.h"

// Define M_PI if not available
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace equipment_tracker {

// ---------------------------------------------------------------------------
// Test fixture
// ---------------------------------------------------------------------------
class PositionTest : public ::testing::Test {
protected:
    void SetUp() override {
        now_ = std::chrono::system_clock::now();
        // A fixed, well-known epoch for deterministic timestamp tests
        epoch_ = std::chrono::system_clock::from_time_t(0);
        // A known date: 2024-06-15 12:30:00 UTC
        knownTime_ = std::chrono::system_clock::from_time_t(1718451000);
    }

    Timestamp now_;
    Timestamp epoch_;
    Timestamp knownTime_;

    // Helper: degrees to radians
    static double deg2rad(double deg) { return deg * M_PI / 180.0; }

    // Reference Haversine implementation for cross-checking
    static double haversine(double lat1, double lon1, double lat2, double lon2) {
        double lat1r = deg2rad(lat1);
        double lat2r = deg2rad(lat2);
        double dlat  = deg2rad(lat2 - lat1);
        double dlon  = deg2rad(lon2 - lon1);
        double a = std::sin(dlat / 2.0) * std::sin(dlat / 2.0) +
                   std::cos(lat1r) * std::cos(lat2r) *
                   std::sin(dlon / 2.0) * std::sin(dlon / 2.0);
        double c = 2.0 * std::atan2(std::sqrt(a), std::sqrt(1.0 - a));
        return EARTH_RADIUS_METERS * c;
    }
};

// ===========================================================================
// Constructor tests
// ===========================================================================

TEST_F(PositionTest, ConstructorSetsAllFields) {
    Position pos(37.7749, -122.4194, 10.5, 5.2, now_);

    EXPECT_DOUBLE_EQ(37.7749,   pos.getLatitude());
    EXPECT_DOUBLE_EQ(-122.4194, pos.getLongitude());
    EXPECT_DOUBLE_EQ(10.5,      pos.getAltitude());
    EXPECT_DOUBLE_EQ(5.2,       pos.getAccuracy());
    EXPECT_EQ(now_,             pos.getTimestamp());
}

TEST_F(PositionTest, ConstructorDefaultAltitudeIsZero) {
    Position pos(10.0, 20.0);

    EXPECT_DOUBLE_EQ(0.0, pos.getAltitude());
}

TEST_F(PositionTest, ConstructorDefaultAccuracyIsConstant) {
    Position pos(10.0, 20.0, 5.0);

    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, pos.getAccuracy());
}

TEST_F(PositionTest, DefaultConstructorZeroesLatLonAlt) {
    Position pos;

    EXPECT_DOUBLE_EQ(0.0, pos.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, pos.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, pos.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, pos.getAccuracy());
}

// ===========================================================================
// Setter / getter round-trip tests
// ===========================================================================

TEST_F(PositionTest, SetLatitudeUpdatesValue) {
    Position pos;
    pos.setLatitude(45.0);
    EXPECT_DOUBLE_EQ(45.0, pos.getLatitude());
}

TEST_F(PositionTest, SetLongitudeUpdatesValue) {
    Position pos;
    pos.setLongitude(-90.0);
    EXPECT_DOUBLE_EQ(-90.0, pos.getLongitude());
}

TEST_F(PositionTest, SetAltitudeUpdatesValue) {
    Position pos;
    pos.setAltitude(8848.86);
    EXPECT_DOUBLE_EQ(8848.86, pos.getAltitude());
}

TEST_F(PositionTest, SetAccuracyUpdatesValue) {
    Position pos;
    pos.setAccuracy(1.0);
    EXPECT_DOUBLE_EQ(1.0, pos.getAccuracy());
}

TEST_F(PositionTest, SetTimestampUpdatesValue) {
    Position pos;
    pos.setTimestamp(knownTime_);
    EXPECT_EQ(knownTime_, pos.getTimestamp());
}

// ===========================================================================
// Builder tests
// ===========================================================================

TEST_F(PositionTest, BuilderCreatesPositionWithAllFields) {
    Position pos = Position::builder()
                       .withLatitude(51.5074)
                       .withLongitude(-0.1278)
                       .withAltitude(11.0)
                       .withAccuracy(3.0)
                       .withTimestamp(now_)
                       .build();

    EXPECT_DOUBLE_EQ(51.5074,  pos.getLatitude());
    EXPECT_DOUBLE_EQ(-0.1278,  pos.getLongitude());
    EXPECT_DOUBLE_EQ(11.0,     pos.getAltitude());
    EXPECT_DOUBLE_EQ(3.0,      pos.getAccuracy());
    EXPECT_EQ(now_,            pos.getTimestamp());
}

TEST_F(PositionTest, BuilderDefaultsMatchDefaultConstructor) {
    Position pos = Position::builder().build();

    EXPECT_DOUBLE_EQ(0.0, pos.getLatitude());
    EXPECT_DOUBLE_EQ(0.0, pos.getLongitude());
    EXPECT_DOUBLE_EQ(0.0, pos.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, pos.getAccuracy());
}

TEST_F(PositionTest, BuilderPartialFieldsLeavesDefaultsForOthers) {
    Position pos = Position::builder()
                       .withLatitude(10.0)
                       .withLongitude(20.0)
                       .build();

    EXPECT_DOUBLE_EQ(10.0, pos.getLatitude());
    EXPECT_DOUBLE_EQ(20.0, pos.getLongitude());
    EXPECT_DOUBLE_EQ(0.0,  pos.getAltitude());
    EXPECT_DOUBLE_EQ(DEFAULT_POSITION_ACCURACY, pos.getAccuracy());
}

// ===========================================================================
// distanceTo – same point
// ===========================================================================

TEST_F(PositionTest, DistanceToSamePositionIsZero) {
    Position pos(37.7749, -122.4194, 10.0, 5.0, now_);
    EXPECT_NEAR(0.0, pos.distanceTo(pos), 1e-6);
}

TEST_F(PositionTest, DistanceToIdenticalCopyIsZero) {
    Position a(48.8566, 2.3522, 35.0, 1.0, now_);
    Position b(48.8566, 2.3522, 35.0, 1.0, now_);
    EXPECT_NEAR(0.0, a.distanceTo(b), 1e-6);
}

// ===========================================================================
// distanceTo – known city pairs
// ===========================================================================

TEST_F(PositionTest, DistanceSanFranciscoToNewYork) {
    Position sf(37.7749, -122.4194, 0, 0, now_);
    Position ny(40.7128, -74.0060,  0, 0, now_);

    double distance = sf.distanceTo(ny);
    // Great-circle distance ≈ 4129 km
    EXPECT_NEAR(4.129e6, distance, 4.129e6 * 0.02);
}

TEST_F(PositionTest, DistanceLondonToTokyo) {
    Position london(51.5074, -0.1278, 0, 0, now_);
    Position tokyo(35.6762, 139.6503, 0, 0, now_);

    double distance = london.distanceTo(tokyo);
    // ≈ 9560 km
    EXPECT_NEAR(9.56e6, distance, 9.56e6 * 0.02);
}

TEST_F(PositionTest, DistanceSydneyToLosAngeles) {
    Position sydney(-33.8688, 151.2093, 0, 0, now_);
    Position la(34.0522, -118.2437, 0, 0, now_);

    double distance = sydney.distanceTo(la);
    // ≈ 12,074 km
    EXPECT_NEAR(12.074e6, distance, 12.074e6 * 0.02);
}

// ===========================================================================
// distanceTo – symmetry
// ===========================================================================

TEST_F(PositionTest, DistanceIsSymmetric) {
    Position a(37.7749, -122.4194, 0, 0, now_);
    Position b(40.7128, -74.0060,  0, 0, now_);

    EXPECT_DOUBLE_EQ(a.distanceTo(b), b.distanceTo(a));
}

// ===========================================================================
// distanceTo – poles and equator
// ===========================================================================

TEST_F(PositionTest, DistanceNorthPoleToSouthPole) {
    Position np(90.0,  0.0, 0, 0, now_);
    Position sp(-90.0, 0.0, 0, 0, now_);

    double expected = M_PI * EARTH_RADIUS_METERS; // half circumference
    EXPECT_NEAR(expected, np.distanceTo(sp), 1.0);
}

TEST_F(PositionTest, DistanceAntipodal) {
    Position a(0.0, 0.0,   0, 0, now_);
    Position b(0.0, 180.0, 0, 0, now_);

    double expected = M_PI * EARTH_RADIUS_METERS;
    EXPECT_NEAR(expected, a.distanceTo(b), 1.0);
}

TEST_F(PositionTest, DistanceAlongEquator90Degrees) {
    Position a(0.0, 0.0,  0, 0, now_);
    Position b(0.0, 90.0, 0, 0, now_);

    double expected = (M_PI / 2.0) * EARTH_RADIUS_METERS; // quarter circumference
    EXPECT_NEAR(expected, a.distanceTo(b), 1.0);
}

// ===========================================================================
// distanceTo – short distances
// ===========================================================================

TEST_F(PositionTest, DistanceApprox100MetersNorth) {
    // ~0.0009 degrees latitude ≈ 100 m
    Position a(37.7749, -122.4194, 0, 0, now_);
    Position b(37.7758, -122.4194, 0, 0, now_);

    double distance = a.distanceTo(b);
    EXPECT_NEAR(100.0, distance, 5.0);
}

TEST_F(PositionTest, DistanceVerySmallSeparation) {
    Position a(37.7749,   -122.4194,  0, 0, now_);
    Position b(37.774901, -122.4194,  0, 0, now_);

    double distance = a.distanceTo(b);
    EXPECT_GT(distance, 0.0);
    EXPECT_LT(distance, 1.0); // sub-meter
}

// ===========================================================================
// distanceTo – cross-check with reference implementation
// ===========================================================================

TEST_F(PositionTest, DistanceMatchesReferenceHaversine) {
    double lat1 = 34.0522, lon1 = -118.2437;
    double lat2 = 48.8566, lon2 = 2.3522;

    Position a(lat1, lon1, 0, 0, now_);
    Position b(lat2, lon2, 0, 0, now_);

    double expected = haversine(lat1, lon1, lat2, lon2);
    EXPECT_NEAR(expected, a.distanceTo(b), 0.001);
}

// ===========================================================================
// distanceTo – negative / southern-hemisphere coordinates
// ===========================================================================

TEST_F(PositionTest, DistanceSouthernHemisphere) {
    Position buenosAires(-34.6037, -58.3816, 0, 0, now_);
    Position capeTown(-33.9249, 18.4241,    0, 0, now_);

    double expected = haversine(-34.6037, -58.3816, -33.9249, 18.4241);
    EXPECT_NEAR(expected, buenosAires.distanceTo(capeTown), 0.001);
}

// ===========================================================================
// distanceTo – date-line crossing
// ===========================================================================

TEST_F(PositionTest, DistanceAcrossDateLine) {
    Position a(0.0, 179.0,  0, 0, now_);
    Position b(0.0, -179.0, 0, 0, now_);

    // Should be ~2 degrees of longitude at equator ≈ 222 km
    double expected = haversine(0.0, 179.0, 0.0, -179.0);
    EXPECT_NEAR(expected, a.distanceTo(b), 0.001);
}

// ===========================================================================
// distanceTo – altitude and accuracy are ignored
// ===========================================================================

TEST_F(PositionTest, DistanceIgnoresAltitude) {
    Position a(37.7749, -122.4194, 0.0,    0, now_);
    Position b(37.7749, -122.4194, 5000.0, 0, now_);

    // Haversine is 2-D; same lat/lon → zero distance regardless of altitude
    EXPECT_NEAR(0.0, a.distanceTo(b), 1e-6);
}

TEST_F(PositionTest, DistanceIgnoresAccuracy) {
    Position a(37.7749, -122.4194, 0, 1.0,   now_);
    Position b(40.7128, -74.0060,  0, 100.0,  now_);

    Position a2(37.7749, -122.4194, 0, 100.0, now_);
    Position b2(40.7128, -74.0060,  0, 1.0,   now_);

    EXPECT_DOUBLE_EQ(a.distanceTo(b), a2.distanceTo(b2));
}

// ===========================================================================
// toString – content checks
// ===========================================================================

TEST_F(PositionTest, ToStringContainsLatitude) {
    Position pos(37.7749, -122.4194, 10.5, 5.2, now_);
    EXPECT_THAT(pos.toString(), ::testing::HasSubstr("lat=37.774900"));
}

TEST_F(PositionTest, ToStringContainsLongitude) {
    Position pos(37.7749, -122.4194, 10.5, 5.2, now_);
    EXPECT_THAT(pos.toString(), ::testing::HasSubstr("lon=-122.419400"));
}

TEST_F(PositionTest, ToStringContainsAltitude) {
    Position pos(37.7749, -122.4194, 10.5, 5.2, now_);
    EXPECT_THAT(pos.toString(), ::testing::HasSubstr("alt=10.50m"));
}

TEST_F(PositionTest, ToStringContainsAccuracy) {
    Position pos(37.7749, -122.4194, 10.5, 5.2, now_);
    EXPECT_THAT(pos.toString(), ::testing::HasSubstr("acc=5.20m"));
}

TEST_F(PositionTest, ToStringContainsTimeLabel) {
    Position pos(37.7749, -122.4194, 10.5, 5.2, now_);
    EXPECT_THAT(pos.toString(), ::testing::HasSubstr("time="));
}

TEST_F(PositionTest, ToStringStartsWithPositionPrefix) {
    Position pos(37.7749, -122.4194, 10.5, 5.2, now_);
    std::string result = pos.toString();
    EXPECT_EQ(result.substr(0, 9), "Position(");
}

TEST_F(PositionTest, ToStringEndsWithClosingParen) {
    Position pos(37.7749, -122.4194, 10.5, 5.2, now_);
    std::string result = pos.toString();
    EXPECT_EQ(result.back(), ')');
}

TEST_F(PositionTest, ToStringHandlesZeroValues) {
    Position pos(0.0, 0.0, 0.0, 0.0, now_);
    std::string result = pos.toString();

    EXPECT_THAT(result, ::testing::HasSubstr("lat=0.000000"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=0.000000"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=0.00m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=0.00m"));
}

TEST_F(PositionTest, ToStringHandlesNegativeLatLon) {
    Position pos(-37.7749, -122.4194, -10.5, 5.2, now_);
    std::string result = pos.toString();

    EXPECT_THAT(result, ::testing::HasSubstr("lat=-37.774900"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=-122.419400"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=-10.50m"));
}

TEST_F(PositionTest, ToStringHandlesLargeValues) {
    Position pos(89.999999, 179.999999, 99999.99, 1000.0, now_);
    std::string result = pos.toString();

    EXPECT_THAT(result, ::testing::HasSubstr("lat=89.999999"));
    EXPECT_THAT(result, ::testing::HasSubstr("lon=179.999999"));
    EXPECT_THAT(result, ::testing::HasSubstr("alt=99999.99m"));
    EXPECT_THAT(result, ::testing::HasSubstr("acc=1000.00m"));
}

TEST_F(PositionTest, ToStringTimestampFormatYYYYMMDD) {
    // Use a known timestamp so we can predict the formatted date portion
    // 2024-06-15 12:30:00 UTC  (local representation depends on TZ, but the
    // format pattern "%Y-%m-%d %H:%M:%S" is always used)
    Position pos(10.0, 20.0, 0.0, 1.0, knownTime_);
    std::string result = pos.toString();

    // The time= field should contain a dash-separated date and colon-separated time
    // We cannot predict the exact local-time value portably, but we can verify
    // the structural pattern "time=YYYY-" exists.
    EXPECT_THAT(result, ::testing::ContainsRegex("time=[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}"));
}

// ===========================================================================
// Edge / boundary coordinate values
// ===========================================================================

TEST_F(PositionTest, ConstructorAcceptsExtremeLatitudes) {
    Position north(90.0, 0.0, 0, 0, now_);
    Position south(-90.0, 0.0, 0, 0, now_);

    EXPECT_DOUBLE_EQ(90.0,  north.getLatitude());
    EXPECT_DOUBLE_EQ(-90.0, south.getLatitude());
}

TEST_F(PositionTest, ConstructorAcceptsExtremeLongitudes) {
    Position east(0.0, 180.0,  0, 0, now_);
    Position west(0.0, -180.0, 0, 0, now_);

    EXPECT_DOUBLE_EQ(180.0,  east.getLongitude());
    EXPECT_DOUBLE_EQ(-180.0, west.getLongitude());
}

TEST_F(PositionTest, DistanceBetweenSameLongitude180AndMinus180IsZero) {
    Position a(0.0, 180.0,  0, 0, now_);
    Position b(0.0, -180.0, 0, 0, now_);

    // 180 and -180 are the same meridian; Haversine gives ~0 or full circle
    // depending on sign handling. With the formula used, dlon = -360 deg →
    // sin(-180) = 0, so a = 0, distance = 0.
    EXPECT_NEAR(0.0, a.distanceTo(b), 1.0);
}

// ===========================================================================
// distanceTo – triangle inequality sanity check
// ===========================================================================

TEST_F(PositionTest, TriangleInequalityHolds) {
    Position a(0.0, 0.0,   0, 0, now_);
    Position b(30.0, 30.0, 0, 0, now_);
    Position c(60.0, 60.0, 0, 0, now_);

    double ab = a.distanceTo(b);
    double bc = b.distanceTo(c);
    double ac = a.distanceTo(c);

    EXPECT_LE(ac, ab + bc + 1.0); // +1 m tolerance for FP
}

// ===========================================================================
// Copy semantics
// ===========================================================================

TEST_F(PositionTest, CopyConstructorPreservesValues) {
    Position original(37.7749, -122.4194, 10.5, 5.2, now_);
    Position copy(original);

    EXPECT_DOUBLE_EQ(original.getLatitude(),  copy.getLatitude());
    EXPECT_DOUBLE_EQ(original.getLongitude(), copy.getLongitude());
    EXPECT_DOUBLE_EQ(original.getAltitude(),  copy.getAltitude());
    EXPECT_DOUBLE_EQ(original.getAccuracy(),  copy.getAccuracy());
    EXPECT_EQ(original.getTimestamp(),         copy.getTimestamp());
}

TEST_F(PositionTest, CopyAssignmentPreservesValues) {
    Position original(37.7749, -122.4194, 10.5, 5.2, now_);
    Position assigned;
    assigned = original;

    EXPECT_DOUBLE_EQ(original.getLatitude(),  assigned.getLatitude());
    EXPECT_DOUBLE_EQ(original.getLongitude(), assigned.getLongitude());
    EXPECT_DOUBLE_EQ(original.getAltitude(),  assigned.getAltitude());
    EXPECT_DOUBLE_EQ(original.getAccuracy(),  assigned.getAccuracy());
    EXPECT_EQ(original.getTimestamp(),         assigned.getTimestamp());
}

} // namespace equipment_tracker
// </test_code>

---

**Confidence:** High — the test file is derived directly from the actual source code (`src/position.cpp`), its header (`position.h`), dependency headers (`types.h`, `constants.h`), and the project's CMake/GoogleTest v1.17.0 build configuration. All public methods (`distanceTo`, `toString`, getters, setters, builder) are exercised across normal, boundary, and edge-case inputs.