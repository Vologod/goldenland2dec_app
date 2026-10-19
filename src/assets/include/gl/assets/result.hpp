#pragma once
#include <string>
#include <utility>
#include <variant>
namespace gl {
enum class ErrorCode { io, format, argument, unsupported };
struct Error { ErrorCode code; std::string message; };
template <typename T>
class Result {
public:
    Result(T value) : storage_(std::move(value)) {}
    Result(Error error) : storage_(std::move(error)) {}
    bool has_value() const { return storage_.index() == 0; }
    explicit operator bool() const { return has_value(); }
    T& value() { return std::get<0>(storage_); }
    const T& value() const { return std::get<0>(storage_); }
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }
    const Error& error() const { return std::get<1>(storage_); }
private:
    std::variant<T, Error> storage_;
};
using VoidResult = Result<std::monostate>;
}
