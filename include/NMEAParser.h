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
    virtual CNMEAParserData::ERROR_E ProcessNMEABuffer([[maybe_unused]] char *pBuffer, [[maybe_unused]] int iSize)
    {
        return CNMEAParserData::ERROR_OK;
    }

    virtual CNMEAParserData::ERROR_E GetGPGGA([[maybe_unused]] CNMEAParserData::GGA_DATA_T &ggaData)
    {
        return CNMEAParserData::ERROR_OK;
    }

    // Virtual methods to override
    virtual void OnError([[maybe_unused]] CNMEAParserData::ERROR_E nError, [[maybe_unused]] char *pCmd) {}
    virtual void LockDataAccess() {}
    virtual void UnlockDataAccess() {}
};