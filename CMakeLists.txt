cmake_minimum_required(VERSION 3.17)
project(PretextView VERSION "1.0.6")


option(FORCE_MAC_X86 "Force macOS x86_64 build" OFF)
option(BUILD_UNIVERSAL "Build macOS universal binary" OFF) 
option(WITH_TORCH "Find and link torch lib" OFF)
option(DEBUG_OUTPUT_LIKELIHOOD_TABLE "Output likelihood table to file" OFF)
option(DEBUG_OUTPUT_PIXEL_CUT_FILE "Output the pixel cut files" OFF)
option(PYTHON_SCOPED_INTERPRETER "Use Python scoped interpreter" OFF)
option(AI_PIC_DETECTION "Enable AI error pattern detection " OFF)

set(CMAKE_C_STANDARD 17)   
set(CMAKE_CXX_STANDARD 17)   
set(CMAKE_C_STANDARD_REQUIRED ON)  
set(CMAKE_CXX_STANDARD_REQUIRED ON)  

set(target_name PretextViewAI)



if (APPLE)
    if(FORCE_MAC_X86) 
        message(STATUS "Forcing x86_64 architecture build")
        set(CMAKE_OSX_ARCHITECTURES "x86_64" CACHE STRING "Build architectures" FORCE)
    elseif(BUILD_UNIVERSAL) 
        message(STATUS "Building macOS universal binary (x86_64 + arm64)")
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64" CACHE STRING "Build architectures" FORCE)
    else()  
        if(CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
            set(CMAKE_OSX_ARCHITECTURES "arm64")
            message(STATUS "Auto-detected architecture: arm64")
        else()
            set(CMAKE_OSX_ARCHITECTURES "x86_64")
            message(STATUS "Auto-detected architecture: x86_64")
        endif()
    endif()

    message(STATUS "MacOS deployment target: ${CMAKE_OSX_DEPLOYMENT_TARGET}")
    message(STATUS "MacOS target architectures: ${CMAKE_OSX_ARCHITECTURES}")

    set(ARCH_TYPE ${CMAKE_OSX_ARCHITECTURES})

else()
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64")
        set(ARCH_TYPE "x86_64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(ARCH_TYPE "ARM64")
    elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "i[3-6]86")
        set(ARCH_TYPE "x86")
    else()
        message(FATAL_ERROR "Unsupported architecture")
    endif()
    if(WIN32)
        message(STATUS "Windows build")
    elseif(UNIX AND NOT APPLE)
        message(STATUS "Linux build")
    else()  
        message(FATAL_ERROR "Unsupported platform")
    endif()
endif()

if (MSVC)
    add_compile_options(/utf-8)  # force MSVC to use UTF-8 
endif()

message(STATUS "Install path: ${CMAKE_INSTALL_PREFIX}")


if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Build version: Debug ")
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -g -O0")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -g -O0")
else()
    message(STATUS "Build version: Release ")
    set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O3")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3")
endif()

include_directories(
    include
    src
    subprojects/glfw/include
    subprojects/libdeflate
    subprojects/glm
    subprojects/fmt/include
    subprojects/eigen
)

add_subdirectory(subprojects/glfw)
add_subdirectory(subprojects/glm)

find_library(
    lib_fmt
    NAMES fmt libfmt
    PATHS 
        subprojects/fmt/build 
        subprojects/fmt/build/debug 
        subprojects/fmt/build/Release
        subprojects/fmt/build/release)
if(NOT lib_fmt)
    message(FATAL_ERROR "lib_fmt not found")
else()
    message(STATUS "lib_fmt found: ${lib_fmt}")
endif()


