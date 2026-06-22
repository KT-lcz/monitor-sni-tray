cmake_minimum_required(VERSION 3.10)

project(monitor-dock-plugin LANGUAGES CXX)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 QUIET COMPONENTS Core Gui Widgets)
if(Qt6_FOUND)
    set(QT_VERSION_MAJOR 6)
else()
    find_package(Qt5 REQUIRED COMPONENTS Core Gui Widgets)
    set(QT_VERSION_MAJOR 5)
endif()
find_package(PkgConfig REQUIRED)

pkg_check_modules(DdeDockInterface REQUIRED dde-dock)

add_definitions(-DQT_PLUGIN)

add_library(${PROJECT_NAME} SHARED
    src/monitorplugin.cpp
    src/monitorplugin.h
    src/monitorwidget.cpp
    src/monitorwidget.h
    src/quickpanelwidget.cpp
    src/quickpanelwidget.h
    src/systemstats.cpp
    src/systemstats.h
    src/tipswidget.cpp
    src/tipswidget.h
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${DdeDockInterface_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib/dde-dock/plugins)

option(BUILD_STATS_SMOKE_TEST "Build a small /proc stats smoke test executable" ON)
if(BUILD_STATS_SMOKE_TEST)
    add_executable(stats-smoke-test
        tests/stats_smoke_test.cpp
        src/systemstats.cpp
        src/systemstats.h
    )
    target_link_libraries(stats-smoke-test PRIVATE Qt${QT_VERSION_MAJOR}::Core)
endif()

option(BUILD_WIDGETS_SMOKE_TEST "Build a small widget rendering smoke test executable" ON)
if(BUILD_WIDGETS_SMOKE_TEST)
    add_executable(widgets-smoke-test
        tests/widgets_smoke_test.cpp
        src/monitorwidget.cpp
        src/monitorwidget.h
        src/quickpanelwidget.cpp
        src/quickpanelwidget.h
        src/systemstats.cpp
        src/systemstats.h
        src/tipswidget.cpp
        src/tipswidget.h
    )
    target_link_libraries(widgets-smoke-test PRIVATE
        Qt${QT_VERSION_MAJOR}::Core
        Qt${QT_VERSION_MAJOR}::Gui
        Qt${QT_VERSION_MAJOR}::Widgets
    )
endif()
