// Copyright 2026 Genius Ventures, Inc.
// Seeded property and stress tests; deterministic, repeatable, no network.
#include "TestFixtures.hpp"

#include <gtest/gtest.h>

#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {
using gqhsm::Json;
using gqhsm::Machine;
using gqhsm::Model;
using gqhsm::Result;

TEST(MachineProperties, ToggleMatchesReferenceImplementationForFiveThousandEvents) {
    const auto model = Model::Parse(testfixtures::toggleModel());
    Machine machine(model);
    machine.start();
    bool referenceOn = false;
    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> choice(0, 3);
    for (int i = 0; i < 5000; ++i) {
        SCOPED_TRACE(i);
        const bool toggle = choice(rng) != 0;
        const auto result = machine.dispatch(toggle ? "TOGGLE" : "INVALID");
        if (toggle) referenceOn = !referenceOn;
        EXPECT_EQ(result, toggle ? Result::Transitioned : Result::Unhandled);
        EXPECT_EQ(machine.currentState(), referenceOn ? "on" : "off");
    }
}

TEST(MachineProperties, SameEventStreamAlwaysProducesTheSameStates) {
    const auto model = Model::Parse(testfixtures::toggleModel());
    Machine a(model), b(model);
    a.start();
    b.start();
    std::mt19937 rng(0xBADC0DEu);
    std::uniform_int_distribution<int> choice(0, 9);
    for (int i = 0; i < 2000; ++i) {
        const std::string event = choice(rng) < 6 ? "TOGGLE" : "UNKNOWN";
        SCOPED_TRACE(i);
        EXPECT_EQ(a.dispatch(event), b.dispatch(event));
        EXPECT_EQ(a.snapshot(), b.snapshot());
    }
}

TEST(MachineProperties, SnapshotJsonRoundTripMatchesUninterruptedExecution) {
    const auto model = Model::Parse(testfixtures::toggleModel());
    Machine uninterrupted(model);
    uninterrupted.start();
    std::mt19937 rng(123456u);
    std::uniform_int_distribution<int> choice(0, 1);
    for (int i = 0; i < 1000; ++i) {
        uninterrupted.dispatch(choice(rng) ? "TOGGLE" : "NOOP");
        if (i % 13 == 0) {
            const auto encoded = uninterrupted.snapshot().dump();
            Machine recovered(model);
            recovered.restore(Json::parse(encoded));
            EXPECT_EQ(recovered.currentState(), uninterrupted.currentState());
            EXPECT_EQ(recovered.snapshot(), uninterrupted.snapshot());
            const auto expected = uninterrupted.currentState() == "on" ? "off" : "on";
            EXPECT_EQ(recovered.dispatch("TOGGLE"), Result::Transitioned);
            EXPECT_EQ(recovered.currentState(), expected);
            // The original instance must not change.
            EXPECT_NE(recovered.snapshot(), uninterrupted.snapshot());
        }
    }
}

TEST(MachineProperties, HundredsOfIndependentMachineInstancesNeverShareState) {
    const auto model = Model::Parse(testfixtures::toggleModel());
    std::vector<std::unique_ptr<Machine>> machines;
    machines.reserve(256);
    for (int i = 0; i < 256; ++i) {
        machines.emplace_back(std::make_unique<Machine>(model));
        machines.back()->start();
    }
    for (int round = 0; round < 12; ++round) {
        for (std::size_t i = 0; i < machines.size(); ++i) {
            if ((i + static_cast<std::size_t>(round)) % 3 == 0)
                EXPECT_EQ(machines[i]->dispatch("TOGGLE"), Result::Transitioned);
        }
    }
    for (std::size_t i = 0; i < machines.size(); ++i) {
        int count = 0;
        for (int round = 0; round < 12; ++round)
            if ((i + static_cast<std::size_t>(round)) % 3 == 0) ++count;
        EXPECT_EQ(machines[i]->currentState(), count % 2 ? "on" : "off");
    }
}

TEST(MachineProperties, GuardDecisionsAreDeterministicForHundredsOfModels) {
    const auto model = testfixtures::hierarchyModel();
    std::mt19937 rng(0x123456u);
    std::uniform_int_distribution<int> choice(0, 1);
    for (int i = 0; i < 400; ++i) {
        SCOPED_TRACE(i);
        const bool allow = choice(rng) == 1;
        Machine machine(model);
        for (const char* name : {
            "root_enter", "a_enter", "idle_enter", "idle_exit", "a_exit",
            "done_enter", "b_enter", "b_idle_enter"
        }) machine.setAction(name, [](const Json&) {});
        machine.setGuard("can_move", [allow](const Json&) { return allow; });
        machine.start();
        EXPECT_EQ(machine.dispatch("CHOICE"), Result::Transitioned);
        EXPECT_EQ(machine.currentState(), allow ? "a_done" : "b_idle");
    }
}

TEST(MachineProperties, InvalidEventStreamsNeverChangeTerminalStates) {
    const auto model = testfixtures::paymentModel();
    Machine machine(model);
    for (const char* name : {
        "open_enter", "requires_action_enter", "record_void", "voided_enter"
    }) machine.setAction(name, [](const Json&) {});
    machine.start();
    ASSERT_EQ(machine.dispatch("VOID"), Result::Transitioned);
    const auto before = machine.snapshot();
    for (int i = 0; i < 1000; ++i) {
        SCOPED_TRACE(i);
        EXPECT_EQ(machine.dispatch(i % 2 ? "CAPTURE" : "AUTHORIZE"), Result::Unhandled);
        EXPECT_EQ(machine.snapshot(), before);
    }
}

TEST(MachineProperties, MutatedInvalidModelsAlwaysFailToParse) {
    const auto base = testfixtures::toggleModel();
    std::vector<Json> bad;
    auto add = [&](const auto& mutate) {
        auto candidate = base;
        mutate(candidate);
        bad.push_back(std::move(candidate));
    };
    add([](Json& j){ j["version"] = 999; });
    add([](Json& j){ j["initial"] = "missing"; });
    add([](Json& j){ j["states"][0]["id"] = ""; });
    add([](Json& j){ j["states"][0]["parent"] = "missing"; });
    add([](Json& j){ j["states"][0]["on"][0]["target"] = "missing"; });
    add([](Json& j){ j["states"][1]["on"][0]["event"] = ""; });
    add([](Json& j){ j["states"][0]["entry"] = "not-an-array"; });
    add([](Json& j){ j["states"][0]["on"][0]["actions"] = 9; });
    add([](Json& j){ j["states"][0]["on"][0]["priority"] = "high"; });
    add([](Json& j){ j["states"][0]["parent"] = "on"; j["states"][1]["parent"] = "off"; });
    for (std::size_t i = 0; i < bad.size(); ++i) {
        SCOPED_TRACE(i);
        EXPECT_THROW(Model::Parse(bad[i]), std::exception);
    }
}
} // namespace
