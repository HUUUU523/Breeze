cmake_minimum_required(VERSION 3.19)

project(Breeze VERSION 2.3.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# MSVC 下源码使用 UTF-8
if(MSVC)
    add_compile_options(/utf-8)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets WebEngineWidgets PrintSupport Network)

qt_standard_project_setup()

# 把版本号注入为编译宏
add_compile_definitions(
    BREEZE_VERSION="${PROJECT_VERSION}"
    BREEZE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    BREEZE_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    BREEZE_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

qt_add_executable(Breeze WIN32
    app.rc
    resources.qrc
    main.cpp
    browserwindow.h
    browserwindow.cpp
    webview.h
    webview.cpp
    adblocker.h
    adblocker.cpp
    downloadmanager.h
    downloadmanager.cpp
    historymanager.h
    historymanager.cpp
    settingsdialog.h
    settingsdialog.cpp
    bookmarkmanager.h
    bookmarkmanager.cpp
    syncmanager.h
    syncmanager.cpp
    syncdialog.h
    syncdialog.cpp
    userscriptmanager.h
    userscriptmanager.cpp
    translator.h
    translator.cpp
    toolbox.h
    toolbox.cpp
    qrcodegen.h
    qrcodegen.cpp
    aimanager.h
    aimanager.cpp
    aidialog.h
    aidialog.cpp
    updatemanager.h
    updatemanager.cpp
    logger.h
    logger.cpp
    syncmerge.h
    syncmerge.cpp
    extension.h
    extension.cpp
    extensiondialog.h
    extensiondialog.cpp
    bookmarksidebar.h
    bookmarksidebar.cpp
    cookiemanagerdialog.h
    cookiemanagerdialog.cpp
    aisidebar.h
    aisidebar.cpp
)

target_link_libraries(Breeze PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::WebEngineWidgets
    Qt6::PrintSupport
    Qt6::Network
)

# ---- 测试 ----
option(BREEZE_BUILD_TESTS "Build unit tests" ON)
if(BREEZE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