# test the parse_agp
if (AGP_TEST_FLAG)
    add_executable(parse_agp_test src/parse_agp_test.cpp)
    target_link_libraries(parse_agp_test PRIVATE ${lib_fmt})
    if (CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_compile_definitions(parse_agp_test PRIVATE DEBUG)
    endif()
endif()

find_library(
    lib_deflate_static 
    NAMES deflate deflatestatic
    PATHS 
        subprojects/libdeflate/build 
        subprojects/libdeflate/build/debug 
        subprojects/libdeflate/build/Release
        subprojects/libdeflate/build/release)
if(NOT lib_deflate_static)
    message(FATAL_ERROR "lib_deflate_static not found")
else()
    message(STATUS "lib_deflate_static found: ${lib_deflate_static}")
endif()

add_library(yahs STATIC 
    src/yahs_sort.cpp
)

# set(CMAKE_PREFIX_PATH "subprojects/libtorch/share/cmake/Torch")
if (WITH_TORCH)
    message(STATUS "CMAKE_PREFIX_PATH: ${CMAKE_PREFIX_PATH}")
    find_package(Torch REQUIRED)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${TORCH_CXX_FLAGS}")
    message(STATUS "Torch libs: ${TORCH_LIBRARIES}")
    message(STATUS "Torch install prefix: ${TORCH_INSTALL_PREFIX}")
    # set(CMAKE_PREFIX_PATH "../libtorch/share/cmake/Torch")
    set(WITH_PYTHON OFF CACHE BOOL "Disable Python integration for pytorch_scatter" FORCE)
    add_subdirectory(subprojects/pytorch_scatter)
    add_library(sortlib STATIC src/aisort.cpp )
    target_link_libraries(
        sortlib PUBLIC 
        ${TORCH_LIBRARIES} 
        ${torchscatter} 
        yahs)
else()
    message(STATUS "Libtorch: Build without libtorch")
    add_library(sortlib STATIC src/frag_sort.cpp )
    target_link_libraries(sortlib PRIVATE yahs)
endif()


add_library(glad STATIC src/glad.c)
add_library(utilsPretextView STATIC src/utilsPretextView.cpp)
add_library(copy_texture STATIC src/copy_texture.cpp)


if(NOT APPLE)
    add_executable( ${target_name} PretextView.cpp )
else()
    add_executable( ${target_name} MACOSX_BUNDLE PretextView.cpp )
    set_target_properties(
        ${target_name} PROPERTIES
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/Info.plist"
    )
endif()

target_compile_definitions(${target_name} PRIVATE PV=${PROJECT_VERSION})
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${target_name} PRIVATE DEBUG)
    target_compile_definitions(copy_texture PRIVATE DEBUG)
endif()
if (DEBUG_OUTPUT_LIKELIHOOD_TABLE)
    message(STATUS "DEBUG_OUTPUT_LIKELIHOOD_TABLE: defined")
    target_compile_definitions(${target_name} PRIVATE DEBUG_OUTPUT_LIKELIHOOD_TABLE)
else()
    message(STATUS "DEBUG_OUTPUT_LIKELIHOOD_TABLE: not defined")
endif()
if (AI_PIC_DETECTION)
    message(STATUS "AI_PIC_DETECTION: defined")
    target_compile_definitions(${target_name} PRIVATE AI_PIC_DETECTION)
else()
    message(STATUS "AI_PIC_DETECTION: not defined")
endif()


target_link_libraries(
    ${target_name} PRIVATE 
    ${lib_deflate_static} 
    ${lib_fmt}
    sortlib 
    glm 
    glad 
    utilsPretextView 
    copy_texture 
    glfw 
    yahs
)   

# link libraries only for debug version
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    include_directories(
        subprojects/imgui
    )
    add_library(imgui STATIC 
        subprojects/imgui/imgui_demo.cpp
        subprojects/imgui/imgui_draw.cpp
        subprojects/imgui/imgui_tables.cpp
        subprojects/imgui/imgui_widgets.cpp
        subprojects/imgui/imgui.cpp
        subprojects/imgui/backends/imgui_impl_glfw.cpp
        subprojects/imgui/backends/imgui_impl_opengl3.cpp
    )
    target_link_libraries(${target_name} PRIVATE imgui)
endif()

