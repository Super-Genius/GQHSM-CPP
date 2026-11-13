// Copyright 2026 Genius Ventures, Inc.
#include "TestFixtures.hpp"

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <vector>

namespace {
using gqhsm::Json;
using gqhsm::Machine;
using gqhsm::Model;
using gqhsm::Result;

void registerActions(Machine& m, std::vector<std::string>& trace, const std::string& skip = "") {
    for (const char* name : {
        "root_enter", "root_exit", "a_enter", "a_exit", "idle_enter", "idle_exit",
        "done_enter", "done_exit", "b_enter", "b_exit", "b_idle_enter",
        "b_idle_exit", "end_enter", "move", "escalate", "local", "restart"
    }) {
        if (name != skip) m.setAction(name, [&, name](const Json&) { trace.emplace_back(name); });
    }
}

TEST(MachineLifecycle, NullModelRejected) {
    EXPECT_THROW(Machine(nullptr), std::invalid_argument);
}

TEST(MachineLifecycle, DispatchBeforeStartRejected) {
    Machine m(testfixtures::hierarchyModel());
    EXPECT_THROW(m.dispatch("MOVE"), std::logic_error);
}

TEST(MachineLifecycle, NestedEntryActionsRunRootToLeaf) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.start();
    EXPECT_EQ(m.currentState(), "a_idle");
    EXPECT_EQ(trace, (std::vector<std::string>{"root_enter", "a_enter", "idle_enter"}));
}

TEST(MachineLifecycle, StartPayloadDeliveredToAllEntryActions) {
    Machine m(testfixtures::hierarchyModel());
    int calls = 0;
    for (const char* name : {"root_enter", "a_enter", "idle_enter"}) {
        m.setAction(name, [&](const Json& payload) {
            EXPECT_EQ(payload.at("request_id"), "r1");
            ++calls;
        });
    }
    m.start({{"request_id", "r1"}});
    EXPECT_EQ(calls, 3);
}

TEST(MachineLifecycle, DoubleStartRejected) {
    Machine m(Model::Parse(testfixtures::toggleModel()));
    m.start();
    EXPECT_THROW(m.start(), std::logic_error);
}

TEST(MachineLifecycle, MissingEntryActionFailsWithoutStarting) {
    Machine m(testfixtures::hierarchyModel());
    EXPECT_THROW(m.start(), std::runtime_error);
    EXPECT_TRUE(m.currentState().empty());
    std::vector<std::string> trace;
    registerActions(m, trace);
    EXPECT_NO_THROW(m.start());
    EXPECT_EQ(m.currentState(), "a_idle");
}

TEST(MachineEvents, ActionOnlyEventLeavesStateIntact) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.start();
    trace.clear();
    EXPECT_EQ(m.dispatch("LOCAL"), Result::Handled);
    EXPECT_EQ(m.currentState(), "a_idle");
    EXPECT_EQ(trace, (std::vector<std::string>{"local"}));
}

TEST(MachineEvents, UnknownEventDoesNotInvokeCallbacks) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.start();
    trace.clear();
    EXPECT_EQ(m.dispatch("UNKNOWN"), Result::Unhandled);
    EXPECT_EQ(m.currentState(), "a_idle");
    EXPECT_TRUE(trace.empty());
}

TEST(MachineEvents, SiblingTransitionOrdersExitEffectEntry) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.start();
    trace.clear();
    EXPECT_EQ(m.dispatch("MOVE"), Result::Transitioned);
    EXPECT_EQ(m.currentState(), "a_done");
    EXPECT_EQ(trace, (std::vector<std::string>{"idle_exit", "move", "done_enter"}));
}

TEST(MachineEvents, ParentEventPropagatesAndTransitionsAcrossBranches) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.start();
    trace.clear();
    EXPECT_EQ(m.dispatch("ESCALATE"), Result::Transitioned);
    EXPECT_EQ(m.currentState(), "b_idle");
    EXPECT_EQ(trace, (std::vector<std::string>{
        "idle_exit", "a_exit", "escalate", "b_enter", "b_idle_enter"
    }));
}

