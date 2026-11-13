#include "gqhsm/Machine.hpp"

#include <cstdint>
#include <limits>
#include <set>

namespace gqhsm {
namespace {
void fields(const Json& value, const std::set<std::string>& allowed) {
    if (!value.is_object()) throw std::invalid_argument("Expected a JSON object");
    for (auto it = value.begin(); it != value.end(); ++it)
        if (!allowed.count(it.key()))
            throw std::invalid_argument("Unsupported compatibility field: " + it.key());
}
std::string textField(const Json& value, const char* key) {
    const auto result = value.at(key).get<std::string>();
    if (result.empty()) throw std::invalid_argument(std::string(key) + " must not be empty");
    return result;
}
void action(const Json& source, const char* from, Json& target, const char* to) {
    if (source.contains(from)) {
        const auto name = source.at(from).get<std::string>();
        if (!name.empty()) target[to] = Json::array({name});
    }
}
std::string flatten(const Json& states, const std::string& parent, Json& output,
                    std::map<std::string, std::size_t>& indices) {
    if (!states.is_array() || states.empty())
        throw std::invalid_argument("Each state list must be a nonempty array");
    std::string initial;
    for (const auto& state : states) {
        fields(state, {"name", "isStartState", "entryAction", "exitAction", "children", "metadata"});
        const auto name = textField(state, "name");
        if (state.value("isStartState", false)) {
            if (!initial.empty()) throw std::invalid_argument("Multiple start states among siblings");
            initial = name;
        }
        Json flat = {{"id", name}, {"parent", parent}, {"on", Json::array()}};
        action(state, "entryAction", flat, "entry");
        action(state, "exitAction", flat, "exit");
        if (!indices.emplace(name, output.size()).second)
            throw std::invalid_argument("Duplicate state name");
        const auto index = output.size();
        output.push_back(flat);
        if (state.contains("children") && !state.at("children").empty())
            output[index]["initial"] = flatten(state.at("children"), name, output, indices);
        else if (state.contains("children") && !state.at("children").is_array())
            throw std::invalid_argument("children must be an array");
    }
    return initial.empty() ? textField(states.front(), "name") : initial;
}
} // namespace

std::shared_ptr<const Model> Model::ParseCompatible(const Json& source) {
    fields(source, {"stateMachineInfo", "states", "transitions"});
    const auto& info = source.at("stateMachineInfo");
    fields(info, {"name", "version", "description", "namespace"});
    if (info.contains("version") &&
        (!info.at("version").is_number_integer() || info.at("version") != 1))
        throw std::invalid_argument("Unsupported compatibility model version");
    Json flat = {{"version", 1}, {"name", textField(info, "name")}, {"states", Json::array()}};
    std::map<std::string, std::size_t> indices;
    flat["initial"] = flatten(source.at("states"), "", flat["states"], indices);
    if (!source.at("transitions").is_array())
        throw std::invalid_argument("transitions must be an array");
    for (const auto& transition : source.at("transitions")) {
        fields(transition, {"name", "eventSignal", "eventSource", "guardCondition", "action",
                            "evaluationOrderPriority", "transitionType", "source", "target"});
        const auto from = textField(transition, "source");
        const auto to = textField(transition, "target");
        if (!indices.count(from) || !indices.count(to))
            throw std::invalid_argument("Unknown transition source or target");
        if (!transition.value("eventSource", std::string{}).empty())
            throw std::invalid_argument("Event ports are not supported yet");
        const auto kind = transition.value("transitionType", std::string{"Normal"});
        if (kind != "Normal" && kind != "Internal")
            throw std::invalid_argument("Unsupported transitionType");
        if (kind == "Internal" && from != to)
            throw std::invalid_argument("Internal transitions must target their source");
        const auto priority = transition.value("evaluationOrderPriority", Json(0));
        if (!priority.is_number_integer() || priority < std::numeric_limits<int>::min() ||
            priority > std::numeric_limits<int>::max())
            throw std::invalid_argument("evaluationOrderPriority must be a signed 32-bit integer");
        // Reverse ordering without overflowing at INT_MIN.
        const auto reversed = -1 - priority.get<std::int64_t>();
        Json rule = {{"event", textField(transition, "eventSignal")},
                     {"target", kind == "Internal" ? "" : to},
                     {"external", kind == "Normal"}, {"priority", reversed},
                     {"guard", transition.value("guardCondition", std::string{})}};
        action(transition, "action", rule, "actions");
        flat["states"][indices.at(from)]["on"].push_back(rule);
    }
    return Parse(flat);
}
} // namespace gqhsm
