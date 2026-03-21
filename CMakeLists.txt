cmake_minimum_required(VERSION 3.16)

# Set version from variable if provided, otherwise use default
if(APP_VERSION)
    # CMake project() VERSION must be numeric (major.minor.patch[.tweak])
    # Extract the numeric part (e.g., 0.0.0 from 0.0.0-PR6)
    string(REGEX MATCH "^[0-9]+\\.[0-9]+\\.[0-9]+" APP_VERSION_NUMERIC "${APP_VERSION}")
    if(APP_VERSION_NUMERIC)
        project(Owon1041 VERSION ${APP_VERSION_NUMERIC} LANGUAGES CXX)
    else()
        project(Owon1041 VERSION 0.0.0 LANGUAGES CXX)
    endif()
    # Keep the full version for later use (packaging, etc.)
    set(FULL_APP_VERSION ${APP_VERSION})
    add_definitions(-DAPP_VERSION_STRING="${FULL_APP_VERSION}")
else()
    project(Owon1041 VERSION 0.0.0 LANGUAGES CXX)
    set(FULL_APP_VERSION "0.0.0")
    add_definitions(-DAPP_VERSION_STRING="${FULL_APP_VERSION}")
endif()

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
    "C:/Qt/6.8.2/msvc2022_64"
        "C:/Qt/6.10.2/msvc2022_64"
        "C:/Qt/6.10.2/mingw_64"
        "C:/Qt/6.5.0/msvc2019_64"
        "C:/Qt/6.5.0/mingw_64"
        "C:/Qt/6.*/msvc2022_64"
        "C:/Qt/6.*/mingw_64"
)

# Find the first valid Qt path
foreach(QT_PATH ${QT_COMMON_PATHS})
    if(EXISTS "${QT_PATH}")
        list(APPEND CMAKE_PREFIX_PATH "${QT_PATH}")
        message(STATUS "Found Qt at: ${QT_PATH}")
        break()
    endif()
endforeach()

# Try to find Qt6 first, fall back to Qt5 if not found
set(QT_VERSION_MAJOR 6)
find_package(Qt${QT_VERSION_MAJOR}
    COMPONENTS
        Core
        Gui
        Widgets
        SerialPort
    REQUIRED
)

if(Qt${QT_VERSION_MAJOR}_VERSION VERSION_GREATER_EQUAL "6.3")
    qt_standard_project_setup()
endif()

# Define macOS bundle properties
set(MACOSX_BUNDLE_BUNDLE_NAME "Owon1041")
set(MACOSX_BUNDLE_GUI_IDENTIFIER "de.macwake.Owon1041")
set(MACOSX_BUNDLE_BUNDLE_VERSION ${FULL_APP_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${FULL_APP_VERSION})
set(MACOSX_BUNDLE_LONG_VERSION_STRING "${FULL_APP_VERSION}")
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
elseif(WIN32)
    # Windows icon (requires a resource file)
    set(APP_ICON_WINDOWS "${CMAKE_CURRENT_SOURCE_DIR}/Owon1041.rc")
    set(PLATFORM_SPECIFIC_ICON_FILES ${APP_ICON_WINDOWS})
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

target_link_libraries(Owon1041
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::SerialPort
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
    
    # Install SVG icon (if exists)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/scalable/owon1041.svg")
        install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/linux/icons/scalable/owon1041.svg
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps
        )
    endif()
elseif(WIN32)
    # Installation paths for Windows
    include(GNUInstallDirs)
    install(TARGETS Owon1041
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
    
    # Generate Qt deployment script for Windows
    if(Qt${QT_VERSION_MAJOR}_VERSION VERSION_GREATER_EQUAL "6.3")
        qt_generate_deploy_app_script(
            TARGET Owon1041
            OUTPUT_SCRIPT deploy_script
            NO_TRANSLATIONS
        )
        install(SCRIPT ${deploy_script})
    endif()
endif()

# CPack configuration
set(CPACK_PACKAGE_NAME "Owon1041")
set(CPACK_PACKAGE_VENDOR "MacWake")
set(CPACK_PACKAGE_VERSION "${FULL_APP_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OWON XDM-1041 Multimeter Interface")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/doc/README.md")

if(WIN32)
    set(CPACK_GENERATOR "WIX")
    set(CPACK_WIX_UPGRADE_GUID "D9A1C5B0-1234-5678-90AB-CDEF12345678") # Generate a real one if needed
    set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/Owon1041.ico")
    set(CPACK_PACKAGE_INSTALL_DIRECTORY "Owon1041")
    # Start menu entry
    set(CPACK_PACKAGE_EXECUTABLES "Owon1041" "Owon1041")
    set(CPACK_CREATE_DESKTOP_LINKS "Owon1041")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER "Owon1041")
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "MacWake <info@macwake.de>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6widgets6, libqt6serialport6, libqt6gui6, libqt6core6")
    # For distinct Debian/Ubuntu packages, we might need to adjust dependencies or names in GHA
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
    set(CPACK_DEBIAN_PACKAGE_VERSION "${FULL_APP_VERSION}")
endif()

include(CPack)