#pragma once

#include "nettrace/model/error.hpp"
#include "nettrace/model/topology.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace nettrace {

enum class IssueSeverity {
    Error,
    Warning
};

std::string_view issueSeverityToString(IssueSeverity sev);

struct ValidationIssue {
    IssueSeverity severity = IssueSeverity::Error;
    std::string code;
    std::string path;
    std::string message;
    std::string hint;

    bool isError() const { return severity == IssueSeverity::Error; }
    bool isWarning() const { return severity == IssueSeverity::Warning; }

    bool operator==(const ValidationIssue& o) const {
        return severity == o.severity && code == o.code && path == o.path &&
               message == o.message && hint == o.hint;
    }

    bool operator<(const ValidationIssue& o) const;
};

struct ValidationReport {
    std::vector<ValidationIssue> issues;

    bool ok() const { return !hasErrors(); }
    bool hasErrors() const;
    bool hasWarnings() const;
    size_t errorCount() const;
    size_t warningCount() const;

    void addIssue(ValidationIssue issue);
    void addError(std::string code, std::string path, std::string msg, std::string hint = "");
    void addWarning(std::string code, std::string path, std::string msg, std::string hint = "");

    void sort(); // Deterministic: errors first, then path, then code
    std::string summary() const;
};

struct ValidationOptions {
    struct Limits {
        size_t maxNodes = 200;
        size_t maxLinks = 500;
        size_t maxInterfacesPerNode = 16;
    } limits;

    bool allowNegativeCosts = false;
};

ValidationReport validate(const TopologyDraft& draft, const ValidationOptions& options = {});

} // namespace nettrace
