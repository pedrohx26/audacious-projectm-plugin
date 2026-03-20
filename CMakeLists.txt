cmake_minimum_required(VERSION 3.16)
project(audacious-projectm VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# ── Qt ──────────────────────────────────────────────────────────────
find_package(Qt6 COMPONENTS Core Gui Widgets OpenGL OpenGLWidgets QUIET)
if(Qt6_FOUND)
    set(QT_LIBRARIES Qt6::Core Qt6::Gui Qt6::Widgets Qt6::OpenGL Qt6::OpenGLWidgets)
    message(STATUS "Using Qt6")
else()
    find_package(Qt5 5.12 REQUIRED COMPONENTS Core Gui Widgets OpenGL)
    set(QT_LIBRARIES Qt5::Core Qt5::Gui Qt5::Widgets Qt5::OpenGL)
    message(STATUS "Using Qt5")
endif()

# ── Audacious ──────────────────────────────────────────────────────
find_package(PkgConfig REQUIRED)
pkg_check_modules(AUDACIOUS REQUIRED IMPORTED_TARGET audacious>=4.0)

execute_process(
    COMMAND pkg-config --variable=plugin_dir audacious
    OUTPUT_VARIABLE AUDACIOUS_PLUGIN_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(VIS_PLUGIN_DIR "${AUDACIOUS_PLUGIN_DIR}/Visualization")

# ── libprojectM 4.x (try several pkg-config names) ────────────────
set(_PM_FOUND FALSE)
foreach(_name libprojectM projectM-4 libprojectM-4)
    if(NOT _PM_FOUND)
        pkg_check_modules(PROJECTM QUIET IMPORTED_TARGET ${_name}>=4.0)
        if(PROJECTM_FOUND)
            set(_PM_FOUND TRUE)
            message(STATUS "Found libprojectM via '${_name}'")
        endif()
    endif()
endforeach()
if(NOT _PM_FOUND)
    message(FATAL_ERROR "libprojectM >= 4.0 not found. Install the 'projectm' package.")
endif()

find_library(PROJECTM_LIBRARY_FILE
    NAMES projectM-4 libprojectM-4
    HINTS ${PROJECTM_LIBRARY_DIRS} /usr/lib /usr/local/lib
    REQUIRED)

# ── libprojectM playlist ──────────────────────────────────────────
set(_PL_FOUND FALSE)
foreach(_name libprojectM-playlist projectM-4-playlist libprojectM-4-playlist)
    if(NOT _PL_FOUND)
        pkg_check_modules(PROJECTM_PL QUIET IMPORTED_TARGET ${_name})
        if(PROJECTM_PL_FOUND)
            set(_PL_FOUND TRUE)
            message(STATUS "Found playlist lib via '${_name}'")
        endif()
    endif()
endforeach()

find_library(PROJECTM_PL_LIBRARY_FILE
    NAMES projectM-4-playlist libprojectM-4-playlist
    HINTS ${PROJECTM_PL_LIBRARY_DIRS} /usr/lib /usr/local/lib)

# ── OpenGL ─────────────────────────────────────────────────────────
find_package(OpenGL REQUIRED)

# ── Build ──────────────────────────────────────────────────────────
add_library(projectm-vis SHARED src/projectm-vis.cc)

target_compile_definitions(projectm-vis PRIVATE
    HAVE_GETTEXT
    PACKAGE="audacious-projectm")

target_link_libraries(projectm-vis PRIVATE
    ${QT_LIBRARIES}
    PkgConfig::AUDACIOUS
    ${PROJECTM_LIBRARY_FILE}
    OpenGL::GL)

if(PROJECTM_PL_FOUND AND PROJECTM_PL_LIBRARY_FILE)
    target_link_libraries(projectm-vis PRIVATE ${PROJECTM_PL_LIBRARY_FILE})
endif()

set_target_properties(projectm-vis PROPERTIES PREFIX "")

# ── Preset Browser General Plugin ─────────────────────────────────
execute_process(
    COMMAND pkg-config --variable=plugin_dir audacious
    OUTPUT_VARIABLE AUDACIOUS_PLUGIN_DIR2
    OUTPUT_STRIP_TRAILING_WHITESPACE)
set(GENERAL_PLUGIN_DIR "${AUDACIOUS_PLUGIN_DIR2}/General")

add_library(projectm-browser SHARED src/projectm-browser.cc)

target_compile_definitions(projectm-browser PRIVATE
    HAVE_GETTEXT
    PACKAGE="audacious-projectm")

target_link_libraries(projectm-browser PRIVATE
    ${QT_LIBRARIES}
    PkgConfig::AUDACIOUS)

set_target_properties(projectm-browser PROPERTIES PREFIX "")

# ── Install ────────────────────────────────────────────────────────
install(TARGETS projectm-vis    LIBRARY DESTINATION "${VIS_PLUGIN_DIR}")
install(TARGETS projectm-browser LIBRARY DESTINATION "${GENERAL_PLUGIN_DIR}")
