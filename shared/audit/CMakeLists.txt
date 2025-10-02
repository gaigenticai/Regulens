# Audit and compliance library

add_library(regulens_audit STATIC
    decision_audit_trail.cpp
)

target_include_directories(regulens_audit
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(regulens_audit
    PUBLIC
        regulens_shared
        Threads::Threads
)
