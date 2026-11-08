# plugins/RsWiki/CMakeLists.txt
#
# A GXS wiki service (derived from WikiPoos) as a RetroShare plugin. It runs
# its own service (own id, database, config, event type, namespace rswiki),
# so it can be used together with RS_GXSWIKIPOS=ON.
cmake_minimum_required(VERSION 3.18)
project(RsWikiPlugin)

if(NOT DEFINED QT_VERSION_MAJOR)
    find_package(Qt6 QUIET COMPONENTS Core)
    if(Qt6_FOUND)
        set(QT_VERSION_MAJOR 6)
    else()
        find_package(Qt5 COMPONENTS Core REQUIRED)
        set(QT_VERSION_MAJOR 5)
    endif()
endif()
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Widgets Network REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(RSWIKI_SOURCES
    RsWikiPlugin.cpp
    services/p3RsWiki.cc
    services/rsWikiItems.cc
    gui/WikiDialog.cpp
    gui/WikiSpaceCard.cpp
    gui/WikiCommentItem.cpp
    gui/WikiEditDialog.cpp
    gui/WikiGroupDialog.cpp
    gui/WikiModeratorChooserDialog.cpp
    gui/WikiUserNotify.cpp
)

set(RSWIKI_HEADERS
    RsWikiPlugin.h
    interface/rsWiki.h
    services/p3RsWiki.h
    services/rsWikiItems.h
    gui/WikiDialog.h
    gui/WikiStyle.h
    gui/WikiSpaceCard.h
    gui/WikiCommentItem.h
    gui/WikiEditDialog.h
    gui/WikiGroupDialog.h
    gui/WikiModeratorChooserDialog.h
    gui/WikiUserNotify.h
)

set(RSWIKI_FORMS
    gui/RsWikiDialog.ui
    gui/RsWikiSpaceCard.ui
    gui/RsWikiCommentItem.ui
    gui/RsWikiEditDialog.ui
    gui/RsWikiModeratorsWidget.ui
    gui/RsWikiModeratorChooserDialog.ui
)

set(RSWIKI_RESOURCES
    gui/RsWiki_images.qrc
)

add_library(RsWiki MODULE
    ${RSWIKI_SOURCES}
    ${RSWIKI_HEADERS}
    ${RSWIKI_FORMS}
    ${RSWIKI_RESOURCES}
)

set_target_properties(RsWiki PROPERTIES
    PREFIX "lib"
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/plugins"
    AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/gui"
)

# Resolve the RetroShare super-project root so this file works both from the
# super-project (add_subdirectory) and standalone (cmake run in this dir),
# as long as the plugin lives in <root>/plugins/RsWiki.
get_filename_component(RS_SUPERPROJECT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
set(RS_BUILD_DIR "${RS_SUPERPROJECT_ROOT}/Build-cmake" CACHE PATH
    "RetroShare build directory to link against when building standalone")

# WikiGroupDialog derives from GxsGroupDialog, whose header includes
# ui_GxsGroupDialog.h. retroshare-gui generates it with qt_wrap_ui into its
# binary dir, so that dir has to be on the include path.
if(TARGET retroshare-gui)
    get_target_property(RS_GUI_BINARY_DIR retroshare-gui BINARY_DIR)
else()
    set(RS_GUI_BINARY_DIR "${RS_BUILD_DIR}/retroshare-gui")
endif()

if(WIN32)
    target_compile_definitions(RsWiki PRIVATE WINDOWS_SYS)

    # A Windows DLL has to resolve every symbol at link time, so the plugin
    # links against RetroShare itself: the CMake targets in the super-project,
    # or the import libraries of a RetroShare build tree when standalone.
    if(TARGET retroshare AND TARGET retroshare-gui)
        target_link_libraries(RsWiki PRIVATE retroshare retroshare-gui)
    else()
        find_library(RS_LIBRETROSHARE_IMPLIB NAMES retroshare
                     HINTS "${RS_BUILD_DIR}/libretroshare" NO_DEFAULT_PATH)
        find_library(RS_GUI_IMPLIB NAMES retroshare-gui
                     HINTS "${RS_BUILD_DIR}/retroshare-gui" NO_DEFAULT_PATH)

        if(NOT RS_LIBRETROSHARE_IMPLIB OR NOT RS_GUI_IMPLIB)
            message(FATAL_ERROR
                "Standalone Windows build needs the import libraries of a built "
                "RetroShare. Looked in ${RS_BUILD_DIR} for libretroshare.dll.a and "
                "libretroshare-gui.dll.a. Build RetroShare first, or point "
                "-DRS_BUILD_DIR=<its build directory> at it.")
        endif()

        message(STATUS "RsWiki: standalone Windows link against ${RS_BUILD_DIR}")
        target_link_libraries(RsWiki PRIVATE
            "${RS_LIBRETROSHARE_IMPLIB}" "${RS_GUI_IMPLIB}")
    endif()
endif()

target_link_libraries(RsWiki PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)

target_compile_definitions(RsWiki PRIVATE RS_NO_WARN_DEPRECATED)

# Keep the library loaded when RetroShare dlclose()s it at shutdown (the
# Windows build pins itself in RsWikiPlugin.cpp).
if(UNIX AND NOT APPLE)
    target_link_options(RsWiki PRIVATE "LINKER:-z,nodelete")
endif()

# Must match the host build: RS_USE_CALENDAR adds a member to RsPlugInInterfaces
# (retroshare/rsplugin.h) before mGxsTunnels, so a plugin built without it reads
# the wrong pointer and crashes at startup. ON by default, like libretroshare.
option(RS_USE_CALENDAR "Build against a RetroShare with the GXS Calendar service (default in CMake builds)" ON)
if(RS_USE_CALENDAR)
    target_compile_definitions(RsWiki PRIVATE RS_USE_CALENDAR)
endif()

target_compile_options(RsWiki PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall;-Wextra;-Wno-deprecated-declarations>
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_GREATER_EQUAL:$<CXX_COMPILER_VERSION>,15>>:-Wno-template-body>
)

if(RS_DEVELOPMENT_BUILD)
    target_compile_options(RsWiki PRIVATE
        $<$<CXX_COMPILER_ID:GNU>:-g;-O0>
    )
endif()

target_include_directories(RsWiki PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/gui
    ${RS_GUI_BINARY_DIR}
    ${RS_SUPERPROJECT_ROOT}/libretroshare/src
    ${RS_SUPERPROJECT_ROOT}/retroshare-gui/src
    ${RS_SUPERPROJECT_ROOT}/retroshare-gui/src/gui
    ${RS_SUPERPROJECT_ROOT}/supportlibs/rapidjson/include
)

install(TARGETS RsWiki DESTINATION lib/retroshare/plugins)
