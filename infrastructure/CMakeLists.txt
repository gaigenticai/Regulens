# Infrastructure Module CMakeLists.txt
# Kubernetes and deployment infrastructure for enterprise Regulens platform

# Kubernetes Operator Framework
add_subdirectory(k8s/operator)

# Install Kubernetes manifests
install(DIRECTORY k8s/crds/
    DESTINATION share/regulens/k8s/crds
    FILES_MATCHING PATTERN "*.yaml"
)
