# Data ingestion library - Production-grade data ingestion components

# Collect all source files (only existing files)
set(DATA_INGESTION_SOURCES
    # Sources will be added as they are implemented
)

# Collect all header files (only existing files)
set(DATA_INGESTION_HEADERS
    # Headers will be added as they are implemented
)

# Create the data ingestion library (interface only if no sources)
if(DATA_INGESTION_SOURCES)
    add_library(regulens_data_ingestion ${DATA_INGESTION_SOURCES} ${DATA_INGESTION_HEADERS})
else()
    add_library(regulens_data_ingestion INTERFACE)
endif()

if(DATA_INGESTION_SOURCES)
    target_include_directories(regulens_data_ingestion
        PUBLIC
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_SOURCE_DIR}/pipelines
            ${CMAKE_CURRENT_SOURCE_DIR}/sources
            ${CMAKE_CURRENT_SOURCE_DIR}/storage
    )

    target_link_libraries(regulens_data_ingestion
        PUBLIC
            regulens_shared
            pqxx
            curl
            ${CMAKE_THREAD_LIBS_INIT}
    )

    # Set compiler features for C++17
    target_compile_features(regulens_data_ingestion PUBLIC cxx_std_17)

    # Add compile definitions for production use
    target_compile_definitions(regulens_data_ingestion
        PRIVATE
            DATA_INGESTION_PRODUCTION_BUILD=1
    )

    # Install targets
    install(TARGETS regulens_data_ingestion
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib
        RUNTIME DESTINATION bin
    )

    install(FILES ${DATA_INGESTION_HEADERS}
        DESTINATION include/regulens/data_ingestion
    )
endif()


