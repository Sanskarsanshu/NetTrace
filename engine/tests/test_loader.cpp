#include "test_framework.hpp"
#include "nettrace/model/topology_io.hpp"
#include "nettrace/model/validate.hpp"
#include "nettrace/model/mac.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace nettrace;

// -----------------------------------------------------------------------------
// T-2-31: Loader error fixtures (TEST_VECTORS.md section 4.10)
// -----------------------------------------------------------------------------
TEST_CASE("T-2-31 loader error fixtures") {
    // 1. "{" (truncated) -> INVALID_JSON
    {
        auto res = loadTopologyJson("{");
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::InvalidJson);
    }

    // Baseline minimal valid JSON
    std::string baseValid = R"({
  "schemaVersion": "1.0",
  "id": "test-topo",
  "name": "Test Topology",
  "nodes": [
    {
      "id": "h1",
      "name": "Host 1",
      "type": "host",
      "interfaces": [
        {
          "id": "eth0",
          "name": "eth0"
        }
      ]
    }
  ],
  "links": []
})";

    // 2. Valid JSON without "nodes" -> MISSING_FIELD, path "nodes"
    {
        std::string jsonNoNodes = R"({
  "schemaVersion": "1.0",
  "id": "test-topo",
  "name": "Test Topology",
  "links": []
})";
        auto res = loadTopologyJson(jsonNoNodes);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::MissingField);
        REQUIRE(res.error().fieldPath.find("nodes") != std::string::npos);
    }

    // 3. "nodes" is a string -> INVALID_FIELD_TYPE, path "nodes"
    {
        std::string jsonNodesString = R"({
  "schemaVersion": "1.0",
  "id": "test-topo",
  "name": "Test Topology",
  "nodes": "not an array",
  "links": []
})";
        auto res = loadTopologyJson(jsonNodesString);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::InvalidFieldType);
        REQUIRE(res.error().fieldPath.find("nodes") != std::string::npos);
    }

    // 4. Node type "firewall" -> INVALID_ENUM_VALUE, path to that node's type
    {
        std::string jsonInvalidType = R"({
  "schemaVersion": "1.0",
  "id": "test-topo",
  "name": "Test Topology",
  "nodes": [
    {
      "id": "fw1",
      "name": "Firewall",
      "type": "firewall",
      "interfaces": []
    }
  ],
  "links": []
})";
        auto res = loadTopologyJson(jsonInvalidType);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::InvalidEnumValue);
        REQUIRE(res.error().fieldPath.find("type") != std::string::npos);
    }

    // 5. Interface address "1.2.3" -> INVALID_ADDRESS
    {
        std::string jsonBadIp = R"({
  "schemaVersion": "1.0",
  "id": "test-topo",
  "name": "Test Topology",
  "nodes": [
    {
      "id": "h1",
      "name": "Host 1",
      "type": "host",
      "interfaces": [
        {
          "id": "eth0",
          "name": "eth0",
          "ipv4Address": "1.2.3",
          "prefixLength": 24
        }
      ]
    }
  ],
  "links": []
})";
        auto res = loadTopologyJson(jsonBadIp);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::InvalidAddress);
        REQUIRE(res.error().fieldPath.find("ipv4") != std::string::npos);
    }

    // 6. Prefix 33 -> INVALID_PREFIX
    {
        std::string jsonBadPrefix = R"({
  "schemaVersion": "1.0",
  "id": "test-topo",
  "name": "Test Topology",
  "nodes": [
    {
      "id": "h1",
      "name": "Host 1",
      "type": "host",
      "interfaces": [
        {
          "id": "eth0",
          "name": "eth0",
          "ipv4Address": "192.168.1.10",
          "prefixLength": 33
        }
      ]
    }
  ],
  "links": []
})";
        auto res = loadTopologyJson(jsonBadPrefix);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::InvalidPrefix);
        REQUIRE(res.error().fieldPath.find("prefix") != std::string::npos);
    }

    // 7. MAC "02-00-00-00-00-01" -> INVALID_MAC
    {
        std::string jsonBadMac = R"({
  "schemaVersion": "1.0",
  "id": "test-topo",
  "name": "Test Topology",
  "nodes": [
    {
      "id": "h1",
      "name": "Host 1",
      "type": "host",
      "interfaces": [
        {
          "id": "eth0",
          "name": "eth0",
          "mac": "02-00-00-00-00-01"
        }
      ]
    }
  ],
  "links": []
})";
        auto res = loadTopologyJson(jsonBadMac);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::InvalidMac);
        REQUIRE(res.error().fieldPath.find("mac") != std::string::npos);
    }

    // 8. schemaVersion "2.0" -> UNSUPPORTED_SCHEMA_VERSION
    {
        std::string jsonBadVersion = R"({
  "schemaVersion": "2.0",
  "id": "test-topo",
  "name": "Test Topology",
  "nodes": [],
  "links": []
})";
        auto res = loadTopologyJson(jsonBadVersion);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::UnsupportedSchemaVersion);
        REQUIRE_EQ(res.error().fieldPath, "schemaVersion");
    }

    // 9. Input larger than 2 MB -> LIMIT_EXCEEDED
    {
        std::string largeInput(2 * 1024 * 1024 + 10, ' ');
        auto res = loadTopologyJson(largeInput);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::LimitExceeded);
    }

    // 10. Nesting deeper than 64 (100000 nested brackets) -> LIMIT_EXCEEDED
    {
        std::string deepNesting;
        deepNesting.reserve(200000);
        for (int i = 0; i < 100000; ++i) {
            deepNesting.push_back('[');
        }
        for (int i = 0; i < 100000; ++i) {
            deepNesting.push_back(']');
        }
        auto res = loadTopologyJson(deepNesting);
        REQUIRE(!res.isOk());
        REQUIRE_EQ(res.error().code, ErrorCode::LimitExceeded);
    }

    // 11. Unknown optional field is ignored, loads OK
    {
        std::string jsonUnknownField = R"({
  "schemaVersion": "1.0",
  "id": "test-topo",
  "name": "Test Topology",
  "customUnknownExtension": { "foo": "bar", "flag": true },
  "nodes": [
    {
      "id": "h1",
      "name": "Host 1",
      "type": "host",
      "extraMetadata": 12345,
      "interfaces": [
        {
          "id": "eth0",
          "name": "eth0",
          "vendorNotes": "tested"
        }
      ]
    }
  ],
  "links": []
})";
        auto res = loadTopologyJson(jsonUnknownField);
        REQUIRE(res.isOk());
        REQUIRE_EQ(res.value().id, "test-topo");
        REQUIRE_EQ(res.value().nodes.size(), 1);
        REQUIRE_EQ(res.value().nodes[0].interfaces.size(), 1);
    }
}

