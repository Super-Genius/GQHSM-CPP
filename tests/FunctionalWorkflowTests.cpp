// Copyright 2026 Genius Ventures, Inc.
// Functional tests: run the entire POS-like payment lifecycle without a provider.
#include "TestFixtures.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {
using gqhsm::Json;
using gqhsm::Machine;
using gqhsm::Model;
using gqhsm::Result;

struct PaymentHarness {
    explicit PaymentHarness(std::shared_ptr<const Model> definition = testfixtures::paymentModel())
        : machine(std::move(definition)) {
        for (const char* name : {
            "open_enter", "open_exit", "requires_action_enter", "authorized_enter",
            "captured_enter", "refunded_enter", "voided_enter", "failed_enter",
            "record_authorization", "record_capture", "record_refund",
            "record_failure", "record_void"
        }) {
            machine.setAction(name, [this, name](const Json&) { events.emplace_back(name); });
        }
        machine.setGuard("provider_authorized", [](const Json& p) {
            return p.value("provider_ok", false);
        });
        machine.setGuard("capture_allowed", [](const Json& p) {
            const auto amount = p.value("amount_minor", std::int64_t{0});
            const auto remaining = p.value("remaining_minor", std::int64_t{0});
            return amount > 0 && amount <= remaining;
        });
        machine.setGuard("refund_allowed", [](const Json& p) {
            const auto refund = p.value("refund_minor", std::int64_t{0});
            const auto captured = p.value("captured_minor", std::int64_t{0});
            return refund > 0 && refund <= captured;
        });
    }
    Machine machine;
    std::vector<std::string> events;
};

Json approved() { return {{"provider_ok", true}, {"payment_id", "pay-001"}}; }
Json captured(int amount = 500, int remaining = 500) {
    return {{"amount_minor", amount}, {"remaining_minor", remaining}, {"payment_id", "pay-001"}};
}
Json refund(int amount = 500, int capturedTotal = 500) {
    return {{"refund_minor", amount}, {"captured_minor", capturedTotal}, {"payment_id", "pay-001"}};
}

struct ScopedTempFile {
    explicit ScopedTempFile(std::string label) {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path = std::filesystem::temp_directory_path() /
            ("gqhsm_" + label + "_" + std::to_string(stamp) + ".json");
    }
    ~ScopedTempFile() {
        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }
    std::filesystem::path path;
};

TEST(PaymentWorkflow, FullAuthorizationCaptureAndRefundHappyPath) {
    PaymentHarness h;
    h.machine.start({{"payment_id", "pay-001"}});
    EXPECT_EQ(h.machine.currentState(), "requires_action");
    EXPECT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    EXPECT_EQ(h.machine.currentState(), "authorized");
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured()), Result::Transitioned);
    EXPECT_EQ(h.machine.currentState(), "captured");
    EXPECT_EQ(h.machine.dispatch("REFUND", refund()), Result::Transitioned);
    EXPECT_EQ(h.machine.currentState(), "refunded");
    EXPECT_EQ(h.events, (std::vector<std::string>{
        "open_enter", "requires_action_enter", "record_authorization",
        "authorized_enter", "record_capture", "captured_enter",
        "record_refund", "refunded_enter"
    }));
}

TEST(PaymentWorkflow, ProviderRejectionDoesNotAuthorize) {
    PaymentHarness h;
    h.machine.start();
    const auto before = h.machine.snapshot();
    EXPECT_EQ(h.machine.dispatch("AUTHORIZE", {{"provider_ok", false}}), Result::Guarded);
    EXPECT_EQ(h.machine.currentState(), "requires_action");
    EXPECT_EQ(h.machine.snapshot(), before);
    EXPECT_EQ(h.events, (std::vector<std::string>{"open_enter", "requires_action_enter"}));
}

TEST(PaymentWorkflow, ExplicitFailureIsTerminal) {
    PaymentHarness h;
    h.machine.start();
    EXPECT_EQ(h.machine.dispatch("FAIL", {{"reason", "declined"}}), Result::Transitioned);
    EXPECT_EQ(h.machine.currentState(), "failed");
    const auto count = h.events.size();
    EXPECT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Unhandled);
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured()), Result::Unhandled);
    EXPECT_EQ(h.events.size(), count);
}

TEST(PaymentWorkflow, PaymentCanBeVoidedBeforeAuthorization) {
    PaymentHarness h;
    h.machine.start();
    EXPECT_EQ(h.machine.dispatch("VOID"), Result::Transitioned);
    EXPECT_EQ(h.machine.currentState(), "voided");
    EXPECT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Unhandled);
}

TEST(PaymentWorkflow, PaymentCanBeVoidedAfterAuthorization) {
    PaymentHarness h;
    h.machine.start();
    ASSERT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    EXPECT_EQ(h.machine.dispatch("VOID"), Result::Transitioned);
    EXPECT_EQ(h.machine.currentState(), "voided");
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured()), Result::Unhandled);
}

TEST(PaymentWorkflow, OvercaptureRejectedAndValidCaptureStillWorks) {
    PaymentHarness h;
    h.machine.start();
    ASSERT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    const auto before = h.machine.snapshot();
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured(501, 500)), Result::Guarded);
    EXPECT_EQ(h.machine.snapshot(), before);
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured(500, 500)), Result::Transitioned);
    EXPECT_EQ(h.machine.currentState(), "captured");
}

