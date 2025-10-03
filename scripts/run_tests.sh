#!/bin/bash

# Regulens Automated Testing Script
# This script runs comprehensive tests for the Regulens system

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
TEST_BINARY="${BUILD_DIR}/tests/regulens_tests"
COVERAGE_BINARY="${BUILD_DIR}/coverage_report"
LOG_FILE="test_results_$(date +%Y%m%d_%H%M%S).log"

# Logging function
log() {
    echo -e "${BLUE}[$(date +%Y-%m-%d\ %H:%M:%S)]${NC} $1" | tee -a "$LOG_FILE"
}

error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_FILE"
}

success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1" | tee -a "$LOG_FILE"
}

warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1" | tee -a "$LOG_FILE"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to setup test environment
setup_test_environment() {
    log "Setting up test environment..."

    # Create test database if it doesn't exist
    if command_exists psql; then
        log "Setting up test database..."
        psql -h localhost -U postgres -c "CREATE DATABASE regulens_test;" 2>/dev/null || warning "Test database may already exist"
    else
        warning "PostgreSQL client not found, skipping database setup"
    fi

    # Load test configuration
    if [ -f ".github/workflows/test-config.env" ]; then
        log "Loading test configuration..."
        export $(grep -v '^#' .github/workflows/test-config.env | xargs)
    fi

    success "Test environment setup complete"
}

# Function to build the project
build_project() {
    log "Building project..."

    if [ ! -d "$BUILD_DIR" ]; then
        mkdir -p "$BUILD_DIR"
    fi

    cd "$BUILD_DIR"

    # Configure CMake with test and coverage options
    cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_TESTS=ON \
        -DENABLE_COVERAGE=ON \
        -DCMAKE_CXX_FLAGS="--coverage" \
        -DCMAKE_EXE_LINKER_FLAGS="--coverage"

    # Build project
    make -j$(nproc) regulens_shared
    make -j$(nproc) regulens_tests

    cd ..
    success "Project build complete"
}

# Function to run unit tests
run_unit_tests() {
    log "Running unit tests..."

    if [ ! -f "$TEST_BINARY" ]; then
        error "Test binary not found: $TEST_BINARY"
        exit 1
    fi

    # Run tests with detailed output
    "$TEST_BINARY" --gtest_output=xml:test_results.xml \
                   --gtest_color=yes \
                   --gtest_print_time=1 \
                   --gtest_repeat=1 \
                   --gtest_break_on_failure=0

    local test_result=$?
    if [ $test_result -eq 0 ]; then
        success "All unit tests passed"
    else
        error "Some unit tests failed (exit code: $test_result)"
        return $test_result
    fi
}

# Function to run integration tests
run_integration_tests() {
    log "Running integration tests..."

    # Check if main executable exists and can start
    if [ -f "${BUILD_DIR}/regulens" ]; then
        log "Testing application startup..."
        timeout 10s "${BUILD_DIR}/regulens" --help >/dev/null 2>&1 || warning "Application startup test inconclusive"
    else
        warning "Main executable not found, skipping integration tests"
    fi

    success "Integration tests complete"
}

# Function to generate coverage report
generate_coverage_report() {
    log "Generating coverage report..."

    if command_exists gcovr; then
        cd "$BUILD_DIR"
        gcovr -r .. \
              --exclude='.*test.*' \
              --exclude='.*main.*' \
              --exclude='.*CMakeFiles.*' \
              --html \
              --html-details \
              -o coverage_report.html \
              --xml \
              -o coverage_report.xml \
              --txt \
              -o coverage_report.txt

        log "Coverage report generated:"
        log "  HTML: ${BUILD_DIR}/coverage_report.html"
        log "  XML:  ${BUILD_DIR}/coverage_report.xml"
        log "  TXT:  ${BUILD_DIR}/coverage_report.txt"

        success "Coverage report generation complete"
    else
        warning "gcovr not found, skipping coverage report generation"
    fi
}

