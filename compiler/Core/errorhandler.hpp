#pragma once
#include <string>
#include <exception>

class KMYParseError : public std::exception {
public:
    // KMYParseError(const std::string& message) : msg(message) {}
    KMYParseError(
        const std::string& message,
        int line_, 
        int start_, 
        int end_
    ) : line_(line_), start_(start_), end_(end_) {
        formattedMsg += message;
    }

    int line() const { return line_; }
    int start() const { return start_; }
    int end() const { return end_; }

    const char* what() const noexcept override {
        return formattedMsg.c_str();
    }

private:
    std::string formattedMsg;
    int line_;
    int start_;
    int end_;
};

class KMYLexError : public std::exception {

public:
    KMYLexError(const std::string& message) : msg(message) {}
    const char* what() const noexcept override {
        return msg.c_str();
    }

private:
    std::string msg;
};


class KMYCompileError : public std::exception {

public:
    KMYCompileError(const std::string& message) : msg(message) {}
    const char* what() const noexcept override {
        return msg.c_str();
    }

private:
    std::string msg;
};