TEST(PaymentWorkflow, ZeroOrNegativeCaptureRejected) {
    PaymentHarness h;
    h.machine.start();
    ASSERT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured(0, 500)), Result::Guarded);
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured(-1, 500)), Result::Guarded);
    EXPECT_EQ(h.machine.currentState(), "authorized");
}

TEST(PaymentWorkflow, DuplicateCaptureDoesNotRunActionsAgain) {
    PaymentHarness h;
    h.machine.start();
    ASSERT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    ASSERT_EQ(h.machine.dispatch("CAPTURE", captured()), Result::Transitioned);
    const auto events = h.events.size();
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured()), Result::Unhandled);
    EXPECT_EQ(h.events.size(), events);
}

TEST(PaymentWorkflow, RefundAboveCapturedAmountRejected) {
    PaymentHarness h;
    h.machine.start();
    ASSERT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    ASSERT_EQ(h.machine.dispatch("CAPTURE", captured()), Result::Transitioned);
    EXPECT_EQ(h.machine.dispatch("REFUND", refund(600, 500)), Result::Guarded);
    EXPECT_EQ(h.machine.currentState(), "captured");
    EXPECT_EQ(h.machine.dispatch("REFUND", refund(500, 500)), Result::Transitioned);
}

TEST(PaymentWorkflow, DuplicateRefundDoesNotExecuteAgain) {
    PaymentHarness h;
    h.machine.start();
    ASSERT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    ASSERT_EQ(h.machine.dispatch("CAPTURE", captured()), Result::Transitioned);
    ASSERT_EQ(h.machine.dispatch("REFUND", refund()), Result::Transitioned);
    const auto count = h.events.size();
    EXPECT_EQ(h.machine.dispatch("REFUND", refund()), Result::Unhandled);
    EXPECT_EQ(h.events.size(), count);
}

TEST(PaymentWorkflow, CaptureBeforeAuthorizationIsRejected) {
    PaymentHarness h;
    h.machine.start();
    EXPECT_EQ(h.machine.dispatch("CAPTURE", captured()), Result::Unhandled);
    EXPECT_EQ(h.machine.currentState(), "requires_action");
}

TEST(PaymentWorkflow, FailedProviderCallbackLeavesStateForRecovery) {
    PaymentHarness h;
    h.machine.setAction("record_authorization", [](const Json&) {
        throw std::runtime_error("processor temporarily unavailable");
    });
    h.machine.start();
    EXPECT_THROW(h.machine.dispatch("AUTHORIZE", approved()), std::runtime_error);
    EXPECT_EQ(h.machine.currentState(), "requires_action");
    h.machine.setAction("record_authorization", [&](const Json&) {
        h.events.emplace_back("record_authorization");
    });
    EXPECT_EQ(h.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
}

TEST(PaymentWorkflow, SnapshotSurvivesFileRoundTripAndResumes) {
    PaymentHarness first;
    first.machine.start();
    ASSERT_EQ(first.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    ScopedTempFile file("authorized");
    {
        std::ofstream out(file.path);
        ASSERT_TRUE(out.good());
        out << first.machine.snapshot().dump(2);
        ASSERT_TRUE(out.good());
    }
    PaymentHarness recovered;
    {
        std::ifstream in(file.path);
        ASSERT_TRUE(in.good());
        recovered.machine.restore(Json::parse(in));
    }
    EXPECT_TRUE(recovered.events.empty()); // restore does not trigger entry callbacks
    EXPECT_EQ(recovered.machine.currentState(), "authorized");
    EXPECT_EQ(recovered.machine.dispatch("CAPTURE", captured()), Result::Transitioned);
    EXPECT_EQ(recovered.machine.currentState(), "captured");
    EXPECT_EQ(recovered.events, (std::vector<std::string>{"record_capture", "captured_enter"}));
}

TEST(PaymentWorkflow, CorruptSnapshotFailsWithoutRestoringWrongState) {
    PaymentHarness h;
    h.machine.start();
    const auto before = h.machine.snapshot();
    auto malformed = before;
    malformed["state"] = "unknown-state";
    EXPECT_THROW(h.machine.restore(malformed), std::out_of_range);
    EXPECT_EQ(h.machine.snapshot(), before);
}

TEST(PaymentWorkflow, ConcurrentBusinessEntitiesDoNotShareState) {
    const auto definition = testfixtures::paymentModel();
    PaymentHarness p1(definition), p2(definition);
    p1.machine.start();
    p2.machine.start();
    ASSERT_EQ(p1.machine.dispatch("AUTHORIZE", approved()), Result::Transitioned);
    ASSERT_EQ(p1.machine.dispatch("CAPTURE", captured()), Result::Transitioned);
    EXPECT_EQ(p1.machine.currentState(), "captured");
    EXPECT_EQ(p2.machine.currentState(), "requires_action");
    ASSERT_EQ(p2.machine.dispatch("FAIL"), Result::Transitioned);
    EXPECT_EQ(p2.machine.currentState(), "failed");
    EXPECT_EQ(p1.machine.currentState(), "captured");
}

TEST(PaymentWorkflow, ModelIsJsonAndNotAnXmlRuntimeDependency) {
    const auto jsonText = testfixtures::load("payment.json").dump();
    EXPECT_NO_THROW(gqhsm::Model::Parse(Json::parse(jsonText)));
    EXPECT_THROW(Json::parse("<CQHStateMachine/>"), Json::parse_error);
}
} // namespace
