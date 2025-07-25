#include <vulkan/vulkan.h>

#include "platform/window/window.h"


int main(int argc, char* argv[])
{
#if defined(ENG_OS_WINDOWS)
    Win32Window window;
#endif

    VkInstanceCreateInfo instCreateInfo = {};

    fprintf_s(stdout, "Hello %s!\n", "World");

    return 0;
}