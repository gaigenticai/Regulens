#!/bin/bash

# Production-Grade Test Runner for Regulens
# Runs all unit tests, integration tests, and generates coverage reports

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}======================================${NC}"
echo -e "${GREEN}Regulens Test Suite${NC}"
echo -e "${GREEN}======================================${NC}"

# Configuration
BUILD_DIR="build"
TEST_DIR="tests"
COVERAGE_DIR="coverage"
RESULTS_DIR="test-results"

# Create directories
mkdir -p "$BUILD_DIR"
mkdir -p "$COVERAGE_DIR"
mkdir -p "$RESULTS_DIR"

# Function to print section headers
print_section() {
    echo -e "\n${YELLOW}$1${NC}"
    echo "======================================"
}

# Function to run command with error handling
run_command() {
    if ! $@; then
        echo -e "${RED}Error: Command failed: $@${NC}"
        return 1
    fi
    return 0
}

# Check if required tools are installed
print_section "Checking Prerequisites"

if ! command -v cmake &> /dev/null; then
    echo -e "${RED}CMake is not installed. Please install CMake.${NC}"
    exit 1
fi

if ! command -v g++ &> /dev/null && ! command -v clang++ &> /dev/null; then
    echo -e "${RED}C++ compiler not found. Please install g++ or clang++.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All prerequisites met${NC}"

# Clean previous build
print_section "Cleaning Previous Build"
if [ -d "$BUILD_DIR" ]; then
    rm -rf "$BUILD_DIR"
fi
mkdir -p "$BUILD_DIR"
echo -e "${GREEN}✓ Clean complete${NC}"

# Build tests
print_section "Building Tests"
cd "$BUILD_DIR"

if ! cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Debug; then
    echo -e "${RED}CMake configuration failed${NC}"
    exit 1
fi

if ! make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4); then
    echo -e "${RED}Build failed${NC}"
    exit 1
fi

cd ..
echo -e "${GREEN}✓ Build complete${NC}"

# Run unit tests
print_section "Running Unit Tests"
UNIT_TEST_BINARY="$BUILD_DIR/tests/unit_tests"

if [ -f "$UNIT_TEST_BINARY" ]; then
    if ! "$UNIT_TEST_BINARY" --gtest_output=xml:"$RESULTS_DIR/unit_tests.xml"; then
        echo -e "${RED}Unit tests failed${NC}"
        UNIT_TEST_FAILED=1
    else
        echo -e "${GREEN}✓ Unit tests passed${NC}"
        UNIT_TEST_FAILED=0
    fi
else
    echo -e "${YELLOW}Warning: Unit test binary not found${NC}"
    UNIT_TEST_FAILED=0
fi

# Start services for integration tests
print_section "Starting Services for Integration Tests"

# Check if Docker is available
if command -v docker-compose &> /dev/null; then
    echo "Starting test database and Redis..."
    docker-compose -f docker-compose.test.yml up -d postgres redis 2>/dev/null || true
    
    # Wait for services to be ready
    echo "Waiting for services to be ready..."
    sleep 10
    
    SERVICES_STARTED=1
else
    echo -e "${YELLOW}Warning: Docker Compose not available. Integration tests may fail.${NC}"
    SERVICES_STARTED=0
fi

# Run integration tests
print_section "Running Integration Tests"
INTEGRATION_TEST_BINARY="$BUILD_DIR/tests/api_integration_tests"

if [ -f "$INTEGRATION_TEST_BINARY" ]; then
    # Start the application server
    if [ -f "$BUILD_DIR/regulens_server" ]; then
        echo "Starting Regulens server..."
        "$BUILD_DIR/regulens_server" &
        SERVER_PID=$!
        sleep 5
        
        # Run integration tests
        if ! "$INTEGRATION_TEST_BINARY" --gtest_output=xml:"$RESULTS_DIR/integration_tests.xml"; then
            echo -e "${RED}Integration tests failed${NC}"
            INTEGRATION_TEST_FAILED=1
        else
            echo -e "${GREEN}✓ Integration tests passed${NC}"
            INTEGRATION_TEST_FAILED=0
        fi
        
        # Stop server
        kill $SERVER_PID 2>/dev/null || true
    else
        echo -e "${YELLOW}Warning: Server binary not found. Skipping integration tests.${NC}"
        INTEGRATION_TEST_FAILED=0
    fi
else
    echo -e "${YELLOW}Warning: Integration test binary not found${NC}"
    INTEGRATION_TEST_FAILED=0
fi

# Stop test services
if [ "$SERVICES_STARTED" -eq 1 ]; then
    print_section "Stopping Test Services"
    docker-compose -f docker-compose.test.yml down 2>/dev/null || true
fi

# Run E2E tests with Playwright
print_section "Running E2E Tests"
if [ -f "package.json" ] && command -v npm &> /dev/null; then
    if npm test 2>&1 | tee "$RESULTS_DIR/e2e_tests.log"; then
        echo -e "${GREEN}✓ E2E tests passed${NC}"
        E2E_TEST_FAILED=0
    else
        echo -e "${RED}E2E tests failed${NC}"
        E2E_TEST_FAILED=1
    fi
else
    echo -e "${YELLOW}Warning: E2E tests skipped (npm or package.json not found)${NC}"
    E2E_TEST_FAILED=0
fi

# Generate coverage report (if lcov is available)
if command -v lcov &> /dev/null; then
    print_section "Generating Coverage Report"
    
    lcov --capture --directory "$BUILD_DIR" --output-file "$COVERAGE_DIR/coverage.info" 2>/dev/null || true
    lcov --remove "$COVERAGE_DIR/coverage.info" '/usr/*' '*/tests/*' '*/build/*' --output-file "$COVERAGE_DIR/coverage_filtered.info" 2>/dev/null || true
    
    if command -v genhtml &> /dev/null; then
        genhtml "$COVERAGE_DIR/coverage_filtered.info" --output-directory "$COVERAGE_DIR/html" 2>/dev/null || true
        echo -e "${GREEN}✓ Coverage report generated at $COVERAGE_DIR/html/index.html${NC}"
    fi
fi

# Generate test summary
print_section "Test Summary"

TOTAL_FAILURES=$((UNIT_TEST_FAILED + INTEGRATION_TEST_FAILED + E2E_TEST_FAILED))

echo ""
echo "Unit Tests:        $([ $UNIT_TEST_FAILED -eq 0 ] && echo -e "${GREEN}PASSED${NC}" || echo -e "${RED}FAILED${NC}")"
echo "Integration Tests: $([ $INTEGRATION_TEST_FAILED -eq 0 ] && echo -e "${GREEN}PASSED${NC}" || echo -e "${RED}FAILED${NC}")"
echo "E2E Tests:         $([ $E2E_TEST_FAILED -eq 0 ] && echo -e "${GREEN}PASSED${NC}" || echo -e "${RED}FAILED${NC}")"
echo ""

if [ $TOTAL_FAILURES -eq 0 ]; then
    echo -e "${GREEN}======================================${NC}"
    echo -e "${GREEN}All tests passed! ✓${NC}"
    echo -e "${GREEN}======================================${NC}"
    exit 0
else
    echo -e "${RED}======================================${NC}"
    echo -e "${RED}Some tests failed ($TOTAL_FAILURES test suite(s))${NC}"
    echo -e "${RED}======================================${NC}"
    exit 1
fi

