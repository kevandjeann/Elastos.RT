xdk_add_subdirectories(
    Chat
)

if("$ENV{XDK_TARGET_PLATFORM}" STREQUAL "android")
    xdk_add_subdirectories(
        Exe
    )
endif()
