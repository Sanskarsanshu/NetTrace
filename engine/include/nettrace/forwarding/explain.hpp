#pragma once

#include "nettrace/forwarding/decision.hpp"
#include <string>
#include <utility>
#include <vector>

namespace nettrace {

// Structured educational explanation constructed from ForwardingDecision fields (M2-37).
struct Explanation {
    std::string what;
    std::string where;
    std::string why;
    std::vector<std::pair<std::string, std::string>> evidence;
    std::string whatNext;

    std::string getEvidence(const std::string& key) const {
        for (const auto& kv : evidence) {
            if (kv.first == key) return kv.second;
        }
        return "";
    }
};

class ExplanationGenerator {
public:
    static Explanation explain(const ForwardingDecision& decision);
};

inline Explanation explain(const ForwardingDecision& decision) {
    return ExplanationGenerator::explain(decision);
}

} // namespace nettrace
