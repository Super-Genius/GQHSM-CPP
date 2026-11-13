// Copyright 2026 Genius Ventures, Inc.
// GoogleTest model-contract tests, both accepted and rejected models.
#include "TestFixtures.hpp"

#include <gtest/gtest.h>

#include <stdexcept>

namespace {
using gqhsm::Json;
using gqhsm::Model;

TEST(ModelValidation, ValidPaymentFixtureLoads) {
    const auto model = testfixtures::paymentModel();
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->name(), "payment_workflow");
    EXPECT_EQ(model->initial(), "open");
    EXPECT_EQ(model->at("captured").parent, "open");
}

TEST(ModelValidation, NestedInitialChildrenAndActionsLoad) {
    const auto model = testfixtures::hierarchyModel();
    EXPECT_EQ(model->at("root").initial, "branch_a");
    EXPECT_EQ(model->at("branch_a").initial, "a_idle");
    EXPECT_EQ(model->at("branch_b").initial, "b_idle");
    EXPECT_EQ(model->at("a_idle").transitions.size(), 4U);
}

TEST(ModelValidation, FlatModelWithoutOptionalFieldsLoads) {
    const auto model = Model::Parse(testfixtures::toggleModel());
    EXPECT_TRUE(model->at("off").entry.empty());
    EXPECT_TRUE(model->at("on").exit.empty());
    EXPECT_TRUE(model->at("off").parent.empty());
}

TEST(ModelValidation, AcceptsMultiplePrioritizedGuardAlternatives) {
    const auto model = testfixtures::hierarchyModel();
    EXPECT_EQ(model->at("a_idle").transitions[2].priority, 10);
    EXPECT_EQ(model->at("a_idle").transitions[3].priority, 5);
}

TEST(ModelValidation, AcceptsActionOnlyInternalEvents) {
    const auto model = testfixtures::hierarchyModel();
    const auto& event = model->at("a_idle").transitions[0];
    EXPECT_EQ(event.event, "LOCAL");
    EXPECT_TRUE(event.target.empty());
    ASSERT_EQ(event.actions.size(), 1U);
    EXPECT_EQ(event.actions.front(), "local");
}

TEST(ModelValidation, UnknownStateLookupThrows) {
    const auto model = testfixtures::paymentModel();
    EXPECT_THROW(model->at("missing"), std::out_of_range);
}

TEST(ModelValidation, RejectsMalformedJsonSyntax) {
    EXPECT_THROW(Json::parse(R"({"version":1,"name":"bad",)"), Json::parse_error);
}

TEST(ModelValidation, RejectsUnsupportedVersion) {
    auto value = testfixtures::toggleModel();
    value["version"] = 2;
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsMissingVersion) {
    auto value = testfixtures::toggleModel();
    value.erase("version");
    EXPECT_THROW(Model::Parse(value), std::exception);
}

TEST(ModelValidation, RejectsMissingName) {
    auto value = testfixtures::toggleModel();
    value.erase("name");
    EXPECT_THROW(Model::Parse(value), std::exception);
}

TEST(ModelValidation, RejectsEmptyName) {
    auto value = testfixtures::toggleModel();
    value["name"] = "";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsMissingInitial) {
    auto value = testfixtures::toggleModel();
    value.erase("initial");
    EXPECT_THROW(Model::Parse(value), std::exception);
}

TEST(ModelValidation, RejectsUnknownInitial) {
    auto value = testfixtures::toggleModel();
    value["initial"] = "does-not-exist";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsEmptyStates) {
    auto value = testfixtures::toggleModel();
    value["states"] = Json::array();
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsMissingStateId) {
    auto value = testfixtures::toggleModel();
    value["states"][0].erase("id");
    EXPECT_THROW(Model::Parse(value), std::exception);
}

TEST(ModelValidation, RejectsEmptyStateId) {
    auto value = testfixtures::toggleModel();
    value["states"][0]["id"] = "";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsDuplicateStateId) {
    auto value = testfixtures::toggleModel();
    value["states"].push_back(value["states"][0]);
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsUnknownParent) {
    auto value = testfixtures::toggleModel();
    value["states"][0]["parent"] = "missing";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsParentCycle) {
    auto value = testfixtures::toggleModel();
    value["states"][0]["parent"] = "on";
    value["states"][1]["parent"] = "off";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsUnknownInitialChild) {
    auto value = testfixtures::load("hierarchy.json");
    value["states"][0]["initial"] = "missing";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsNonDirectInitialChild) {
    auto value = testfixtures::load("hierarchy.json");
    value["states"][0]["initial"] = "a_idle";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsUnknownTransitionTarget) {
    auto value = testfixtures::toggleModel();
    value["states"][0]["on"][0]["target"] = "missing";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsMissingTransitionEvent) {
    auto value = testfixtures::toggleModel();
    value["states"][0]["on"][0].erase("event");
    EXPECT_THROW(Model::Parse(value), std::exception);
}

TEST(ModelValidation, RejectsEmptyTransitionEvent) {
    auto value = testfixtures::toggleModel();
    value["states"][0]["on"][0]["event"] = "";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsDuplicateEventPriority) {
    auto value = testfixtures::toggleModel();
    value["states"][0]["on"].push_back({{"event", "TOGGLE"}, {"target", "on"}});
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsNonArrayEntryActions) {
    auto value = testfixtures::load("hierarchy.json");
    value["states"][0]["entry"] = "not-an-array";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsNonStringAction) {
    auto value = testfixtures::load("hierarchy.json");
    value["states"][0]["entry"][0] = 42;
    EXPECT_THROW(Model::Parse(value), std::exception);
}

TEST(ModelValidation, RejectsNonArrayTransitionActions) {
    auto value = testfixtures::load("payment.json");
    value["states"][1]["on"][0]["actions"] = "record_authorization";
    EXPECT_THROW(Model::Parse(value), std::invalid_argument);
}

TEST(ModelValidation, RejectsNonnumericPriority) {
    auto value = testfixtures::toggleModel();
    value["states"][0]["on"][0]["priority"] = "high";
    EXPECT_THROW(Model::Parse(value), std::exception);
}

TEST(ModelValidation, RejectsNonObjectTopLevelModel) {
    EXPECT_THROW(Model::Parse(Json::array({"model"})), std::exception);
}
} // namespace
