#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <functional>
#include <variant>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <span>
#include <queue>
#include <deque>
#include <bitset>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <fstream>
#include <filesystem>

#include <cstdarg>

#include <chrono>
#include <thread>

#include "core/platform/platform.h"

#if defined(ENG_OS_WINDOWS)
    #include "core/platform/native/win32/win32_includes.h"
#endif