cmake_minimum_required(VERSION 3.10)
project(CG_LAB)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True) # Good practice
include_directories(/opt/homebrew/include)
link_directories(/opt/homebrew/lib)

# --- OpenCV Integration ---
# Help CMake find packages installed by Homebrew
# For Apple Silicon (M1/M2/M3): /opt/homebrew
# For Intel Macs: /usr/local
# CMake will search <prefix>/lib/cmake/OpenCV or <prefix>/share/cmake/OpenCV
# for OpenCVConfig.cmake or opencv-config.cmake
if(APPLE AND CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "arm64") # Apple Silicon
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew/opt/opencv") # More specific
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")         # General Homebrew prefix
elseif(APPLE) # Intel Macs
    list(APPEND CMAKE_PREFIX_PATH "/usr/local/opt/opencv") # More specific
    list(APPEND CMAKE_PREFIX_PATH "/usr/local")            # General Homebrew prefix
endif()

# Find OpenCV package.
# REQUIRED means CMake will error out if it can't find OpenCV.
# You can specify components if you only need specific modules, e.g.:
# find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc)
find_package(OpenCV REQUIRED)

# --- End OpenCV Integration ---

# Silence OpenGL deprecation warnings on macOS
if(APPLE)
    add_definitions(-DGL_SILENCE_DEPRECATION)
endif()

add_executable(CG_LAB main.cpp)

# Add OpenCV include directories to your target
# target_include_directories is preferred over global include_directories
if(OpenCV_FOUND)
    target_include_directories(CG_LAB PRIVATE ${OpenCV_INCLUDE_DIRS})
endif()

# Link with libraries
target_link_libraries(CG_LAB PRIVATE
        ${OpenCV_LIBRARIES} # This variable is set by find_package(OpenCV)
        # macOS Frameworks
        "-framework OpenGL"
        "-framework GLUT"
        "-framework Cocoa"
)

# Optional: Print a message to confirm OpenCV was found and which version
if(OpenCV_FOUND)
    message(STATUS "Found OpenCV version: ${OpenCV_VERSION}")
    message(STATUS "OpenCV include_dirs: ${OpenCV_INCLUDE_DIRS}")
    message(STATUS "OpenCV libraries: ${OpenCV_LIBRARIES}") # This can be a long list
endif()

# The old include_directories and link_directories are generally not needed
# when using find_package correctly, as find_package populates
# OpenCV_INCLUDE_DIRS and OpenCV_LIBRARIES.
# You can remove these:
# include_directories(/opt/homebrew/include)
# link_directories(/opt/homebrew/lib)

#
#
#cmake_minimum_required(VERSION 3.10)
#project(CG_LAB)
#
#set(CMAKE_CXX_STANDARD 14)
#
## Include Homebrew directories for Apple Silicon
#include_directories(/opt/homebrew/include)
#link_directories(/opt/homebrew/lib)
#
## Silence OpenGL deprecation warnings on macOS
#add_definitions(-DGL_SILENCE_DEPRECATION)
#
#add_executable(CG_LAB main.cpp)
#
## Link with frameworks properly on macOS
#target_link_libraries(CG_LAB
#        "-framework OpenGL"
#        "-framework GLUT"
#        "-framework Cocoa"
#)