if (APPLE)
    set(library_need Cocoa OpenGL IOKit CoreVideo) 
    find_library(METAL_LIB Metal)
    find_library(ACCELERATE_LIB Accelerate)
    if (METAL_LIB) 
        list(APPEND library_need Metal)
    endif()
    if (ACCELERATE_LIB)
        list(APPEND library_need Accelerate)
    endif()
elseif(WIN32)
    set(library_need gdi32 opengl32 kernel32 user32 shell32 Ole32) 
elseif(UNIX AND NOT APPLE)
    set(library_need m dl X11) 
else()
    message(FATAL_ERROR "Unsupported platform")
endif()


foreach (library ${library_need})
    if ( NOT WIN32)
        find_library(${library}_LIB ${library})
        if (NOT ${library}_LIB)
            message(FATAL_ERROR "${library} not found")
        else()
            message(STATUS "${library} found: ${${library}_LIB}")  
        endif()
        target_link_libraries(${target_name} PRIVATE ${${library}_LIB} )
    else() # WIN32
        foreach (library ${library_need})
            target_link_libraries(${target_name} PRIVATE ${library} )
        endforeach()
    endif()
endforeach()



# ============ Kmeans for Clustering ==============

if (PYTHON_SCOPED_INTERPRETER)
    find_package(Python COMPONENTS Development Interpreter)
else()
    message(STATUS "PYTHON_SCOPED_INTERPRETER defined, but Python not found, build without PYTHON_SCOPED_INTERPRETER")
endif()

if (PYTHON_SCOPED_INTERPRETER AND Python_FOUND)
    message(STATUS "PYTHON_SCOPED_INTERPRETER defined, build with PYTHON_SCOPED_INTERPRETER")
    add_subdirectory(subprojects/pybind11)
    # 拷贝python lib文件到编译目录下
    get_filename_component(Python_LIBRARIES "${Python_LIBRARIES}" REALPATH)
    get_filename_component(Python_lib_name "${Python_LIBRARIES}" NAME)
    file(COPY ${Python_LIBRARIES} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    message(STATUS "Python lib realpath: ${Python_LIBRARIES}, name: ${Python_lib_name}")
    # 获取拷贝后文件的路径
    # find_library(PYTHON_LIB_PATH 
    #     NAMES ${Python_lib_name}  
    #     PATHS ${CMAKE_CURRENT_BINARY_DIR} 
    #     REQUIRED)
    set(PYTHON_LIB_PATH "${CMAKE_CURRENT_BINARY_DIR}/${Python_lib_name}")
    add_custom_command(TARGET ${target_name} POST_BUILD
        COMMAND install_name_tool -id @rpath/${Python_lib_name} ${Python_LIBRARIES}
        COMMENT "Fix install_name of python lib")    
    message(STATUS "Build with PYTHON_SCOPED_INTERPRETER. Python search path: ${CMAKE_CURRENT_BINARY_DIR}, Python version: ${Python_VERSION}, Python_lib: ${PYTHON_LIB_PATH}")

    # 将 Python 脚本复制到构建目录
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/python/frag_cluster/kmeans_utils.py
        ${CMAKE_CURRENT_BINARY_DIR}/kmeans_utils.py
        COPYONLY )

    # 链接 pybind11 和 Python 库
    target_link_libraries(${target_name} PRIVATE 
        ${PYTHON_LIB_PATH}
        pybind11::embed 
    )
    message(STATUS "Using Python scoped interpreter for Kmeans")
    target_compile_definitions(${target_name} PRIVATE PYTHON_SCOPED_INTERPRETER)
endif()


# ============ CMAKE INSTALLATION ==============
file(GLOB aimodel_files "${CMAKE_CURRENT_SOURCE_DIR}/ai_model/*")
file(GLOB torch_copy_libs "${TORCH_INSTALL_PREFIX}/lib/*")
if (APPLE)
    execute_process(
        COMMAND bash ${CMAKE_SOURCE_DIR}/make_macos_app_plist.sh ${PROJECT_VERSION}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error_output
        TIMEOUT 30
        )
