#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "common.h"
#include "Logger.h"

static void testProtocolBuildAndParse() {
    std::string expectedCmd = Command::SEND_PARCEL;
    std::vector<std::string> expectedArgs = {"alice", "1", "2.5", "fragile-book"};

    std::string rawRequest = Protocol::buildRequest(expectedCmd, expectedArgs);
    assert(!rawRequest.empty());
    assert(rawRequest.back() == '\n');

    std::string parsedCmd;
    std::vector<std::string> parsedArgs;
    Protocol::parseRequest(rawRequest, parsedCmd, parsedArgs);

    assert(parsedCmd == expectedCmd);
    assert(parsedArgs.size() == expectedArgs.size());
    for (size_t i = 0; i < expectedArgs.size(); ++i) {
        assert(parsedArgs[i] == expectedArgs[i]);
    }
}

static void testLogLevelParsing() {
    assert(parseLogLevel("debug") == LOG_DEBUG);
    assert(parseLogLevel("INFO") == LOG_INFO);
    assert(parseLogLevel("Warn") == LOG_WARNING);
    assert(parseLogLevel("error") == LOG_ERROR);
    assert(parseLogLevel("fatal") == LOG_FATAL);
    assert(parseLogLevel("unknown") == LOG_INFO);
}

static void testLoggerFileWrite() {
    namespace fs = std::filesystem;
    fs::path tempPath = fs::current_path() / "common_test_logger.log";
    if (fs::exists(tempPath)) {
        fs::remove(tempPath);
    }

    {
        Logger testLogger;
        bool initialized = testLogger.initialize(tempPath.string(), LOG_DEBUG, false);
        assert(initialized);

        testLogger.info("Logger file write test");
        testLogger.debug("Debug entry for test");
    }

    std::ifstream ifs(tempPath);
    assert(ifs.is_open());

    std::string line;
    bool foundInfo = false;
    bool foundDebug = false;
    while (std::getline(ifs, line)) {
        if (line.find("Logger file write test") != std::string::npos) {
            foundInfo = true;
        }
        if (line.find("Debug entry for test") != std::string::npos) {
            foundDebug = true;
        }
    }

    assert(foundInfo);
    assert(foundDebug);

    ifs.close();
    fs::remove(tempPath);
}

static void testCommandRequestRoundTrip() {
    std::string cmd = Command::QUERY_PARCEL;
    std::vector<std::string> args = {"P123", "alice", "bob", "courier1", "0", "0", "0"};

    std::string raw = Protocol::buildRequest(cmd, args);
    std::string parsedCmd;
    std::vector<std::string> parsedArgs;
    Protocol::parseRequest(raw, parsedCmd, parsedArgs);

    assert(parsedCmd == cmd);
    assert(parsedArgs == args);
}

int main() {
    try {
        testProtocolBuildAndParse();
        testLogLevelParsing();
        testLoggerFileWrite();
        testCommandRequestRoundTrip();
    } catch (const std::exception& ex) {
        std::cerr << "Test failed with exception: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "All common tests passed." << std::endl;
    return 0;
}
