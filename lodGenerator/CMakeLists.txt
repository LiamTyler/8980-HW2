cmake_minimum_required(VERSION 3.1.3)

set(PROJECT_NAME "LOD_Generator")
 
project(${PROJECT_NAME})
 
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
 
 
include_directories(${CMAKE_CURRENT_SOURCE_DIR})

set(SRCS
    main.cpp
    material.cpp
    mesh.cpp
    model.cpp
)

set(INCS
    material.hpp
    model.hpp
    mesh.hpp
    serialize.hpp

    ../external/tiny_obj_loader.h
)
 
add_executable(lodGenerator ${SRCS} ${INCS})
target_link_libraries(lodGenerator meshoptimizer)
 
set_target_properties( lodGenerator 
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/../GEFS"
)
