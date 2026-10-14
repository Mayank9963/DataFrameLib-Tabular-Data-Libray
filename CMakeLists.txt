cmake_minimum_required(VERSION 3.15)
project(dataframelib CXX) # Changed to lowercase to match target

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Platform-specific settings for macOS
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

# Find required packages
find_package(Arrow REQUIRED)
find_package(Parquet REQUIRED)

# Build the DataFrame Library (name MUST be 'dataframelib')
add_library(dataframelib
    src/apachearrow.cpp
    src/eagerdataframe.cpp
    src/expression.cpp
    src/graph.cpp
    src/lazydataframe.cpp
    src/queryoptimiser.cpp
    src/expr.cpp
    src/eager_helper.cpp
)

# Include directories
target_include_directories(dataframelib PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Link libraries - Using generic targets for better compatibility
target_link_libraries(dataframelib PUBLIC
    Arrow::arrow_shared
    Parquet::parquet_shared
)
