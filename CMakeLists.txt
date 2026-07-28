cmake_minimum_required(VERSION 3.10)

project(monitor-sni-tray LANGUAGES CXX)

include(GNUInstallDirs)

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
find_package(Threads REQUIRED)
pkg_check_modules(DBus REQUIRED IMPORTED_TARGET dbus-1)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/trayapplication.cpp
    src/trayapplication.h
    src/cpuhwmon.cpp
    src/cpuhwmon.h
    src/gpuhwmon.cpp
    src/gpuhwmon.h
    src/systemstats.cpp
    src/systemstats.h
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    MONITOR_POWER_HELPER_PATH="${CMAKE_INSTALL_FULL_LIBEXECDIR}/monitor-power-helper"
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    PkgConfig::DBus
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

add_executable(monitor-power-helper
    src/powerhelper.cpp
    src/raplreader.cpp
    src/raplreader.h
)
target_link_libraries(monitor-power-helper PRIVATE Threads::Threads)
install(TARGETS monitor-power-helper RUNTIME DESTINATION ${CMAKE_INSTALL_LIBEXECDIR})

install(FILES assets/monitor-sni-tray.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES assets/monitor-sni-tray.desktop DESTINATION /etc/xdg/autostart)

option(BUILD_STATS_SMOKE_TEST "Build a small /proc stats smoke test executable" ON)
if(BUILD_STATS_SMOKE_TEST)
    add_executable(stats-smoke-test
        tests/stats_smoke_test.cpp
        src/cpuhwmon.cpp
        src/cpuhwmon.h
        src/gpuhwmon.cpp
        src/gpuhwmon.h
        src/raplreader.cpp
        src/raplreader.h
        src/systemstats.cpp
        src/systemstats.h
    )
    target_link_libraries(stats-smoke-test PRIVATE Qt${QT_VERSION_MAJOR}::Core)
endif()

option(BUILD_WIDGETS_SMOKE_TEST "Build the legacy widget rendering smoke test" ON)
if(BUILD_WIDGETS_SMOKE_TEST)
    add_executable(widgets-smoke-test
        tests/widgets_smoke_test.cpp
        src/monitorwidget.cpp
        src/monitorwidget.h
        src/quickpanelwidget.cpp
        src/quickpanelwidget.h
        src/cpuhwmon.cpp
        src/cpuhwmon.h
        src/gpuhwmon.cpp
        src/gpuhwmon.h
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
