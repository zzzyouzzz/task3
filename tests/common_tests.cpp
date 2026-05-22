#include <cassert>
#include <climits>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "common.h"
#include "Logger.h"
#include "Parcel.h"
#include "User.h"

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

// ===================== parseInt tests =====================

static void testParseInt() {
    int value = 0;

    // Normal cases
    assert(parseInt("42", value));
    assert(value == 42);
    assert(parseInt("-100", value));
    assert(value == -100);
    assert(parseInt("0", value));
    assert(value == 0);

    // Boundary: INT_MAX / INT_MIN
    assert(parseInt(std::to_string(INT_MAX), value));
    assert(value == INT_MAX);
    assert(parseInt(std::to_string(INT_MIN), value));
    assert(value == INT_MIN);

    // Overflow: value too large for any integer type -> stol throws
    assert(!parseInt("99999999999999999999", value));

    // Invalid inputs
    assert(!parseInt("", value));
    assert(!parseInt("abc", value));
    assert(!parseInt("12.5", value));
    assert(!parseInt("12abc", value));

    // Leading whitespace: stol skips it
    assert(parseInt("  42", value));
    assert(value == 42);
}

// ===================== parseLongLong tests =====================

static void testParseLongLong() {
    long long value = 0;

    // Normal cases
    assert(parseLongLong("42", value));
    assert(value == 42);
    assert(parseLongLong("-100", value));
    assert(value == -100);
    assert(parseLongLong("0", value));
    assert(value == 0);

    // Boundary: LLONG_MAX / LLONG_MIN
    assert(parseLongLong(std::to_string(LLONG_MAX), value));
    assert(value == LLONG_MAX);
    assert(parseLongLong(std::to_string(LLONG_MIN), value));
    assert(value == LLONG_MIN);

    // Overflow
    assert(!parseLongLong("99999999999999999999", value));

    // Invalid inputs
    assert(!parseLongLong("", value));
    assert(!parseLongLong("abc", value));
    assert(!parseLongLong("12.5", value));
    assert(!parseLongLong("12abc", value));

    // Leading whitespace
    assert(parseLongLong("  42", value));
    assert(value == 42);
}

// ===================== parseDouble tests =====================

static void testParseDouble() {
    double value = 0.0;

    // Normal cases
    assert(parseDouble("3.14", value));
    assert(std::fabs(value - 3.14) < 1e-9);
    assert(parseDouble("-2.5", value));
    assert(std::fabs(value + 2.5) < 1e-9);
    assert(parseDouble("0", value));
    assert(std::fabs(value) < 1e-9);

    // Scientific notation
    assert(parseDouble("1e10", value));
    assert(std::fabs(value - 1e10) < 1e-3);

    // Invalid inputs
    assert(!parseDouble("", value));
    assert(!parseDouble("abc", value));

    // Trailing garbage: parseDouble accepts it (only checks idx != 0)
    assert(parseDouble("12.5abc", value));
    assert(std::fabs(value - 12.5) < 1e-9);

    // Leading whitespace: stod skips it, idx > 0, so accepted
    assert(parseDouble("  3.14", value));
    assert(std::fabs(value - 3.14) < 1e-9);

    // Whitespace only: stod throws
    assert(!parseDouble("   ", value));
}

// ===================== trimString tests =====================

static void testTrimString() {
    assert(trimString("  hello  ") == "hello");
    assert(trimString("\t\n  test\t\n") == "test");
    assert(trimString("nochange") == "nochange");
    assert(trimString("") == "");
    assert(trimString("   ") == "");
    assert(trimString("a") == "a");
    assert(trimString("  a  ") == "a");
    assert(trimString("  leading only") == "leading only");
    assert(trimString("trailing only  ") == "trailing only");
}

// ===================== User serialization tests =====================

static void testUserSerialization() {
    Customer alice("alice", "pwd", "Alice", "111", "Addr", 100.0);
    Courier bob("bob", "pwdb", "Bob", "222", "Addr2", 50.5);
    Administrator admin("admin", "sec", "Admin", "000", "Office", 999.99);

    assert(alice.serialize() == "0|alice|pwd|Alice|111|Addr|100");
    assert(bob.serialize() == "1|bob|pwdb|Bob|222|Addr2|50.5");
    assert(admin.serialize() == "2|admin|sec|Admin|000|Office|999.99");
}

// ===================== User method tests =====================

