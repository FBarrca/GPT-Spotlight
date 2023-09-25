cmake_minimum_required(VERSION 3.2)
project(helloworld_with_helloimgui)
set(CMAKE_CXX_STANDARD 17)

set(DCMAKE_TOOLCHAIN_FILE "C:/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(CMAKE_PREFIX_PATH "C:/vcpkg/installed/x64-windows")
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
find_package(CURL CONFIG REQUIRED)
##########################################################
# Prepare imgui_bundle during configure time
##########################################################
# Download imgui_bundle
include(FetchContent)
Set(FETCHCONTENT_QUIET FALSE)
FetchContent_Declare(
    imgui_bundle
    GIT_REPOSITORY https://github.com/pthom/imgui_bundle.git
    GIT_PROGRESS TRUE
    # Enter the desired git tag below
    GIT_TAG main
)
FetchContent_MakeAvailable(imgui_bundle)

# Make cmake function `imgui_bundle_add_app` available
list(APPEND CMAKE_MODULE_PATH ${IMGUIBUNDLE_CMAKE_PATH})
include(imgui_bundle_add_app)

# Uncomment the next line if you which to also automatically fetch and compile OpenCV for immvision support
# set(IMGUI_BUNDLE_FETCH_OPENCV ON)


##########################################################
# Add libuiohook
##########################################################
# Download libuiohook
FetchContent_Declare(
    libuiohook
    GIT_REPOSITORY https://github.com/kwhat/libuiohook.git
    GIT_PROGRESS TRUE
    GIT_TAG 1.2
)
FetchContent_MakeAvailable(libuiohook)

FetchContent_GetProperties(libuiohook)
if(NOT libuiohook_POPULATED)
    FetchContent_Populate(libuiohook)
    add_subdirectory(${libuiohook_SOURCE_DIR} ${libuiohook_BINARY_DIR})
endif()

# If necessary, specify include directories and libraries manually
include_directories(${libuiohook_SOURCE_DIR}/include)
##########################################################
# Add nlohmann/json
##########################################################
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable(json)

##########################################################
# Add dacap/clip
##########################################################
FetchContent_Declare(
    clip
    GIT_REPOSITORY https://github.com/dacap/clip.git
    GIT_TAG main
)

FetchContent_MakeAvailable(clip)
FetchContent_GetProperties(clip)
if(NOT clip_POPULATED)
  FetchContent_Populate(clip)
  add_subdirectory(${clip_SOURCE_DIR} ${clip_BINARY_DIR})
endif()

##########################################################
# Add oai
########################################################## 
FetchContent_Declare(
    oai
    GIT_REPOSITORY https://github.com/D7EAD/liboai.git
    GIT_TAG main
    GIT_PROGRESS TRUE
)


FetchContent_GetProperties(oai)
if(NOT oai_POPULATED)
# the cmakelist.txt of oai is in the root directory of the repo, but in a subdirectory of the source directory
  FetchContent_Populate(oai)
  add_subdirectory(${oai_SOURCE_DIR}/liboai ${oai_BINARY_DIR})


endif()
##########################################################
# Build your app
##########################################################
# Call imgui_bundle_add_app
imgui_bundle_add_app(GPT-Spotlight main.cpp)

# Link libuiohook to your application
target_link_libraries(GPT-Spotlight PRIVATE uiohook)
target_link_libraries(GPT-Spotlight PRIVATE nlohmann_json::nlohmann_json)
target_link_libraries(GPT-Spotlight PRIVATE clip)
target_include_directories(GPT-Spotlight  PRIVATE ${clip_SOURCE_DIR})
target_link_libraries(GPT-Spotlight PRIVATE oai)
target_include_directories(GPT-Spotlight  PRIVATE ${oai_SOURCE_DIR}/liboai/include)


target_link_libraries(GPT-Spotlight PRIVATE CURL::libcurl)
# Set target properties if necessary
set_target_properties(GPT-Spotlight PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Add commands.json to the builded app
add_custom_command(TARGET GPT-Spotlight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/commands.json
        $<TARGET_FILE_DIR:GPT-Spotlight>
)
# Add openai.json to the builded app
add_custom_command(TARGET GPT-Spotlight POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/openai.json
        $<TARGET_FILE_DIR:GPT-Spotlight>
)
# Now you can build your app with
#     mkdir build && cd build && cmake .. && cmake --build .
