#pragma once

#include <QByteArray>
#include <QJsonDocument>
#include <QString>

#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

namespace cvat {

enum class ErrorKind {
    Configuration,
    Network,
    Timeout,
    Http,
    Parse,
    Cancelled,
    Unauthorized,
    Forbidden,
    NotFound,
    Server,
};

struct ApiError {
    ErrorKind kind = ErrorKind::Http;
    int status = 0;
    QString message;
    QByteArray body;
    QJsonDocument details;

    ApiError() = default;

    ApiError(ErrorKind kind_, QString message_, int status_ = 0)
        : kind(kind_), status(status_), message(std::move(message_))
    {
    }

    [[nodiscard]] bool isRetryable() const
    {
        return kind == ErrorKind::Network || kind == ErrorKind::Timeout ||
               status == 429 || (status >= 500 && status <= 599);
    }

    [[nodiscard]] QString toString() const;
};

template <typename T>
class Result {
public:
    Result(T value) : data_(std::move(value)) {}
    Result(ApiError error) : data_(std::move(error)) {}

    [[nodiscard]] bool hasValue() const { return std::holds_alternative<T>(data_); }
    explicit operator bool() const { return hasValue(); }

    T& value()
    {
        if (!hasValue()) {
            throw std::logic_error("Result::value() called on an error");
        }
        return std::get<T>(data_);
    }

    const T& value() const
    {
        if (!hasValue()) {
            throw std::logic_error("Result::value() called on an error");
        }
        return std::get<T>(data_);
    }

    T& operator*() { return value(); }
    const T& operator*() const { return value(); }
    T* operator->() { return &value(); }
    const T* operator->() const { return &value(); }

    [[nodiscard]] const ApiError& error() const
    {
        if (hasValue()) {
            throw std::logic_error("Result::error() called on a value");
        }
        return std::get<ApiError>(data_);
    }

    [[nodiscard]] ApiError& error()
    {
        if (hasValue()) {
            throw std::logic_error("Result::error() called on a value");
        }
        return std::get<ApiError>(data_);
    }

private:
    std::variant<T, ApiError> data_;
};

struct Unit {
    bool operator==(const Unit&) const = default;
};

using VoidResult = Result<Unit>;

} // namespace cvat
