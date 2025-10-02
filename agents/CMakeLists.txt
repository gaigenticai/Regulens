set(TARGET_NAME regulens_agents)

# Find sources
file(GLOB_RECURSE SOURCES "*.cpp" "*.hpp")

# Create library
add_library(${TARGET_NAME} ${SOURCES})

# Include directories
target_include_directories(${TARGET_NAME}
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE
        ${CMAKE_SOURCE_DIR}/shared
        ${CMAKE_SOURCE_DIR}/shared/network
        ${CMAKE_SOURCE_DIR}/core
)

# Link dependencies
target_link_libraries(${TARGET_NAME}
    PUBLIC
        regulens_shared
        regulens_core
    PRIVATE
        CURL::libcurl
        Threads::Threads
)

# Compile definitions
target_compile_definitions(${TARGET_NAME}
    PRIVATE
        $<$<CONFIG:Debug>:DEBUG>
        $<$<CONFIG:Release>:NDEBUG>
)

# Set C++ standard
set_target_properties(${TARGET_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)