cmake_minimum_required(VERSION 3.16)
project(Owon1041 VERSION 1.0.0 LANGUAGES CXX)

# Add this near the top, after your project() declaration
if(APPLE)
    # Set the install RPATH to the bundle framework location
    set(CMAKE_INSTALL_RPATH "@executable_path/../Frameworks")
    # Keep the RPATH even when installing
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
    # Make sure RPATHs are used on macOS
    set(CMAKE_MACOSX_RPATH ON)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Try to find Qt in common installation paths
set(QT_COMMON_PATHS
    # Qt6 paths
    "/opt/homebrew/opt/qt@6"
    "/opt/homebrew/opt/qt6"
    "/usr/local/opt/qt@6"
    "/usr/local/opt/qt6"
    "/usr/local/qt6"
    "C:/Qt/6.5.0/msvc2019_64"
    "C:/Qt/6.5.0/mingw_64"
)

# Find the first valid Qt path
foreach(QT_PATH ${QT_COMMON_PATHS})
    if(EXISTS "${QT_PATH}")
        list(APPEND CMAKE_PREFIX_PATH "${QT_PATH}")
        message(STATUS "Found Qt at: ${QT_PATH}")
        break()
    endif()
endforeach()

# Add these lines before find_package()
set(QT_STATIC ON)
set(BUILD_SHARED_LIBS OFF)

# Optionally, for Qt6
set(QT_STATIC_RUNTIME ON)

# If using Qt5, you might need
set(CMAKE_FIND_LIBRARY_SUFFIXES ".a")

set(QT_VERSION_MAJOR 6)

# Modify find_package to include more components if needed
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS
    Core
    Gui
    Widgets
    SerialPort
        REQUIRED
)

# Define macOS bundle properties
set(MACOSX_BUNDLE_BUNDLE_NAME "Owon1041")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "de.macwake.Owon1041")
set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_LONG_VERSION_STRING "${PROJECT_VERSION}")
set(MACOSX_BUNDLE_COPYRIGHT "Copyright © 2025 MacWake. All rights reserved.")
set(MACOSX_BUNDLE_INFO_STRING "OWON XDM-1041 Multimeter Interface")

# Icons setup based on platform
if(APPLE)
    # macOS icon
    set(MACOSX_BUNDLE_ICON_FILE Owon.icns)
    set(APP_ICON_MACOSX ${CMAKE_CURRENT_SOURCE_DIR}/Owon.icns)
    set_source_files_properties(${APP_ICON_MACOSX} PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources")
    set(PLATFORM_SPECIFIC_ICON_FILES ${APP_ICON_MACOSX})
elseif(UNIX AND NOT APPLE)
    # Linux doesn't use resource files in the executable directly
    set(PLATFORM_SPECIFIC_ICON_FILES "")
endif()

# Add executable with platform-specific resources
add_executable(Owon1041 
    ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/MainWindow.h
    ${CMAKE_CURRENT_SOURCE_DIR}/Settings.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/Settings.h
    ${CMAKE_CURRENT_SOURCE_DIR}/ConnectDialog.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/ConnectDialog.h
    ${PLATFORM_SPECIFIC_ICON_FILES}
)

# Update target_link_libraries to use static libraries
target_link_libraries(Owon1041
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
        # Windows MinGW specific static libraries
        $<$<AND:$<PLATFORM_ID:Windows>,$<CXX_COMPILER_ID:GNU>>:-static-libgcc>
        $<$<AND:$<PLATFORM_ID:Windows>,$<CXX_COMPILER_ID:GNU>>:-static-libstdc++>
)

# Set platform-specific properties
if(APPLE)
    set_target_properties(Owon1041 PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "${MACOSX_BUNDLE_BUNDLE_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "${MACOSX_BUNDLE_GUI_IDENTIFIER}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${MACOSX_BUNDLE_BUNDLE_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${MACOSX_BUNDLE_SHORT_VERSION_STRING}"
        MACOSX_BUNDLE_LONG_VERSION_STRING "${MACOSX_BUNDLE_LONG_VERSION_STRING}"
        MACOSX_BUNDLE_COPYRIGHT "${MACOSX_BUNDLE_COPYRIGHT}"
        MACOSX_BUNDLE_INFO_STRING "${MACOSX_BUNDLE_INFO_STRING}"
        MACOSX_BUNDLE_ICON_FILE "${MACOSX_BUNDLE_ICON_FILE}"
    )
    
    # Set High DPI support
    target_compile_definitions(Owon1041 PRIVATE
        "QT_MAC_WANTS_LAYER=1"
    )
elseif(UNIX AND NOT APPLE)
    # Installation paths for Linux
    include(GNUInstallDirs)
    
    # Install the executable
    install(TARGETS Owon1041
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    # Install desktop file
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/owon1041.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications
    )
    
    # Install icons in various sizes
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/16x16/owon1041.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/16x16/apps
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/32x32/owon1041.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/32x32/apps
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/48x48/owon1041.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/48x48/apps
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/64x64/owon1041.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/64x64/apps
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/128x128/owon1041.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/128x128/apps
    )
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/256x256/owon1041.png
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/256x256/apps
    )
    
    # Install SVG icon
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/scalable/owon1041.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
    )
endif()

# Add these definitions to handle static Qt
target_compile_definitions(Owon1041 PRIVATE
        QT_STATIC_BUILD
)