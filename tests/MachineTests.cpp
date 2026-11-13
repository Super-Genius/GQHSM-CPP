#include "gqhsm/Machine.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace gqhsm;
    auto model = Model::Parse(Json::parse(R"({
      "version":1,"name":"payment","initial":"open",
      "states":[
        {"id":"open","initial":"authorized"},
        {"id":"authorized","parent":"open","entry":["enter_authorized"],"exit":["exit_authorized"],
          "on":[{"event":"CAPTURE","target":"captured","guard":"can_capture","actions":["capture"]}]},
        {"id":"captured","parent":"open","entry":["enter_captured"]},
        {"id":"refunded",
          "on":[{"event":"REOPEN","target":"authorized"}]},
        {"id":"done","on":[]},
        {"id":"terminal","on":[]}
      ]
    })"));
    Machine machine(model);
    std::vector<std::string> trace;
    machine.setAction("enter_authorized", [&](const Json&){ trace.push_back("enter_authorized"); });
    machine.setAction("exit_authorized", [&](const Json&){ trace.push_back("exit_authorized"); });
    machine.setAction("capture", [&](const Json&){ trace.push_back("capture"); });
    machine.setAction("enter_captured", [&](const Json&){ trace.push_back("enter_captured"); });
    machine.setGuard("can_capture", [&](const Json& payload){ return payload.value("allowed",false); });
    machine.start();
    assert(machine.currentState() == "authorized");
    assert(machine.dispatch("CAPTURE", {{"allowed", false}}) == Result::Guarded);
    assert(machine.currentState() == "authorized");
    assert(machine.dispatch("CAPTURE", {{"allowed", true}}) == Result::Transitioned);
    assert(machine.currentState() == "captured");
    assert((trace == std::vector<std::string>{"enter_authorized", "exit_authorized", "capture", "enter_captured"}));
    const auto saved = machine.snapshot();
    Machine restored(model);
    restored.restore(saved);
    assert(restored.currentState() == "captured");
    assert(restored.dispatch("NOT_AN_EVENT") == Result::Unhandled);
    try {
        Model::Parse(Json::parse(R"({"version":1,"name":"bad","initial":"a","states":[{"id":"a","parent":"b"},{"id":"b","parent":"a"}]})"));
        assert(false);
    } catch (const std::invalid_argument&) {}
    std::cout << "GQHSM-CPP smoke tests passed\n";
}
