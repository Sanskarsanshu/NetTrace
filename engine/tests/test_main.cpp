#include "test_framework.hpp"
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--list") {
            for (const auto& test : nettrace::test::TestRegistry::instance().tests()) {
                std::cout << test.name << "\n";
            }
            return 0;
        }
        return nettrace::test::TestRegistry::instance().runFilter(arg);
    }
    return nettrace::test::TestRegistry::instance().runAll();
}
