cmake_minimum_required(VERSION 3.12)
project(logging_lib CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Пути
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Источники библиотеки
set(LOGGING_SOURCES
    src/logger.cpp
)

set(LOGGING_HEADERS
    include/logging/logger.hpp
    src/sinks.hpp
)

# Статическая библиотека
add_library(logging STATIC ${LOGGING_SOURCES} ${LOGGING_HEADERS})
target_compile_definitions(logging PRIVATE -D_POSIX_C_SOURCE=200809L)
target_link_libraries(logging PRIVATE)

# Динамическая библиотека
add_library(logging_shared SHARED ${LOGGING_SOURCES} ${LOGGING_HEADERS})
set_target_properties(logging_shared PROPERTIES OUTPUT_NAME "logging")
target_compile_definitions(logging_shared PRIVATE -D_POSIX_C_SOURCE=200809L)
target_link_libraries(logging_shared PRIVATE)

# Приложения
add_executable(logger_demo apps/logger_demo.cpp)
target_link_libraries(logger_demo PRIVATE logging pthread)

add_executable(stats_collector apps/stats_collector.cpp)
target_link_libraries(stats_collector PRIVATE logging pthread)

# Установка (опционально)
install(TARGETS logging logging_shared logger_demo stats_collector
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
install(FILES include/logging/logger.hpp DESTINATION include/logging)

