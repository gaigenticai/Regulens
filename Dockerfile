# Regulens Agentic AI Compliance System - Production Docker Image
# Multi-stage build for optimal image size and security

# =============================================================================
# BUILD STAGE - Compile the application
# =============================================================================
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libpq-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    nlohmann-json3-dev \
    pkg-config \
    git \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /app

# Copy source code
COPY . .

# Build the application
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF && \
    make -j$(nproc) regulens

# =============================================================================
# RUNTIME STAGE - Minimal production image
# =============================================================================
FROM ubuntu:22.04 AS runtime

# Install only runtime dependencies
RUN apt-get update && apt-get install -y \
    libpq5 \
    libssl3 \
    libcurl4 \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/* \
    && apt-get clean

# Create non-root user for security
RUN groupadd -r regulens && useradd -r -g regulens regulens

# Set working directory
WORKDIR /app

# Copy compiled binary from builder stage
COPY --from=builder /app/build/regulens .

# Copy configuration templates
COPY --from=builder /app/.env.example .
COPY --from=builder /app/schema.sql .

# Create logs directory with proper permissions
RUN mkdir -p /app/logs && chown -R regulens:regulens /app

# Switch to non-root user
USER regulens

# Expose port for REST API
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --start-period=60s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# Run the application
CMD ["./regulens"]

# =============================================================================
# DEVELOPMENT STAGE - For local development with debugging
# =============================================================================
FROM runtime AS development

# Switch back to root for development setup
USER root

# Install development tools
RUN apt-get update && apt-get install -y \
    gdb \
    valgrind \
    strace \
    tcpdump \
    && rm -rf /var/lib/apt/lists/*

# Switch back to regulens user
USER regulens

# Development command
CMD ["gdb", "./regulens"]

# =============================================================================
# METADATA
# =============================================================================
LABEL org.opencontainers.image.title="Regulens Agentic AI Compliance System" \
      org.opencontainers.image.description="Production-grade agentic AI system for regulatory compliance monitoring" \
      org.opencontainers.image.version="1.0.0" \
      org.opencontainers.image.vendor="Regulens" \
      org.opencontainers.image.maintainer="admin@regulens.ai" \
      org.opencontainers.image.source="https://github.com/regulens/regulens" \
      org.opencontainers.image.licenses="Proprietary"