static void testUserMethods() {
    Customer user("testuser", "secret", "Test User", "123", "Addr", 100.0);

    // Getters
    assert(user.getUsername() == "testuser");
    assert(user.getName() == "Test User");
    assert(user.getPhone() == "123");
    assert(user.getAddress() == "Addr");
    assert(user.getUserType() == UserType::CUSTOMER);
    assert(user.getUserTypeStr() == "客户");

    // Login
    assert(user.login("secret"));
    assert(!user.login("wrong"));

    // changePassword: wrong old password should fail
    assert(!user.changePassword("wrong", "newpwd"));
    assert(user.login("secret")); // still works

    // changePassword: correct old password
    assert(user.changePassword("secret", "newpwd"));
    assert(user.login("newpwd"));

    // Balance operations
    assert(std::fabs(user.getBalance() - 100.0) < 1e-9);
    user.recharge(50.0);
    assert(std::fabs(user.getBalance() - 150.0) < 1e-9);

    // Deduct with sufficient balance
    assert(user.deduct(30.0));
    assert(std::fabs(user.getBalance() - 120.0) < 1e-9);

    // Deduct with insufficient balance
    assert(!user.deduct(200.0));
    assert(std::fabs(user.getBalance() - 120.0) < 1e-9);

    // addBalance
    user.addBalance(10.0);
    assert(std::fabs(user.getBalance() - 130.0) < 1e-9);
}

// ===================== Parcel serialization tests =====================

static void testParcelSerialization() {
    time_t sendTime = 1000000;
    time_t recvTime = 2000000;

    NormalParcel normal(ParcelType::NORMAL, "PID1", "alice", "bob",
                        sendTime, recvTime, ParcelStatus::PENDING_COLLECTION,
                        "test", 2.5, "");
    assert(normal.serialize() == "0|PID1|alice|bob|1000000|2000000|0|test|2.5|");

    FragileParcel fragile(ParcelType::FRAGILE, "PID2", "charlie", "dave",
                          sendTime, recvTime, ParcelStatus::PENDING_SIGN,
                          "glass", 1.2, "courier1");
    assert(fragile.serialize() == "1|PID2|charlie|dave|1000000|2000000|1|glass|1.2|courier1");

    BookParcel book(ParcelType::BOOK, "PID3", "eve", "frank",
                    sendTime, recvTime, ParcelStatus::SIGNED,
                    "novel", 3.0, "");
    assert(book.serialize() == "2|PID3|eve|frank|1000000|2000000|2|novel|3|");
}

// ===================== Parcel getPrice tests =====================

static void testParcelGetPrice() {
    NormalParcel normal("NP1", "s", "r", 2.5, "normal");
    assert(std::fabs(normal.getPrice() - 12.5) < 1e-9);

    FragileParcel fragile("FP1", "s", "r", 1.2, "fragile");
    assert(std::fabs(fragile.getPrice() - 9.6) < 1e-9);

    BookParcel book("BP1", "s", "r", 3.0, "book");
    assert(std::fabs(book.getPrice() - 6.0) < 1e-9);

    // Zero weight
    NormalParcel zero("ZP1", "s", "r", 0.0, "zero");
    assert(std::fabs(zero.getPrice()) < 1e-9);
}

// ===================== Parcel getters and setters tests =====================

static void testParcelGettersAndSetters() {
    NormalParcel p("PID1", "sender", "receiver", 2.5, "desc");

    assert(p.getParcelId() == "PID1");
    assert(p.getSenderName() == "sender");
    assert(p.getReceiverName() == "receiver");
    assert(p.getWeight() == 2.5);
    assert(p.getDescription() == "desc");
    assert(p.getParcelType() == ParcelType::NORMAL);

    p.setCourier("courier1");
    assert(p.getCourierName() == "courier1");

    p.setStatus(ParcelStatus::SIGNED);
    assert(p.getStatus() == ParcelStatus::SIGNED);

    p.setReceiveTime(12345);
    assert(p.getReceiveTime() == 12345);
}

int main() {
    try {
        testProtocolBuildAndParse();
        testLogLevelParsing();
        testLoggerFileWrite();
        testCommandRequestRoundTrip();

        testParseInt();
        testParseLongLong();
        testParseDouble();
        testTrimString();
        testUserSerialization();
        testUserMethods();
        testParcelSerialization();
        testParcelGetPrice();
        testParcelGettersAndSetters();
    } catch (const std::exception& ex) {
        std::cerr << "Test failed with exception: " << ex.what() << std::endl;
        return 1;
    }

    std::cout << "All common tests passed." << std::endl;
    return 0;
}
