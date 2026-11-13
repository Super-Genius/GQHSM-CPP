#include "gqhsm/Machine.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace gqhsm {
namespace {
std::vector<std::string> strings(const Json& src, const char* key) {
    std::vector<std::string> out;
    if (!src.contains(key)) return out;
    if (!src.at(key).is_array()) throw std::invalid_argument(std::string(key) + " must be an array");
    for (const auto& item : src.at(key)) out.push_back(item.get<std::string>());
    return out;
}
std::string required(const Json& j, const char* key) {
    auto result = j.at(key).get<std::string>();
    if (result.empty()) throw std::invalid_argument(std::string(key) + " must not be empty");
    return result;
}
} // namespace

std::shared_ptr<const Model> Model::Parse(const Json& src) {
    if (src.at("version") != 1) throw std::invalid_argument("Unsupported JSON model version");
    auto result = std::make_shared<Model>();
    result->name_ = required(src, "name");
    result->initial_ = required(src, "initial");
    for (const auto& value : src.at("states")) {
        State state;
        state.id = required(value, "id");
        state.parent = value.value("parent", std::string{});
        state.initial = value.value("initial", std::string{});
        state.entry = strings(value, "entry");
        state.exit = strings(value, "exit");
        if (value.contains("on")) {
            for (const auto& tr : value.at("on")) {
                Transition t;
                t.event = required(tr, "event");
                t.target = tr.value("target", std::string{});
                t.guard = tr.value("guard", std::string{});
                t.priority = tr.value("priority", 0);
                t.actions = strings(tr, "actions");
                state.transitions.push_back(std::move(t));
            }
        }
        if (!result->states_.emplace(state.id, std::move(state)).second)
            throw std::invalid_argument("Duplicate state id");
    }
    if (!result->states_.count(result->initial_)) throw std::invalid_argument("Unknown initial state");
    for (const auto& [name, state] : result->states_) {
        if (!state.parent.empty() && !result->states_.count(state.parent)) throw std::invalid_argument("Unknown parent");
        if (!state.initial.empty()) {
            auto it = result->states_.find(state.initial);
            if (it == result->states_.end() || it->second.parent != name) throw std::invalid_argument("Invalid initial child");
        }
        for (const auto& tr : state.transitions)
            if (!tr.target.empty() && !result->states_.count(tr.target)) throw std::invalid_argument("Unknown transition target");
        std::set<std::string> visited;
        std::string cur = name;
        while (!cur.empty()) {
            if (!visited.insert(cur).second) throw std::invalid_argument("Parent cycle");
            cur = result->states_.at(cur).parent;
        }
        for (std::size_t i = 0; i < state.transitions.size(); ++i)
            for (std::size_t j = i + 1; j < state.transitions.size(); ++j)
                if (state.transitions[i].event == state.transitions[j].event &&
                    state.transitions[i].priority == state.transitions[j].priority)
                    throw std::invalid_argument("Ambiguous event priority");
    }
    return result;
}
const State& Model::at(const std::string& name) const {
    return states_.at(name);
}
Machine::Machine(std::shared_ptr<const Model> model) : model_(std::move(model)) {
    if (!model_) throw std::invalid_argument("Missing machine model");
}
void Machine::setGuard(const std::string& name, Guard guard) { guards_[name] = std::move(guard); }
void Machine::setAction(const std::string& name, Action action) { actions_[name] = std::move(action); }
void Machine::execute(const std::vector<std::string>& names, const Json& payload) {
    for (const auto& name : names) {
        const auto it = actions_.find(name);
        if (it == actions_.end()) throw std::runtime_error("Missing action: " + name);
        it->second(payload);
    }
}
std::vector<std::string> Machine::ancestors(const std::string& state) const {
    std::vector<std::string> chain;
    std::string current = state;
    while (!current.empty()) {
        chain.push_back(current);
        current = model_->at(current).parent;
    }
    return chain;
}
std::string Machine::descendInitial(std::string state, const Json& payload) {
    std::set<std::string> seen;
    while (!model_->at(state).initial.empty()) {
        if (!seen.insert(state).second) throw std::runtime_error("Initial child cycle");
        state = model_->at(state).initial;
        execute(model_->at(state).entry, payload);
    }
    return state;
}
void Machine::start(const Json& payload) {
    if (busy_ || !state_.empty()) throw std::logic_error("Machine already started or dispatching");
    busy_ = true;
    try {
        auto parents = ancestors(model_->initial());
        for (auto it = parents.rbegin(); it != parents.rend(); ++it)
            execute(model_->at(*it).entry, payload);
        state_ = descendInitial(model_->initial(), payload);
    } catch (...) {
        busy_ = false;
        throw;
    }
    busy_ = false;
}
Result Machine::dispatch(const std::string& event, const Json& payload) {
    if (state_.empty()) throw std::logic_error("Machine not started");
    if (busy_) throw std::logic_error("Reentrant dispatch is unsupported");
    busy_ = true;
    try {
        bool any = false;
        for (const auto& source : ancestors(state_)) {
            std::vector<const Transition*> rules;
            for (const auto& tr : model_->at(source).transitions) if (tr.event == event) rules.push_back(&tr);
            if (rules.empty()) continue;
            any = true;
            std::sort(rules.begin(), rules.end(), [](auto a, auto b) { return a->priority > b->priority; });
            for (const auto* rule : rules) {
                if (!rule->guard.empty()) {
                    auto it = guards_.find(rule->guard);
                    if (it == guards_.end()) throw std::runtime_error("Missing guard: " + rule->guard);
                    if (!it->second(payload)) continue;
                }
                if (!rule->target.empty()) {
                    const auto oldPath = ancestors(state_);
                    const auto nextPath = ancestors(rule->target);
                    std::size_t oldKeep = oldPath.size(), newKeep = nextPath.size();
                    while (oldKeep && newKeep && oldPath[oldKeep - 1] == nextPath[newKeep - 1]) {
                        --oldKeep;
                        --newKeep;
                    }
                    for (std::size_t i = 0; i < oldKeep; ++i) execute(model_->at(oldPath[i]).exit, payload);
                    execute(rule->actions, payload);
                    for (std::size_t i = newKeep; i > 0; --i) execute(model_->at(nextPath[i - 1]).entry, payload);
                    state_ = descendInitial(rule->target, payload);
                    busy_ = false;
                    return Result::Transitioned;
                }
                execute(rule->actions, payload);
                busy_ = false;
                return Result::Handled;
            }
        }
        busy_ = false;
        return any ? Result::Guarded : Result::Unhandled;
    } catch (...) {
        busy_ = false;
        throw;
    }
}
Json Machine::snapshot() const {
    if (busy_) throw std::logic_error("Cannot snapshot during dispatch");
    return {{"model", model_->name()}, {"version", 1}, {"state", state_}};
}
void Machine::restore(const Json& snap) {
    if (busy_) throw std::logic_error("Cannot restore during dispatch");
    if (snap.at("version") != 1 || snap.at("model") != model_->name()) throw std::invalid_argument("Snapshot model mismatch");
    const auto state = snap.at("state").get<std::string>();
    if (!state.empty()) model_->at(state);
    state_ = state;
}
} // namespace gqhsm
