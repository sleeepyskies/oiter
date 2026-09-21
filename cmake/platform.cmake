# detect OS and set defines
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "Detected Linux environment.")
    set(OITER_LINUX ON)
    target_compile_definitions(oiter PUBLIC OITER_LINUX)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(STATUS "Detected Apple environment.")
    set(OITER_MACOS ON)
    target_compile_definitions(oiter PUBLIC
        OITER_MACOS
        IMGUI_IMPL_METAL_CPP
    )
    enable_language(OBJCXX)
    set_target_properties(oiter PROPERTIES
        OBJCXX_STANDARD 23
        OBJCXX_STANDARD_REQUIRED ON
        OBJCXX_EXTENSIONS OFF
    )
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Detected Windows environment.")
    set(OITER_WINDOWS ON)
    target_compile_definitions(oiter PUBLIC OITER_WINDOWS)
else()
    message(FATAL_ERROR, "Unsupported environment, aborting build.")
endif()
