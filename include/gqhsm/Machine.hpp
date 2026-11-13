#pragma once

#include <functional>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace gqhsm {

using Json = nlohmann::json;
using Guard = std::function<bool(const Json&)>;
using Action = std::function<void(const Json&)>;

struct Transition {
    std::string event;
    std::string target;
    std::string guard;
    std::vector<std::string> actions;
    int priority{0};
};
struct State {
    std::string id;
    std::string parent;
    std::string initial;
    std::vector<std::string> entry;
    std::vector<std::string> exit;
    std::vector<Transition> transitions;
};
class Model {
public:
    static std::shared_ptr<const Model> Parse(const Json& source);
    const std::string& name() const noexcept { return name_; }
    const std::string& initial() const noexcept { return initial_; }
    const State& at(const std::string& name) const;
private:
    std::string name_;
    std::string initial_;
    std::map<std::string, State> states_;
    friend class Machine;
};
enum class Result { Transitioned, Handled, Guarded, Unhandled };
class Machine {
public:
    explicit Machine(std::shared_ptr<const Model> model);
    void setGuard(const std::string& name, Guard guard);
    void setAction(const std::string& name, Action action);
    void start(const Json& payload = Json::object());
    Result dispatch(const std::string& event, const Json& payload = Json::object());
    Json snapshot() const;
    void restore(const Json& snapshot);
    const std::string& currentState() const noexcept { return state_; }
private:
    std::shared_ptr<const Model> model_;
    std::map<std::string, Guard> guards_;
    std::map<std::string, Action> actions_;
    std::string state_;
    bool busy_{false};
    void execute(const std::vector<std::string>& names, const Json& payload);
    std::string descendInitial(std::string state, const Json& payload);
    std::vector<std::string> ancestors(const std::string& state) const;
};
} // namespace gqhsm
