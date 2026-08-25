cmake_minimum_required(VERSION 3.20)

add_executable(cjoka
    src/main.cpp
    src/Demo.cpp
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

# LSAN ложно ругается на libglfw/libdecor (423KB в 10k аллокациях) — не наш код.
# Запуск: LSAN_OPTIONS=suppressions=engine/sanitizer/lsan.supp ./build/cjoka
# или ASAN_OPTIONS=detect_leaks=0 ./build/cjoka (быстрый костыль)
# sjoka.sh play game уже выставляет LSAN_OPTIONS автоматически.

# Для удобства: бинарник в build/game/ но также доступен как build/cjoka через симлинк
set_target_properties(cjoka PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})
