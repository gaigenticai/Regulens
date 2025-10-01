# Regulatory monitor library - TODO: Implement regulatory monitoring components

add_library(regulens_regulatory_monitor INTERFACE)

target_include_directories(regulens_regulatory_monitor
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(regulens_regulatory_monitor
    INTERFACE
        regulens_shared
)


