#ifndef PINE_STD_OPTIONAL_H
#define PINE_STD_OPTIONAL_H

#include <pstd/move.h>
#include <pstd/archive.h>

namespace pstd {

struct nullopt_t {};

inline constexpr nullopt_t nullopt;

template <typename T>
struct optional {
    optional() = default;
    optional(nullopt_t){};

    template <typename U>
    optional(U&& val) {
        value_ = pstd::forward<U>(val);
        valid = true;
    }

    T& operator*() {
        return value_;
    }
    const T& operator*() const {
        return value_;
    }

    T* operator->() {
        return &value_;
    }
    const T* operator->() const {
        return &value_;
    }

    explicit operator bool() const {
        return valid;
    }

    PSTD_ARCHIVE(value_)

  private:
    T value_;
    bool valid = false;
};

}  // namespace pstd

#endif  // PINE_STD_OPTIONAL_H