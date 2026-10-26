#pragma once

#include <iostream>
#include <string>
#include <vector>
#include <functional>
#include <sstream>
#include <stdexcept>

namespace nettrace::test {

struct TestFailure : public std::runtime_error {
    TestFailure(const std::string& msg) : std::runtime_error(msg) {}
};

struct TestCase {
    std::string name;
    std::function<void()> func;
};

class TestRegistry {
public:
    static TestRegistry& instance() {
        static TestRegistry reg;
        return reg;
    }

    void addTest(std::string name, std::function<void()> func) {
        tests_.push_back({std::move(name), std::move(func)});
    }

    int runAll() {
        int passed = 0;
        int failed = 0;
        std::cout << "==========================================\n";
        std::cout << "   NetTrace C++ Unit Test Runner (" << tests_.size() << " tests)\n";
        std::cout << "==========================================\n";

        for (const auto& test : tests_) {
            try {
                test.func();
                std::cout << "[PASS] " << test.name << "\n";
                passed++;
            } catch (const TestFailure& e) {
                std::cout << "[FAIL] " << test.name << "\n       " << e.what() << "\n";
                failed++;
            } catch (const std::exception& e) {
                std::cout << "[EXCEPTION] " << test.name << "\n            " << e.what() << "\n";
                failed++;
            } catch (...) {
                std::cout << "[CRASH] " << test.name << " threw an unknown exception\n";
                failed++;
            }
        }

        std::cout << "------------------------------------------\n";
        std::cout << "Summary: " << passed << " passed, " << failed << " failed\n";
        std::cout << "==========================================\n";
        return failed == 0 ? 0 : 1;
    }

    const std::vector<TestCase>& tests() const { return tests_; }

    int runFilter(const std::string& pattern) {
        int passed = 0;
        int failed = 0;
        int matched = 0;
        std::cout << "==========================================\n";
        std::cout << "   NetTrace C++ Unit Test Runner (filter: " << pattern << ")\n";
        std::cout << "==========================================\n";

        for (const auto& test : tests_) {
            if (test.name.find(pattern) != std::string::npos) {
                matched++;
                try {
                    test.func();
                    std::cout << "[PASS] " << test.name << "\n";
                    passed++;
                } catch (const TestFailure& e) {
                    std::cout << "[FAIL] " << test.name << "\n       " << e.what() << "\n";
                    failed++;
                } catch (const std::exception& e) {
                    std::cout << "[EXCEPTION] " << test.name << "\n            " << e.what() << "\n";
                    failed++;
                } catch (...) {
                    std::cout << "[CRASH] " << test.name << " threw an unknown exception\n";
                    failed++;
                }
            }
        }

        std::cout << "------------------------------------------\n";
        std::cout << "Summary: " << passed << " passed, " << failed << " failed (matched " << matched << ")\n";
        std::cout << "==========================================\n";
        if (matched == 0) {
            std::cerr << "Error: No tests matched filter '" << pattern << "'\n";
            return 2;
        }
        return failed == 0 ? 0 : 1;
    }

private:
    std::vector<TestCase> tests_;
};

struct AutoRegister {
    AutoRegister(const std::string& name, std::function<void()> func) {
        TestRegistry::instance().addTest(name, std::move(func));
    }
};

} // namespace nettrace::test

#define NT_CONCAT_INTERNAL(a, b) a##b
#define NT_CONCAT(a, b) NT_CONCAT_INTERNAL(a, b)

#define TEST_CASE(name) \
    static void NT_CONCAT(test_func_, __LINE__)(); \
    static ::nettrace::test::AutoRegister NT_CONCAT(reg_, __LINE__)(name, NT_CONCAT(test_func_, __LINE__)); \
    static void NT_CONCAT(test_func_, __LINE__)()

#define REQUIRE(expr) \
    do { \
        if (!(expr)) { \
            std::ostringstream oss; \
            oss << "REQUIRE failed at " << __FILE__ << ":" << __LINE__ << " (" #expr ")"; \
            throw ::nettrace::test::TestFailure(oss.str()); \
        } \
    } while (0)

#define REQUIRE_FALSE(expr) REQUIRE(!(expr))

#define REQUIRE_EQ(a, b) \
    do { \
        if (!((a) == (b))) { \
            std::ostringstream oss; \
            oss << "REQUIRE_EQ failed at " << __FILE__ << ":" << __LINE__ \
                << " [" << (a) << " != " << (b) << "] (" #a " == " #b ")"; \
            throw ::nettrace::test::TestFailure(oss.str()); \
        } \
    } while (0)

#define CHECK(expr) \
    do { \
        if (!(expr)) { \
            std::cerr << "CHECK warning at " << __FILE__ << ":" << __LINE__ << " (" #expr ")\n"; \
        } \
    } while (0)
