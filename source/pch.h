#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <array>
#include <vector>
#include <span>
#include <queue>
#include <deque>
#include <functional>
#include <fstream>
#include <filesystem>

#include <chrono>
#include <thread>

#include "platform/platform.h"

#if defined(ENG_OS_WINDOWS)
    #include "platform/win32/win32_headers.h"
#endif