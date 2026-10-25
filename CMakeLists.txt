cmake_minimum_required(VERSION 3.16)

project(Calculator VERSION 1.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
# 单元测试模块缺失时不影响主程序构建
find_package(Qt${QT_VERSION_MAJOR} QUIET COMPONENTS Test)

set(PROJECT_SOURCES
        main.cpp
        calculate.cpp
        calculate.h
        calculate.ui
        calculatorengine.cpp
        calculatorengine.h
        historypanel.cpp
        historypanel.h
        keymapping.cpp
        keymapping.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Calculator
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(Calculator
        ${PROJECT_SOURCES}
    )
endif()

target_link_libraries(Calculator PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

set_target_properties(Calculator PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# ---------------------------------------------------------------------------
# 单元测试：只编译纯逻辑部分，不依赖界面，便于快速回归
# ---------------------------------------------------------------------------
if(Qt${QT_VERSION_MAJOR}Test_FOUND)
    enable_testing()

    add_executable(tst_calculatorengine
        tests/tst_calculatorengine.cpp
        calculatorengine.cpp
        calculatorengine.h
        keymapping.cpp
        keymapping.h
    )
    target_include_directories(tst_calculatorengine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(tst_calculatorengine PRIVATE Qt${QT_VERSION_MAJOR}::Test)

    add_test(NAME tst_calculatorengine COMMAND tst_calculatorengine)
else()
    message(STATUS "未找到 Qt Test 模块，跳过单元测试目标")
endif()

include(GNUInstallDirs)
install(TARGETS Calculator
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Calculator)
endif()
