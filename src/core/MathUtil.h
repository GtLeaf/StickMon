#pragma once

#include <type_traits>

namespace MathUtil {

template <typename A, typename B>
inline typename std::common_type<A, B>::type min(A first, B second) {
    using Result = typename std::common_type<A, B>::type;
    return static_cast<Result>(second) < static_cast<Result>(first)
        ? static_cast<Result>(second)
        : static_cast<Result>(first);
}

template <typename A, typename B>
inline typename std::common_type<A, B>::type max(A first, B second) {
    using Result = typename std::common_type<A, B>::type;
    return static_cast<Result>(first) < static_cast<Result>(second)
        ? static_cast<Result>(second)
        : static_cast<Result>(first);
}

template <typename V, typename L, typename H>
inline typename std::common_type<V, L, H>::type clamp(
    V value, L lower, H upper) {
    using Result = typename std::common_type<V, L, H>::type;
    Result result = static_cast<Result>(value);
    Result low = static_cast<Result>(lower);
    Result high = static_cast<Result>(upper);
    return result < low ? low : (high < result ? high : result);
}

}  // namespace MathUtil
