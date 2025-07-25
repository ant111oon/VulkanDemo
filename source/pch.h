#pragma once

#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <span>
#include <array>
#include <functional>
#include <deque>
#include <fstream>
#include <filesystem>

#include <chrono>
#include <thread>

#include "platform/platform.h"

#if defined(ENG_OS_WINDOWS)
    #include "platform/win32_headers.h"
#endif