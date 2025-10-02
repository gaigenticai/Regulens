# Web UI Library

add_library(regulens_web_ui STATIC
    web_ui_server.cpp
    web_ui_handlers.cpp
    regulatory_monitor_ui.cpp
)

target_include_directories(regulens_web_ui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../config
        ${CMAKE_CURRENT_SOURCE_DIR}/../logging
        ${CMAKE_CURRENT_SOURCE_DIR}/../metrics
        ${CMAKE_CURRENT_SOURCE_DIR}/../database
)

target_link_libraries(regulens_web_ui
    PUBLIC
        regulens_shared
        Threads::Threads
        ${Boost_LIBRARIES}
)