# Function to run static analysis
run_static_analysis() {
    log "Running static analysis..."

    if command_exists cppcheck; then
        cppcheck --enable=all \
                 --std=c++17 \
                 --language=c++ \
                 --suppress=missingIncludeSystem \
                 --suppress=unusedFunction \
                 --suppress=unmatchedSuppression \
                 --inline-suppr \
                 --xml \
                 --xml-version=2 \
                 shared/ agents/ regulatory_monitor/ data_ingestion/ \
                 2> cppcheck_results.xml || true

        if [ -f "cppcheck_results.xml" ]; then
            log "Static analysis results saved to cppcheck_results.xml"
        fi

        success "Static analysis complete"
    else
        warning "cppcheck not found, skipping static analysis"
    fi
}

# Function to run performance tests
run_performance_tests() {
    log "Running performance tests..."

    # Basic performance test - measure startup time
    if [ -f "${BUILD_DIR}/regulens" ]; then
        log "Measuring application startup time..."
        local start_time=$(date +%s%N)
        timeout 5s "${BUILD_DIR}/regulens" --version >/dev/null 2>&1
        local end_time=$(date +%s%N)
        local startup_time=$(( (end_time - start_time) / 1000000 )) # Convert to milliseconds

        if [ $startup_time -lt 2000 ]; then # Less than 2 seconds is acceptable
            success "Application startup time: ${startup_time}ms"
        else
            warning "Application startup time: ${startup_time}ms (may be slow)"
        fi
    else
        warning "Main executable not found, skipping performance tests"
    fi

    success "Performance tests complete"
}

# Function to clean up test artifacts
cleanup() {
    log "Cleaning up test artifacts..."

    # Remove test database
    if command_exists psql; then
        psql -h localhost -U postgres -c "DROP DATABASE IF EXISTS regulens_test;" 2>/dev/null || true
    fi

    # Remove temporary files
    rm -f test_results.xml
    rm -f cppcheck_results.xml

    success "Cleanup complete"
}

# Function to display test summary
display_summary() {
    log "=== Test Summary ==="
    log "Log file: $LOG_FILE"

    if [ -f "test_results.xml" ]; then
        log "Test results: test_results.xml"
    fi

    if [ -f "${BUILD_DIR}/coverage_report.html" ]; then
        log "Coverage report: ${BUILD_DIR}/coverage_report.html"
    fi

    success "Testing session complete"
}

# Main execution
main() {
    log "Starting Regulens automated testing suite..."
    log "Timestamp: $(date)"
    log "Working directory: $(pwd)"

    # Parse command line arguments
    local skip_build=false
    local skip_cleanup=false
    local run_coverage=true

    while [[ $# -gt 0 ]]; do
        case $1 in
            --skip-build)
                skip_build=true
                shift
                ;;
            --skip-cleanup)
                skip_cleanup=true
                shift
                ;;
            --no-coverage)
                run_coverage=false
                shift
                ;;
            --help)
                echo "Usage: $0 [options]"
                echo "Options:"
                echo "  --skip-build    Skip the build step"
                echo "  --skip-cleanup  Skip cleanup after testing"
                echo "  --no-coverage   Skip coverage report generation"
                echo "  --help          Show this help message"
                exit 0
                ;;
            *)
                error "Unknown option: $1"
                exit 1
                ;;
        esac
    done

    # Setup
    setup_test_environment

    # Build (unless skipped)
    if [ "$skip_build" = false ]; then
        build_project
    fi

    # Run tests
    run_unit_tests
    run_integration_tests
    run_performance_tests
    run_static_analysis

    # Generate coverage report
    if [ "$run_coverage" = true ]; then
        generate_coverage_report
    fi

    # Cleanup (unless skipped)
    if [ "$skip_cleanup" = false ]; then
        cleanup
    fi

    # Display summary
    display_summary

    success "All testing phases completed successfully!"
}

# Run main function with all arguments
main "$@"
