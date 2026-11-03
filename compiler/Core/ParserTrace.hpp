#pragma once

#include <exception>
#include <memory>
#include <string>
#include <stdexcept>
#include <type_traits>
#include <vector>
#include "AstBaseForward.hpp"

// The parser records facts; JSON formatting lives in Utils/FrontendJson.cpp.
std::string captureAstNode(const ASTNodePtr& node);

struct ParserTraceEvent {
    std::string kind;
    size_t tokenIndex = 0;
    int minimumBindingPower = 0;
    int bindingPower = 0;
    int callId = -1;
    int parentCallId = -1;
    int depth = 0;
    std::string rule;
    std::string nodeJson;
};

class ParserTrace {
    std::vector<ParserTraceEvent>* events;
    std::vector<ParserTraceEvent> calls;
    int nextCall = 0;
    int nextNode = 0;
    static constexpr size_t maxEvents = 50000;
public:
    explicit ParserTrace(std::vector<ParserTraceEvent>* events) : events(events) {}
    bool enabled() const { return events != nullptr; }

    void emit(std::string kind, size_t token, int minimum = 0, int power = 0, std::string node = {}) {
        if (!events) return;
        if (events->size() >= maxEvents) throw std::runtime_error("Parser recording exceeds 50000 events; use a smaller program.");
        ParserTraceEvent event;
        if (!calls.empty()) event = calls.back();
        event.kind = std::move(kind);
        event.tokenIndex = token;
        event.minimumBindingPower = minimum;
        event.bindingPower = power;
        event.nodeJson = std::move(node);
        events->push_back(std::move(event));
    }

    class Frame {
        ParserTrace* owner;
        size_t& cursor;
        int exceptions;
    public:
        Frame(ParserTrace& trace, const char* rule, size_t& cursor, int minimum)
            : owner(trace.enabled() ? &trace : nullptr), cursor(cursor), exceptions(std::uncaught_exceptions()) {
            if (!owner) return;
            if (owner->calls.size() >= 128) throw std::runtime_error("Parser nesting exceeds 128 calls.");
            ParserTraceEvent call;
            call.rule = rule;
            call.callId = owner->nextCall++;
            call.parentCallId = owner->calls.empty() ? -1 : owner->calls.back().callId;
            call.depth = static_cast<int>(owner->calls.size());
            owner->calls.push_back(call);
            owner->emit("enter", cursor, minimum);
        }
        Frame(const Frame&) = delete;
        ~Frame() noexcept {
            if (!owner) return;
            try { owner->emit(std::uncaught_exceptions() > exceptions ? "errorExit" : "exit", cursor); } catch (...) {}
            owner->calls.pop_back();
        }
    };

    Frame scope(const char* rule, size_t& cursor, int minimum = 0) { return Frame(*this, rule, cursor, minimum); }

    template<typename T> void created(const std::shared_ptr<T>& node, size_t token) {
        if constexpr (std::is_base_of_v<ASTNode, T>) {
            if (!events) return;
            node->traceId = nextNode++;
            emit("createNode", token, 0, 0, captureAstNode(node));
        }
    }
};
