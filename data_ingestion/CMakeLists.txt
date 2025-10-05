# Data ingestion library - Production-grade data ingestion components

# Collect all source files
set(DATA_INGESTION_SOURCES
    data_ingestion_framework.cpp
    ingestion_metrics.cpp
    pipelines/standard_ingestion_pipeline.cpp
    sources/database_source.cpp
    sources/rest_api_source.cpp
    sources/web_scraping_source.cpp
    storage/postgresql_storage_adapter.cpp
)

# Collect all header files
set(DATA_INGESTION_HEADERS
    data_ingestion_framework.hpp
    ingestion_metrics.hpp
    pipelines/standard_ingestion_pipeline.hpp
    sources/database_source.hpp
    sources/rest_api_source.hpp
    sources/web_scraping_source.hpp
    storage/postgresql_storage_adapter.hpp
)

# Create the data ingestion library
add_library(regulens_data_ingestion ${DATA_INGESTION_SOURCES} ${DATA_INGESTION_HEADERS})

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


