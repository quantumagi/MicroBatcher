#pragma once

#include <optional>
#include <stdexcept>

namespace MicroBatcher {

    template<typename T>
    class JobResult {
    public:
        T result;
        /*
        std::optional<std::string> error; // Use std::string to store the error message

        JobResult(T result, std::optional<std::string> error = std::nullopt)
            : result(result), error(error) {}

        bool hasError() const {
            return error.has_value();
        }
        */
    };

} // namespace MicroBatcher

