# Tool integration library

add_library(regulens_tool_integration STATIC
    tool_interface.cpp
    tools/email_tool.cpp
)

target_include_directories(regulens_tool_integration
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(regulens_tool_integration
    PUBLIC
        regulens_shared
        CURL::libcurl
        Threads::Threads
)
