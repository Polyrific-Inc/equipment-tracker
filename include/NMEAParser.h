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