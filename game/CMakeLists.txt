cmake_minimum_required(VERSION 3.20)

add_executable(cjoka
    src/main.cpp
    src/Demo.cpp
    src/SceneEditor.cpp
)

target_include_directories(cjoka PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(cjoka PRIVATE cjoka_engine)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(cjoka PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        $<$<CONFIG:Debug>:-g -O0 -fno-omit-frame-pointer -fsanitize=address,undefined>
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
    target_link_options(cjoka PRIVATE
        $<$<CONFIG:Debug>:-fsanitize=address,undefined>
    )
endif()

# Standalone Executable target for distribution / game exports
add_executable(cjoka_standalone
    src/StandaloneMain.cpp
)
target_include_directories(cjoka_standalone PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(cjoka_standalone PRIVATE cjoka_engine)
set_target_properties(cjoka_standalone PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(cjoka_standalone PRIVATE
        -Wall -Wextra -Wpedantic -Wconversion -Wshadow
        $<$<CONFIG:Release>:-O3 -DNDEBUG>
    )
endif()

add_custom_command(TARGET cjoka_standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink ${CMAKE_SOURCE_DIR}/assets ${CMAKE_BINARY_DIR}/assets
)

