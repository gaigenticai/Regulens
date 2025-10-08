# Regulatory monitor library - Production-grade regulatory change monitoring system

add_library(regulens_regulatory_monitor STATIC
    regulatory_monitor.cpp
    regulatory_monitor.hpp
    regulatory_source.cpp
    regulatory_source.hpp
    change_detector.cpp
    change_detector.hpp
    production_regulatory_monitor.cpp
    production_regulatory_monitor.hpp
    rest_api_server.cpp
    rest_api_server.hpp
)

target_include_directories(regulens_regulatory_monitor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
)

target_link_libraries(regulens_regulatory_monitor
    PUBLIC
        regulens_shared
)


