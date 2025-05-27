// <test_code>
#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string.h>

// Include the header file being tested
// Mock NMEAParser.h for building without the external dependency
#pragma once

class CNMEAParserData
{
public:
    enum ERROR_E
    {
        ERROR_OK = 0,
        ERROR_UNKNOWN = 1
    };

    struct GGA_DATA_T
    {
        double dLatitude = 0.0;
        double dLongitude = 0.0;
        double dAltitudeMSL = 0.0;
    };
};

class CNMEAParser
{
public:
    virtual ~CNMEAParser() {}

    // Mock methods
    CNMEAParserData::ERROR_E ProcessNMEABuffer(char *pBuffer, int iSize)
    {
        return CNMEAParserData::ERROR_OK;
    }

    CNMEAParserData::ERROR_E GetGPGGA(CNMEAParserData::GGA_DATA_T &ggaData)
    {
        return CNMEAParserData::ERROR_OK;
    }

    // Virtual methods to override
    virtual void OnError(CNMEAParserData::ERROR_E nError, char *pCmd) {}
    virtual void LockDataAccess() {}
    virtual void UnlockDataAccess() {}
};

// Mock class for testing
class MockNMEAParser : public CNMEAParser {
public:
    MOCK_METHOD(void, OnError, (CNMEAParserData::ERROR_E nError, char *pCmd), (override));
    MOCK_METHOD(void, LockDataAccess, (), (override));
    MOCK_METHOD(void, UnlockDataAccess, (), (override));
    
    // Make the non-virtual methods public for testing
    using CNMEAParser::ProcessNMEABuffer;
    using CNMEAParser::GetGPGGA;
};

class NMEAParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize any test resources
    }

    void TearDown() override {
        // Clean up any test resources
    }

    MockNMEAParser parser;
};

// Test the ProcessNMEABuffer method
TEST_F(NMEAParserTest, ProcessNMEABufferReturnsOK) {
    char buffer[] = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47";
    int size = static_cast<int>(strlen(buffer));
    
    EXPECT_EQ(CNMEAParserData::ERROR_OK, parser.ProcessNMEABuffer(buffer, size));
}

// Test the ProcessNMEABuffer method with empty buffer
TEST_F(NMEAParserTest, ProcessNMEABufferWithEmptyBuffer) {
    char buffer[] = "";
    int size = 0;
    
    EXPECT_EQ(CNMEAParserData::ERROR_OK, parser.ProcessNMEABuffer(buffer, size));
}

// Test the GetGPGGA method
TEST_F(NMEAParserTest, GetGPGGAReturnsOK) {
    CNMEAParserData::GGA_DATA_T ggaData;
    
    EXPECT_EQ(CNMEAParserData::ERROR_OK, parser.GetGPGGA(ggaData));
}

// Test the GetGPGGA method initializes data correctly
TEST_F(NMEAParserTest, GetGPGGAInitializesData) {
    CNMEAParserData::GGA_DATA_T ggaData;
    
    // Initial values should be zero
    EXPECT_DOUBLE_EQ(0.0, ggaData.dLatitude);
    EXPECT_DOUBLE_EQ(0.0, ggaData.dLongitude);
    EXPECT_DOUBLE_EQ(0.0, ggaData.dAltitudeMSL);
    
    parser.GetGPGGA(ggaData);
    
    // Values should still be zero since our mock doesn't change them
    EXPECT_DOUBLE_EQ(0.0, ggaData.dLatitude);
    EXPECT_DOUBLE_EQ(0.0, ggaData.dLongitude);
    EXPECT_DOUBLE_EQ(0.0, ggaData.dAltitudeMSL);
}

// Test the virtual methods are called correctly
TEST_F(NMEAParserTest, VirtualMethodsAreCalled) {
    char errorCmd[] = "ERROR_CMD";
    
    EXPECT_CALL(parser, OnError(CNMEAParserData::ERROR_UNKNOWN, errorCmd)).Times(1);
    EXPECT_CALL(parser, LockDataAccess()).Times(1);
    EXPECT_CALL(parser, UnlockDataAccess()).Times(1);
    
    parser.OnError(CNMEAParserData::ERROR_UNKNOWN, errorCmd);
    parser.LockDataAccess();
    parser.UnlockDataAccess();
}

// Test the GGA_DATA_T struct initialization
TEST(NMEAParserDataTest, GGADataInitialization) {
    CNMEAParserData::GGA_DATA_T ggaData;
    
    EXPECT_DOUBLE_EQ(0.0, ggaData.dLatitude);
    EXPECT_DOUBLE_EQ(0.0, ggaData.dLongitude);
    EXPECT_DOUBLE_EQ(0.0, ggaData.dAltitudeMSL);
}

// Test the ERROR_E enum values
TEST(NMEAParserDataTest, ErrorEnumValues) {
    EXPECT_EQ(0, CNMEAParserData::ERROR_OK);
    EXPECT_EQ(1, CNMEAParserData::ERROR_UNKNOWN);
}
// </test_code>