static std::string readExampleFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in.is_open()) return "";
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// -----------------------------------------------------------------------------
// T-2-32: Round trip and canonical bytes
// -----------------------------------------------------------------------------
TEST_CASE("T-2-32 JSON round trip and canonical bytes") {
    const std::vector<std::string> exampleFiles = {
        "same-subnet.json",
        "gateway-routing.json",
        "multi-router-cloud.json",
        "ttl-expiry.json",
        "no-route.json",
        "bandwidth-vs-cost.json",
        "link-failover.json",
        "mtu-df.json",
        "fragmentation.json",
        "lossy.json",
        "loop.json",
        "shortcut-cost-vs-hops.json"
    };

    // Try relative paths from tests or workspace root
    std::vector<std::string> prefixes = {"examples/", "../examples/", "../../examples/"};
    std::string prefixFound = "";
    for (const auto& p : prefixes) {
        if (!readExampleFile(p + exampleFiles[0]).empty()) {
            prefixFound = p;
            break;
        }
    }
    REQUIRE(!prefixFound.empty());

    for (const auto& fname : exampleFiles) {
        std::string rawBytes = readExampleFile(prefixFound + fname);
        REQUIRE(!rawBytes.empty());

        // 1. First load
        auto draft1Res = loadTopologyJson(rawBytes);
        REQUIRE(draft1Res.isOk());
        auto draft1 = draft1Res.value();

        // Normalize (ensure MACs)
        assignMissingMacs(draft1);

        auto snap1Res = TopologySnapshot::freeze(draft1);
        REQUIRE(snap1Res.isOk());
        auto snap1 = snap1Res.value();

        // 2. toJson
        std::string canonicalJson = toJson(snap1);

        // 3. Second load
        auto draft2Res = loadTopologyJson(canonicalJson);
        REQUIRE(draft2Res.isOk());
        auto draft2 = draft2Res.value();

        // Second normalize must NOT change any MAC
        auto draft2BeforeMacs = draft2;
        assignMissingMacs(draft2);
        for (size_t n = 0; n < draft2.nodes.size(); ++n) {
            for (size_t i = 0; i < draft2.nodes[n].interfaces.size(); ++i) {
                REQUIRE_EQ(draft2.nodes[n].interfaces[i].mac.has_value(),
                           draft2BeforeMacs.nodes[n].interfaces[i].mac.has_value());
                if (draft2.nodes[n].interfaces[i].mac.has_value()) {
                    REQUIRE_EQ(*draft2.nodes[n].interfaces[i].mac,
                               *draft2BeforeMacs.nodes[n].interfaces[i].mac);
                }
            }
        }

        auto snap2Res = TopologySnapshot::freeze(draft2);
        REQUIRE(snap2Res.isOk());
        auto snap2 = snap2Res.value();

        // 4. Content hashes match
        REQUIRE_EQ(snap1.contentHash(), snap2.contentHash());

        const char* canonEnv = std::getenv("CANONICALIZE_EXAMPLES");
        if (canonEnv && std::string(canonEnv) == "1") {
            std::ofstream out(prefixFound + fname, std::ios::binary);
            out << canonicalJson;
            rawBytes = canonicalJson;
        }

        // 5. Canonical file bytes equality: toJson(freeze(load(file))) equals the committed bytes
        // Normalize line endings if Windows CRLF
        std::string fileText = rawBytes;
        fileText.erase(std::remove_if(fileText.begin(), fileText.end(), [](char c) { return c == '\r'; }), fileText.end());
        std::string canonText = canonicalJson;
        canonText.erase(std::remove_if(canonText.begin(), canonText.end(), [](char c) { return c == '\r'; }), canonText.end());
        REQUIRE_EQ(canonText, fileText);
    }
}
