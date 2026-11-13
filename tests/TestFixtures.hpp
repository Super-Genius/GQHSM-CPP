#pragma once

#include "gqhsm/Machine.hpp"

#include <fstream>
#include <stdexcept>
#include <string>

namespace testfixtures {

inline gqhsm::Json load(const std::string& filename) {
    const std::string path = std::string(GQHSM_TEST_FIXTURES_DIR) + "/" + filename;
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open fixture: " + path);
    return gqhsm::Json::parse(in);
}

inline std::shared_ptr<const gqhsm::Model> paymentModel() {
    return gqhsm::Model::Parse(load("payment.json"));
}

inline std::shared_ptr<const gqhsm::Model> hierarchyModel() {
    return gqhsm::Model::Parse(load("hierarchy.json"));
}

inline gqhsm::Json toggleModel() {
    return gqhsm::Json::parse(R"({
      "version":1, "name":"toggle", "initial":"off",
      "states":[
        {"id":"off","on":[{"event":"TOGGLE","target":"on"}]},
        {"id":"on","on":[{"event":"TOGGLE","target":"off"}]}
      ]
    })");
}
} // namespace testfixtures
