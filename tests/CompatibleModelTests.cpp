#include "TestFixtures.hpp"
#include <gtest/gtest.h>
#include <limits>

namespace {
using namespace gqhsm;
Json definition() {
    return {{"stateMachineInfo", {{"name", "compatible"}, {"version", 1}}},
            {"states", Json::array({{{"name", "open"}, {"isStartState", true},
                {"entryAction", "enter_open"}, {"exitAction", "exit_open"},
                {"children", Json::array({{{"name", "idle"}, {"entryAction", "enter_idle"},
                                          {"exitAction", "exit_idle"}}})}},
                                   {{"name", "done"}, {"entryAction", "enter_done"}}})},
            {"transitions", Json::array({{{"source", "open"}, {"target", "done"},
                                          {"eventSignal", "FINISH"}, {"action", "finish"}}})}};
}
void callbacks(Machine& machine, std::vector<std::string>& trace) {
    for (const auto* name : {"enter_open", "exit_open", "enter_idle", "exit_idle", "enter_done", "finish"})
        machine.setAction(name, [&, name](const Json&) { trace.emplace_back(name); });
}
TEST(CompatibleModel, NestedWorkflowUsesParentTransitionAndOrderedActions) {
    Machine machine(Model::ParseCompatible(definition()));
    std::vector<std::string> trace;
    callbacks(machine, trace);
    machine.start();
    EXPECT_EQ(machine.currentState(), "idle");
    EXPECT_EQ(machine.dispatch("FINISH"), Result::Transitioned);
    EXPECT_EQ(machine.currentState(), "done");
    EXPECT_EQ(trace, (std::vector<std::string>{"enter_open", "enter_idle", "exit_idle", "exit_open", "finish", "enter_done"}));
    Machine restored(Model::ParseCompatible(definition()));
    restored.restore(machine.snapshot());
    EXPECT_EQ(restored.currentState(), "done");
}
TEST(CompatibleModel, NormalSelfTransitionReentersSourceAndInitialChild) {
    auto value = definition();
    value["transitions"][0]["target"] = "open";
    Machine machine(Model::ParseCompatible(value));
    std::vector<std::string> trace;
    callbacks(machine, trace);
    machine.start();
    trace.clear();
    EXPECT_EQ(machine.dispatch("FINISH"), Result::Transitioned);
    EXPECT_EQ(machine.currentState(), "idle");
    EXPECT_EQ(trace, (std::vector<std::string>{"exit_idle", "exit_open", "finish", "enter_open", "enter_idle"}));
}
TEST(CompatibleModel, InternalTransitionKeepsActiveChild) {
    auto value = definition();
    value["transitions"][0]["target"] = "open";
    value["transitions"][0]["transitionType"] = "Internal";
    Machine machine(Model::ParseCompatible(value));
    std::vector<std::string> trace;
    callbacks(machine, trace);
    machine.start();
    trace.clear();
    EXPECT_EQ(machine.dispatch("FINISH"), Result::Handled);
    EXPECT_EQ(machine.currentState(), "idle");
    EXPECT_EQ(trace, (std::vector<std::string>{"finish"}));
}
TEST(CompatibleModel, LowerPriorityRunsFirstIncludingIntegerLimits) {
    auto value = definition();
    auto fallback = value["transitions"][0];
    value["transitions"][0]["evaluationOrderPriority"] = std::numeric_limits<int>::max();
    fallback["evaluationOrderPriority"] = std::numeric_limits<int>::min();
    fallback["guardCondition"] = "allow";
    value["transitions"].push_back(fallback);
    Machine machine(Model::ParseCompatible(value));
    std::vector<std::string> trace;
    callbacks(machine, trace);
    machine.setGuard("allow", [&](const Json&) { trace.push_back("guard"); return false; });
    machine.start();
    trace.clear();
    EXPECT_EQ(machine.dispatch("FINISH"), Result::Transitioned);
    ASSERT_FALSE(trace.empty());
    EXPECT_EQ(trace.front(), "guard");
}
TEST(CompatibleModel, MissingGuardFailsClosed) {
    auto value = definition();
    value["transitions"][0]["guardCondition"] = "missing";
    Machine machine(Model::ParseCompatible(value));
    std::vector<std::string> trace;
    callbacks(machine, trace);
    machine.start();
    EXPECT_THROW(machine.dispatch("FINISH"), std::runtime_error);
    EXPECT_EQ(machine.currentState(), "idle");
}
TEST(CompatibleModel, SelectsMarkedChildAndDefaultsVersion) {
    auto value = definition();
    value["stateMachineInfo"].erase("version");
    value["states"][0]["children"].push_back({{"name", "second"}, {"isStartState", true}});
    const auto model = Model::ParseCompatible(value);
    EXPECT_EQ(model->at("open").initial, "second");
}
TEST(CompatibleModel, ExistingNeoRouterFixtureRunsHitAndMissWorkflows) {
    const auto value = testfixtures::load("neo-router.json");
    for (const bool hit : {false, true}) {
        Machine machine(Model::ParseCompatible(value));
        std::string selected;
        for (const auto& state : value.at("states")) {
            const auto callback = state.value("entryAction", std::string{});
            if (!callback.empty()) machine.setAction(callback, [](const Json&) {});
        }
        machine.setGuard("confidence_above_threshold", [](const Json& j) {
            return j.at("confidence").get<double>() >= 0.8;
        });
        machine.setAction("set_selected_specialist", [&](const Json& j) {
            selected = j.at("specialist").get<std::string>();
        });
        machine.start();
        EXPECT_EQ(machine.dispatch("begin_classification"), Result::Transitioned);
        if (hit) {
            EXPECT_EQ(machine.dispatch("keyword_hit", {{"confidence", 0.2}}), Result::Guarded);
            EXPECT_EQ(machine.dispatch("keyword_hit", {{"confidence", 0.9}, {"specialist", "code"}}), Result::Transitioned);
            EXPECT_EQ(machine.currentState(), "select_specialist");
            EXPECT_EQ(selected, "code");
        } else {
            EXPECT_EQ(machine.dispatch("keyword_miss"), Result::Transitioned);
            EXPECT_EQ(machine.dispatch("regex_miss"), Result::Transitioned);
            EXPECT_EQ(machine.dispatch("syntax_miss"), Result::Transitioned);
            EXPECT_EQ(machine.currentState(), "no_match");
            EXPECT_TRUE(selected.empty());
        }
    }
}
TEST(CompatibleModel, RejectsInvalidAndUnsupportedModels) {
    const std::vector<std::function<void(Json&)>> mutations = {
        [](Json& j) { j["states"][1]["isStartState"] = true; },
        [](Json& j) { j["states"][1]["name"] = "idle"; },
        [](Json& j) { j["transitions"][0]["source"] = "missing"; },
        [](Json& j) { j["transitions"][0]["target"] = "missing"; },
        [](Json& j) { j["transitions"][0]["eventSource"] = "port"; },
        [](Json& j) { j["transitions"][0]["transitionType"] = "Internal"; },
        [](Json& j) { j["transitions"][0]["transitionType"] = "History"; },
        [](Json& j) { j["transitions"][0]["evaluationOrderPriority"] = 1.5; },
        [](Json& j) { j["transitions"][0]["evaluationOrderPriority"] = 2147483648LL; },
        [](Json& j) { j["stateMachineInfo"]["version"] = 2; },
        [](Json& j) { j["states"][0]["children"] = Json::object(); },
        [](Json& j) { j["states"][0]["timeout"] = 10; },
        [](Json& j) { j["transitions"].push_back(j["transitions"][0]); },
        [](Json& j) { j["states"] = Json::array(); }
    };
    for (std::size_t i = 0; i < mutations.size(); ++i) {
        SCOPED_TRACE(i);
        auto value = definition();
        mutations[i](value);
        EXPECT_THROW(Model::ParseCompatible(value), std::exception);
    }
}
} // namespace