endif()
if (CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Debug on ${CMAKE_SYSTEM_NAME} - ${ARCH_TYPE}.")
    if (APPLE)
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist" DESTINATION "${target_name}.app/Contents")
        file(COPY "${CMAKE_CURRENT_SOURCE_DIR}/icon.icns" DESTINATION "${target_name}.app/Contents/Resources" )
        if (WITH_TORCH)
            file(COPY ${aimodel_files} DESTINATION "${target_name}.app/Contents/Resources/ai_model")
        endif()
    endif()
endif()

if(APPLE)
    # set the shared/dynamic library path for the installed binary
    message(STATUS "[Cmake Install]: Mac")
    set_target_properties(
        ${target_name} 
        PROPERTIES 
        INSTALL_RPATH "@executable_path/../Frameworks"
        BUILD_WITH_INSTALL_RPATH TRUE
        MACOSX_RPATH TRUE
        BUNDLE TRUE
        )
    install(TARGETS ${target_name}
            BUNDLE DESTINATION .
            COMPONENT Runtime)  
    # install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist"
    #         DESTINATION "${target_name}.app/Contents")
    install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/icon.icns"
            DESTINATION "${target_name}.app/Contents/Resources" )
    install(
        FILES ${lib_deflate_static}
        DESTINATION "${target_name}.app/Contents/Frameworks")
    # Install libfmt dylib - get the real file path
    get_filename_component(lib_fmt_real ${lib_fmt} REALPATH)
    install(
        FILES ${lib_fmt_real}
        DESTINATION "${target_name}.app/Contents/Frameworks")
    # Create symlink for libfmt.11.dylib during install
    get_filename_component(lib_fmt_realname ${lib_fmt_real} NAME)
    install(CODE "
        execute_process(
            COMMAND ln -sf ${lib_fmt_realname} libfmt.11.dylib
            WORKING_DIRECTORY \${CMAKE_INSTALL_PREFIX}/${target_name}.app/Contents/Frameworks
        )
        execute_process(
            COMMAND install_name_tool -id @rpath/libfmt.11.dylib ${lib_fmt_realname}
            WORKING_DIRECTORY \${CMAKE_INSTALL_PREFIX}/${target_name}.app/Contents/Frameworks
        )
    ")
    if (WITH_TORCH)
        install(FILES ${torch_copy_libs}
                DESTINATION "${target_name}.app/Contents/Frameworks" )
        install(FILES ${aimodel_files}
                DESTINATION "${target_name}.app/Contents/Resources/ai_model" )
    endif()
    if (PYTHON_SCOPED_INTERPRETER)
        install(
            FILES ${PYTHON_LIB_PATH}
            DESTINATION "${target_name}.app/Contents/Frameworks" )
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/python/frag_cluster/kmeans_utils.py
            DESTINATION "${target_name}.app/Contents/Resources" )
    endif ()
elseif(UNIX AND NOT APPLE) # Linux
    message(STATUS "[Cmake Install]: Linux")
    set_target_properties(
        ${target_name}
        PROPERTIES 
        INSTALL_RPATH "$ORIGIN/../lib")
    install(
        TARGETS ${target_name}
        RUNTIME DESTINATION "bin"
        COMPONENT Runtime)
    install(
        FILES ${lib_deflate_static}
        DESTINATION "lib")
    if (WITH_TORCH)
        install(
            FILES ${torch_copy_libs} 
            DESTINATION "lib") 
        install(
            FILES ${aimodel_files} 
            DESTINATION "ai_model")
    endif()
    if (PYTHON_SCOPED_INTERPRETER)
        install(
            FILES ${PYTHON_LIB_PATH}
            DESTINATION "lib" )
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/python/frag_cluster/kmeans_utils.py
            DESTINATION "." )
    endif ()
