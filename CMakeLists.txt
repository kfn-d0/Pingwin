cmake_minimum_required(VERSION 3.20)
project(Pingwin LANGUAGES CXX RC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(Pingwin WIN32
    main.cpp
    gui_manager.cpp
    network_service.cpp
    ui_windows.cpp
    resource.rc
)

target_link_libraries(Pingwin PRIVATE
    ws2_32
    winhttp
    iphlpapi
    wlanapi
    comctl32
    gdi32
    user32
    shell32
)

# Static linking for standalone distribution (matches README build flags)
if(MINGW)
    target_link_options(Pingwin PRIVATE -static -static-libgcc -static-libstdc++)
endif()
