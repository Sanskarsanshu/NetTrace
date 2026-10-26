#include "test_framework.hpp"

int main() {
    return nettrace::test::TestRegistry::instance().runAll();
}
