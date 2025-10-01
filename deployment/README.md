# Regulens Deployment Guide

This document explains how build artifacts are handled during deployment and ensures they are properly installed in production environments.

## Build Artifacts Policy

**Build artifacts are intentionally NOT committed to Git.** This is a best practice for:

- **Security**: Prevents accidental exposure of compiled binaries
- **Reproducibility**: Ensures builds are fresh and consistent
- **Size**: Keeps repository small and manageable
- **Compliance**: Follows enterprise security standards

## Deployment Architecture

### Source → Build → Deploy Pipeline

```
Source Code (Git) → CI/CD Build → Artifacts → Deployment
     ↓                    ↓            ↓            ↓
  .gitignore'd       Fresh Build   Stored in      Installed on
  (never committed)  Every Time    Registry       Target System
```

### What Gets Built vs What Gets Committed

#### ✅ Committed to Git (Source Code)
- Header files (`.hpp`)
- Implementation files (`.cpp`)
- CMakeLists.txt files
- Configuration templates (`.env.example`)
- Documentation
- Deployment scripts
- Dockerfiles
- Kubernetes manifests

#### ❌ Not Committed to Git (Build Artifacts)
- Compiled executables (`regulens`, `regulens.exe`)
- Shared libraries (`.so`, `.dll`, `.dylib`)
- Static libraries (`.a`, `.lib`)
- Object files (`.o`, `.obj`)
- CMake cache and generated files
- Test executables
- Coverage reports

## Deployment Methods

### 1. Container Deployment (Recommended)

```dockerfile
# Dockerfile
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    libboost-all-dev \
    git

# Copy source code
WORKDIR /app
COPY . .

# Build application
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    make -j$(nproc) && \
    make install DESTDIR=/install

# Runtime image
FROM ubuntu:22.04 AS runtime

# Install runtime dependencies only
RUN apt-get update && apt-get install -y \
    libboost-system-dev \
    libboost-filesystem-dev \
    ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# Copy built artifacts from builder stage
COPY --from=builder /install /

# Set up application
WORKDIR /app
EXPOSE 8080

CMD ["/usr/local/bin/regulens"]
```

**Benefits:**
- Build artifacts are created during container build
- No binaries stored in Git
- Reproducible builds
- Security scanning on artifacts

### 2. CI/CD Pipeline Deployment

```yaml
# .github/workflows/deploy.yml
name: Deploy to Production

on:
  push:
    branches: [main]

jobs:
  build:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3

      - name: Build Application
        run: |
          mkdir build && cd build
          cmake .. -DCMAKE_BUILD_TYPE=Release
          make -j$(nproc)
          make package  # Creates .deb/.rpm

      - name: Store Artifacts
        uses: actions/upload-artifact@v3
        with:
          name: regulens-${{ github.sha }}
          path: build/*.deb

  deploy:
    needs: build
    runs-on: ubuntu-latest
    steps:
      - name: Download Artifacts
        uses: actions/download-artifact@v3
        with:
          name: regulens-${{ github.sha }}

      - name: Deploy to Production
        run: |
          # Install package on production servers
          sudo dpkg -i regulens-*.deb
          sudo systemctl restart regulens
```

### 3. Package Manager Deployment

```bash
# Build package locally or in CI
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
cpack  # Creates distributable packages

# Install on target system
sudo dpkg -i regulens-1.0.0-Linux.deb
# or
sudo rpm -i regulens-1.0.0-Linux.rpm
```

### 4. Direct Binary Deployment

```bash
# On build server
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/regulens
make -j$(nproc)
make install

# Package binaries
tar -czf regulens-1.0.0.tar.gz /opt/regulens

# Deploy to target server
scp regulens-1.0.0.tar.gz server:/tmp/
ssh server "cd / && tar -xzf /tmp/regulens-1.0.0.tar.gz"
```

## Installation Verification

After deployment, verify installation:

```bash
# Check if binary exists and is executable
ls -la /usr/local/bin/regulens
file /usr/local/bin/regulens

# Test basic functionality
/usr/local/bin/regulens --version
/usr/local/bin/regulens --help

# Check configuration
/usr/local/bin/regulens --validate-only

# Verify service (if using systemd)
sudo systemctl status regulens
sudo systemctl is-enabled regulens
```

## Security Considerations

### Binary Verification
```bash
# Generate checksums during build
sha256sum regulens > regulens.sha256

# Verify on deployment
sha256sum -c regulens.sha256
```

### Secure Distribution
- Use HTTPS for artifact repositories
- Sign packages with GPG keys
- Implement artifact scanning (virus, vulnerabilities)
- Use private artifact repositories for enterprise deployments

## Troubleshooting Deployment

### Common Issues

1. **Missing Dependencies**
   ```bash
   # Check for missing shared libraries
   ldd /usr/local/bin/regulens

   # Install missing dependencies
   sudo apt-get install libboost-system1.74.0 libboost-filesystem1.74.0
   ```

2. **Permission Issues**
   ```bash
   # Ensure proper permissions
   sudo chown regulens:regulens /usr/local/bin/regulens
   sudo chmod 755 /usr/local/bin/regulens
   ```

3. **Configuration Errors**
   ```bash
   # Validate configuration before starting
   sudo -u regulens /usr/local/bin/regulens --validate-only
   ```

4. **Port Conflicts**
   ```bash
   # Check if ports are available
   sudo netstat -tlnp | grep :8080
   ```

### Rollback Procedure

```bash
# Stop service
sudo systemctl stop regulens

# Restore previous version
sudo dpkg -i regulens-1.0.0-previous.deb

# Start service
sudo systemctl start regulens

# Verify
sudo systemctl status regulens
```

## Performance Optimization

### Build Optimizations
```cmake
# In CMakeLists.txt
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native -flto")
set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
```

### Runtime Optimizations
- Use appropriate compiler flags for target architecture
- Enable link-time optimization (LTO)
- Strip debug symbols for production: `strip regulens`
- Use static linking where possible for better performance

## Monitoring Deployment Health

After deployment, monitor:

```bash
# Application logs
tail -f /var/log/regulens/application.log

# System resources
htop -p $(pgrep regulens)

# Network connectivity
netstat -tlnp | grep regulens

# Health check endpoint
curl http://localhost:8080/health
```

## Support and Maintenance

- **Log Rotation**: Configure logrotate for application logs
- **Backup**: Include configuration and data in backup procedures
- **Updates**: Use rolling deployment for zero-downtime updates
- **Monitoring**: Implement comprehensive monitoring and alerting

---

**Remember**: Build artifacts belong in deployment pipelines, not Git repositories. This ensures security, reproducibility, and maintainability of your enterprise application.
