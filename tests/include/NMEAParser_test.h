// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string.h>

// Include the header file being tested
#include "NMEAParser.h"

// Mock class that extends CNMEAParser to test virtual methods
class MockNMEAParser : public CNMEAParser {
public:
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E nError, char* pCmd), (override));
    MOCK_METHOD(void, LockDataAccess, (), (override));
    MOCK_METHOD(void, UnlockDataAccess, (), (override));
};

class NMEAParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize any test resources
    }

    void TearDown() override {
        // Clean up any test resources
    }

    CNMEAParser parser;
};

// Test the ProcessNMEABuffer method with valid input
TEST_F(NMEAParserTest, ProcessNMEABufferWithValidInput) {
    char buffer[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    int size = static_cast<int>(strlen(buffer));
    
    CNMEAParserData::ERROR_E result = parser.ProcessNMEABuffer(buffer, size);
    
    EXPECT_EQ(result, CNMEAParserData::ERROR_OK);
}

// Test the ProcessNMEABuffer method with empty buffer
TEST_F(NMEAParserTest, ProcessNMEABufferWithEmptyBuffer) {
    char buffer[] = "";
    int size = 0;
    
    CNMEAParserData::ERROR_E result = parser.ProcessNMEABuffer(buffer, size);
    
    EXPECT_EQ(result, CNMEAParserData::ERROR_OK);
}

// Test the ProcessNMEABuffer method with nullptr
TEST_F(NMEAParserTest, ProcessNMEABufferWithNullptr) {
    CNMEAParserData::ERROR_E result = parser.ProcessNMEABuffer(nullptr, 0);
    
    EXPECT_EQ(result, CNMEAParserData::ERROR_OK);
}

// Test the GetGPGGA method
TEST_F(NMEAParserTest, GetGPGGA) {
    CNMEAParserData::GGA_DATA_T ggaData;
    
    CNMEAParserData::ERROR_E result = parser.GetGPGGA(ggaData);
    
    EXPECT_EQ(result, CNMEAParserData::ERROR_OK);
    EXPECT_EQ(ggaData.dLatitude, 0.0);
    EXPECT_EQ(ggaData.dLongitude, 0.0);
    EXPECT_EQ(ggaData.dAltitudeMSL, 0.0);
}

// Test the virtual methods using the mock
TEST_F(NMEAParserTest, VirtualMethods) {
    MockNMEAParser mockParser;
    char cmd[] = "GPGGA";
    
    EXPECT_CALL(mockParser, OnError(CNMEAParserData::ERROR_UNKNOWN, testing::_))
        .Times(1);
    EXPECT_CALL(mockParser, LockDataAccess())
        .Times(1);
    EXPECT_CALL(mockParser, UnlockDataAccess())
        .Times(1);
    
    mockParser.OnError(CNMEAParserData::ERROR_UNKNOWN, cmd);
    mockParser.LockDataAccess();
    mockParser.UnlockDataAccess();
}

// Test the GGA_DATA_T struct default values
TEST_F(NMEAParserTest, GGADataDefaultValues) {
    CNMEAParserData::GGA_DATA_T ggaData;
    
    EXPECT_DOUBLE_EQ(ggaData.dLatitude, 0.0);
    EXPECT_DOUBLE_EQ(ggaData.dLongitude, 0.0);
    EXPECT_DOUBLE_EQ(ggaData.dAltitudeMSL, 0.0);
}

// Test the ERROR_E enum values
TEST_F(NMEAParserTest, ErrorEnumValues) {
    EXPECT_EQ(static_cast<int>(CNMEAParserData::ERROR_OK), 0);
    EXPECT_EQ(static_cast<int>(CNMEAParserData::ERROR_UNKNOWN), 1);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
// </test_code>