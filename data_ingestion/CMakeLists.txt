# Data ingestion library - TODO: Implement actual data ingestion components

add_library(regulens_data_ingestion INTERFACE)

target_include_directories(regulens_data_ingestion
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(regulens_data_ingestion
    INTERFACE
        regulens_shared
)