else () # windows
    message(STATUS "[Cmake Install]: Windows")
    target_sources(${target_name} PRIVATE "${CMAKE_SOURCE_DIR}/PretextView.rc") # add logo to win version
    install(
        TARGETS ${target_name}
        RUNTIME DESTINATION .
        COMPONENT Runtime)
    install(
        FILES ${lib_deflate_static}
        DESTINATION .)
    if (WITH_TORCH)
        install(
            FILES ${torch_copy_libs} 
            DESTINATION .) # libtorch
        install(
            FILES ${aimodel_files}   
            DESTINATION "ai_model")
    endif()
    if (PYTHON_SCOPED_INTERPRETER)
        install(
            FILES ${PYTHON_LIB_PATH}
            DESTINATION . )
        install(
            FILES ${CMAKE_CURRENT_SOURCE_DIR}/python/frag_cluster/kmeans_utils.py
            DESTINATION . )
    endif ()
endif()


# ==== CPack: installation package ====
include(InstallRequiredSystemLibraries)
set(CPACK_PACKAGE_NAME ${target_name})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DIRECTORY ${CMAKE_BINARY_DIR})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/sanger-tol/PretextView")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_SYSTEM_NAME}-${ARCH_TYPE}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENCE.txt")

if(${CMAKE_SYSTEM_NAME} STREQUAL "Darwin") # macOS
    message(STATUS "[CPack]: Generate on Mac")
    set(CPACK_GENERATOR "DragNDrop")
    set(CPACK_DMG_VOLUME_NAME "${target_name} Installer")
    set(CPACK_DMG_FORMAT "UDZO") 
elseif(${CMAKE_SYSTEM_NAME} STREQUAL "Linux") # Linux (Ubuntu)
    message(STATUS "[CPack]: Generate on Linux")
    install(
        FILES "${CMAKE_SOURCE_DIR}/ico_design/icon_48x48x32.png"
        DESTINATION "/usr/share/icons/hicolor/48x48/apps"
        RENAME "pretextview.png"
    )
    install(
        FILES "${CMAKE_SOURCE_DIR}/ico_design/icon_64x64x32.png"
        DESTINATION "share/icons/hicolor/64x64/apps"
        RENAME "pretextview.png"
    )
    install(
        FILES "${CMAKE_SOURCE_DIR}/ico_design/icon_128x128x32.png"
        DESTINATION "/usr/share/icons/hicolor/128x128/apps"
        RENAME "pretextview.png"
    )
    install(
        FILES "${CMAKE_SOURCE_DIR}/ico_design/icon_256x256x32.png"
        DESTINATION "/usr/share/icons/hicolor/256x256/apps"
        RENAME "pretextview.png"
    )
    install(
        FILES "${CMAKE_SOURCE_DIR}/ico_design/icon_512x512x32.png"
        DESTINATION "/usr/share/icons/hicolor/512x512/apps"
        RENAME "pretextview.png"
    )
    configure_file(
        "${CMAKE_SOURCE_DIR}/pretextviewai.desktop.in"
        "${CMAKE_BINARY_DIR}/pretextviewai.desktop"
        @ONLY
    )
    install(
        FILES "${CMAKE_BINARY_DIR}/pretextviewai.desktop"
        DESTINATION "share/applications"
    )
    set(CPACK_GENERATOR "DEB")
    set(CPACK_DEBIAN_HARDLINKS OFF)
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Shaoheng Guan") 
    set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "This is ${target_name}, HiC map viewer, editor with AI-powered automatic curation.")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.29)")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils") 
elseif(WIN32) # Windows
    message(STATUS "[CPack]: Generate on Windows")
    set(CPACK_GENERATOR "NSIS")
    set(CPACK_NSIS_DISPLAY_NAME "${target_name}")
    set(CPACK_NSIS_PACKAGE_NAME "${target_name} Installer")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/pretextview_icon.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/pretextview_icon.ico") 
    set(CPACK_NSIS_INSTALLED_ICON_NAME "${target_name}.exe")
    set(CPACK_NSIS_HELP_LINK "https://github.com/guanshaoheng/PretextView") 
    set(CPACK_NSIS_URL_INFO_ABOUT "https://github.com/guanshaoheng/PretextView") 
endif()

include(CPack)