TEST(MachineEvents, EventPayloadDeliveredToTransitionEffect) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.setAction("move", [&](const Json& payload) {
        EXPECT_EQ(payload.at("ticket"), 723);
        trace.emplace_back("move");
    });
    m.start();
    EXPECT_EQ(m.dispatch("MOVE", {{"ticket", 723}}), Result::Transitioned);
}

TEST(MachineGuards, HigherPriorityPassingGuardWins) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.setGuard("can_move", [](const Json& payload) { return payload.value("allowed", false); });
    m.start();
    EXPECT_EQ(m.dispatch("CHOICE", {{"allowed", true}}), Result::Transitioned);
    EXPECT_EQ(m.currentState(), "a_done");
}

TEST(MachineGuards, FailedHighPriorityGuardFallsBackToLowerPriority) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.setGuard("can_move", [](const Json&) { return false; });
    m.start();
    EXPECT_EQ(m.dispatch("CHOICE"), Result::Transitioned);
    EXPECT_EQ(m.currentState(), "b_idle");
}

TEST(MachineGuards, AllGuardsFalseReturnsGuardedWithoutTransition) {
    Machine m(testfixtures::paymentModel());
    for (const char* name : {"open_enter", "requires_action_enter"})
        m.setAction(name, [](const Json&) {});
    m.setGuard("provider_authorized", [](const Json&) { return false; });
    m.start();
    EXPECT_EQ(m.dispatch("AUTHORIZE"), Result::Guarded);
    EXPECT_EQ(m.currentState(), "requires_action");
}

TEST(MachineGuards, MissingGuardThrowsWithoutStateChange) {
    Machine m(testfixtures::paymentModel());
    for (const char* name : {"open_enter", "requires_action_enter"})
        m.setAction(name, [](const Json&) {});
    m.start();
    EXPECT_THROW(m.dispatch("AUTHORIZE"), std::runtime_error);
    EXPECT_EQ(m.currentState(), "requires_action");
}

TEST(MachineGuards, ThrowingGuardDoesNotPoisonNextDispatch) {
    Machine m(testfixtures::paymentModel());
    for (const char* name : {
        "open_enter", "requires_action_enter", "authorized_enter", "record_authorization"
    }) m.setAction(name, [](const Json&) {});
    m.setGuard("provider_authorized", [](const Json&) -> bool {
        throw std::runtime_error("provider unavailable");
    });
    m.start();
    EXPECT_THROW(m.dispatch("AUTHORIZE"), std::runtime_error);
    EXPECT_EQ(m.currentState(), "requires_action");
    m.setGuard("provider_authorized", [](const Json&) { return true; });
    EXPECT_EQ(m.dispatch("AUTHORIZE"), Result::Transitioned);
    EXPECT_EQ(m.currentState(), "authorized");
}

TEST(MachineCallbacks, MissingEffectThrowsWithoutChangingState) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace, "move");
    m.start();
    EXPECT_THROW(m.dispatch("MOVE"), std::runtime_error);
    EXPECT_EQ(m.currentState(), "a_idle");
}

TEST(MachineCallbacks, ThrowingEffectCanBeReplacedAndRetried) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.setAction("move", [](const Json&) { throw std::runtime_error("side effect failed"); });
    m.start();
    EXPECT_THROW(m.dispatch("MOVE"), std::runtime_error);
    EXPECT_EQ(m.currentState(), "a_idle");
    m.setAction("move", [](const Json&) {});
    EXPECT_EQ(m.dispatch("MOVE"), Result::Transitioned);
    EXPECT_EQ(m.currentState(), "a_done");
}

TEST(MachineCallbacks, ReentrantDispatchRejectedInsideAction) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.setAction("local", [&](const Json&) {
        EXPECT_THROW(m.dispatch("MOVE"), std::logic_error);
    });
    m.start();
    EXPECT_EQ(m.dispatch("LOCAL"), Result::Handled);
}

TEST(MachineCallbacks, BusyMachineRejectsSnapshotAndRestore) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.setAction("local", [&](const Json&) {
        EXPECT_THROW(m.snapshot(), std::logic_error);
        EXPECT_THROW(m.restore({
            {"version", 1}, {"model", "nested_lifecycle"}, {"state", "a_idle"}
        }), std::logic_error);
    });
    m.start();
    EXPECT_EQ(m.dispatch("LOCAL"), Result::Handled);
}

