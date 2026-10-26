#pragma once

#include "nettrace/model/error.hpp"
#include <variant>
#include <stdexcept>
#include <utility>

namespace nettrace {

template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)) {}
    Result(Error error) : data_(std::move(error)) {}

    bool isOk() const { return std::holds_alternative<T>(data_); }
    bool isErr() const { return std::holds_alternative<Error>(data_); }
    explicit operator bool() const { return isOk(); }

    const T& value() const {
        if (!isOk()) {
            throw std::runtime_error("Attempted to access value of an Err Result: " + error().message);
        }
        return std::get<T>(data_);
    }

    T& value() {
        if (!isOk()) {
            throw std::runtime_error("Attempted to access value of an Err Result: " + error().message);
        }
        return std::get<T>(data_);
    }

    const Error& error() const {
        if (!isErr()) {
            throw std::runtime_error("Attempted to access error of an Ok Result");
        }
        return std::get<Error>(data_);
    }

    static Result<T> ok(T val) { return Result<T>(std::move(val)); }
    static Result<T> err(ErrorCode code, std::string msg, std::string path = "") {
        return Result<T>(Error(code, std::move(msg), std::move(path)));
    }
    static Result<T> err(Error e) { return Result<T>(std::move(e)); }

private:
    std::variant<T, Error> data_;
};

template <>
class Result<void> {
public:
    Result() : hasError_(false) {}
    Result(Error error) : hasError_(true), error_(std::move(error)) {}

    bool isOk() const { return !hasError_; }
    bool isErr() const { return hasError_; }
    explicit operator bool() const { return isOk(); }

    const Error& error() const {
        if (!isErr()) {
            throw std::runtime_error("Attempted to access error of an Ok Result");
        }
        return error_;
    }

    static Result<void> ok() { return Result<void>(); }
    static Result<void> err(ErrorCode code, std::string msg, std::string path = "") {
        return Result<void>(Error(code, std::move(msg), std::move(path)));
    }
    static Result<void> err(Error e) { return Result<void>(std::move(e)); }

private:
    bool hasError_ = false;
    Error error_;
};

} // namespace nettrace

