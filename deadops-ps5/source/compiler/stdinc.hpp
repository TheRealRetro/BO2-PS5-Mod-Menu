#pragma once

#include <algorithm>
#include <array>
#include <cstdio>
#include <deque>
#include <functional>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std::literals;

namespace xsk
{
using usize = std::size_t;
using u8 = std::uint8_t;
using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using i8 = std::int8_t;
using i16 = std::int16_t;
using i32 = std::int32_t;
using i64 = std::int64_t;
using f32 = float;
using f64 = double;

template <typename C, typename K>
inline auto contains(C const& c, K const& k) -> bool
{
    return c.find(k) != c.end();
}

inline auto starts_with(std::string_view s, std::string_view p) -> bool
{
    return s.size() >= p.size() && s.compare(0, p.size(), p) == 0;
}

inline auto ends_with(std::string_view s, std::string_view p) -> bool
{
    return s.size() >= p.size() && s.compare(s.size() - p.size(), p.size(), p) == 0;
}

inline auto hex_upper(u64 v) -> std::string
{
    char buf[17];
    std::snprintf(buf, sizeof(buf), "%llX", static_cast<unsigned long long>(v));
    return std::string{ buf };
}
}