TEST(MachineEvents, FailedChildGuardCanFallBackToParentEvent) {
    const Json spec = Json::parse(R"({
      "version":1,"name":"fallback","initial":"root",
      "states":[
        {"id":"root","initial":"child","on":[{"event":"GO","target":"other"}]},
        {"id":"child","parent":"root","on":[{"event":"GO","guard":"deny","target":"other"}]},
        {"id":"other"}
      ]})");
    Machine m(Model::Parse(spec));
    m.setGuard("deny", [](const Json&) { return false; });
    m.start();
    EXPECT_EQ(m.dispatch("GO"), Result::Transitioned);
    EXPECT_EQ(m.currentState(), "other");
}

TEST(MachineSnapshots, FreshSnapshotContainsModelVersionAndEmptyState) {
    Machine m(Model::Parse(testfixtures::toggleModel()));
    const Json snap = m.snapshot();
    EXPECT_EQ(snap.at("model"), "toggle");
    EXPECT_EQ(snap.at("version"), 1);
    EXPECT_EQ(snap.at("state"), "");
}

TEST(MachineSnapshots, RestoreDoesNotReplayEntryActions) {
    Machine m(testfixtures::hierarchyModel());
    std::vector<std::string> trace;
    registerActions(m, trace);
    m.start();
    m.dispatch("MOVE");
    const Json snap = m.snapshot();
    Machine restored(testfixtures::hierarchyModel());
    registerActions(restored, trace);
    trace.clear();
    restored.restore(snap);
    EXPECT_EQ(restored.currentState(), "a_done");
    EXPECT_TRUE(trace.empty());
}

TEST(MachineSnapshots, EmptySnapshotCanBeRestoredAndStarted) {
    const auto model = Model::Parse(testfixtures::toggleModel());
    Machine original(model), restored(model);
    restored.restore(original.snapshot());
    restored.start();
    EXPECT_EQ(restored.currentState(), "off");
}

TEST(MachineSnapshots, WrongModelRejectedWithoutMutation) {
    Machine m(Model::Parse(testfixtures::toggleModel()));
    m.start();
    EXPECT_THROW(m.restore({{"version", 1}, {"model", "different"}, {"state", "on"}}), std::invalid_argument);
    EXPECT_EQ(m.currentState(), "off");
}

TEST(MachineSnapshots, UnsupportedVersionRejectedWithoutMutation) {
    Machine m(Model::Parse(testfixtures::toggleModel()));
    m.start();
    EXPECT_THROW(m.restore({{"version", 99}, {"model", "toggle"}, {"state", "on"}}), std::invalid_argument);
    EXPECT_EQ(m.currentState(), "off");
}

TEST(MachineSnapshots, UnknownStateRejectedWithoutMutation) {
    Machine m(Model::Parse(testfixtures::toggleModel()));
    m.start();
    EXPECT_THROW(m.restore({{"version", 1}, {"model", "toggle"}, {"state", "missing"}}), std::out_of_range);
    EXPECT_EQ(m.currentState(), "off");
}

TEST(MachineSnapshots, MissingOrWrongStateFieldRejected) {
    Machine m(Model::Parse(testfixtures::toggleModel()));
    EXPECT_THROW(m.restore({{"version", 1}, {"model", "toggle"}}), std::exception);
    EXPECT_THROW(m.restore({{"version", 1}, {"model", "toggle"}, {"state", 123}}), std::exception);
}

TEST(MachineIsolation, SharedModelKeepsIndependentStatePerInstance) {
    const auto model = Model::Parse(testfixtures::toggleModel());
    Machine a(model), b(model);
    a.start();
    b.start();
    EXPECT_EQ(a.dispatch("TOGGLE"), Result::Transitioned);
    EXPECT_EQ(a.currentState(), "on");
    EXPECT_EQ(b.currentState(), "off");
    EXPECT_EQ(b.dispatch("TOGGLE"), Result::Transitioned);
    EXPECT_EQ(b.currentState(), "on");
    EXPECT_EQ(a.dispatch("TOGGLE"), Result::Transitioned);
    EXPECT_EQ(a.currentState(), "off");
}
} // namespace
