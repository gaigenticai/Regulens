# Event system library

add_library(regulens_event_system STATIC
    event.cpp
    event_bus.cpp
)

target_include_directories(regulens_event_system
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(regulens_event_system
    PUBLIC
        regulens_shared
        Threads::Threads
)
