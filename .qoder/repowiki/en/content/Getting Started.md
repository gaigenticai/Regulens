# Getting Started

<cite>
**Referenced Files in This Document**   
- [docker-compose.yml](file://docker-compose.yml) - *Updated in recent commit*
- [start.sh](file://start.sh)
- [schema.sql](file://schema.sql)
- [seed_data.sql](file://seed_data.sql)
- [package.json](file://package.json)
- [frontend/package.json](file://frontend/package.json) - *Updated in recent commit*
- [CMakeLists.txt](file://CMakeLists.txt)
- [Dockerfile](file://Dockerfile)
- [main.cpp](file://main.cpp)
- [server_with_auth.cpp](file://server_with_auth.cpp)
- [AUTOMATED_SETUP.md](file://AUTOMATED_SETUP.md) - *Updated in recent commit*
- [wiki-server.js](file://wiki-server.js) - *Updated in recent commit*
- [frontend/vite.config.ts](file://frontend/vite.config.ts) - *Updated in recent commit*
- [frontend/src/hooks/useDocumentation.ts](file://frontend/src/hooks/useDocumentation.ts) - *Updated in recent commit*
- [install-documentation.sh](file://install-documentation.sh) - *Deprecated in recent commit*
- [start-documentation.sh](file://start-documentation.sh) - *Deprecated in recent commit*
</cite>

## Update Summary
**Changes Made**   
- Updated installation methods to reflect automated documentation system
- Removed outdated manual documentation setup instructions
- Added new section on automated documentation system
- Updated configuration setup to include wiki server settings
- Revised running instructions to reflect new startup process
- Updated troubleshooting section with new common issues
- Marked deprecated scripts as obsolete

## Table of Contents
1. [Prerequisites](#prerequisites)
2. [Installation Methods](#installation-methods)
3. [Configuration Setup](#configuration-setup)
4. [Running Regulens](#running-regulens)
5. [Initial System Access](#initial-system-access)
6. [Verification and Testing](#verification-and-testing)
7. [Troubleshooting](#troubleshooting)
8. [Platform-Specific Considerations](#platform-specific-considerations)
9. [Automated Documentation System](#automated-documentation-system)

## Prerequisites

Before installing Regulens, ensure your system meets the following requirements:

### Core Dependencies
- **C++ Toolchain**: GCC 9+ or Clang 10+ with C++20 support
- **Node.js**: Version 18 or higher
- **PostgreSQL**: Version 15+ with pgvector extension
- **Redis**: Version 7+
- **CMake**: Version 3.20+
- **Build Essentials**: make, build-essential (Linux), Xcode Command Line Tools (macOS)

### Environment Variables
Regulens requires several environment variables to be set. The system will fail to start if critical variables are missing:

```bash
# Database Configuration
export DB_HOST=localhost
export DB_PORT=5432
export DB_NAME=regulens_compliance
export DB_USER=regulens_user
export DB_PASSWORD=your_secure_password

# Redis Configuration
export REDIS_HOST=localhost
export REDIS_PORT=6379
export REDIS_PASSWORD=your_secure_redis_password

# Security Configuration
export JWT_SECRET_KEY=your_32_character_jwt_secret_key
export DATA_ENCRYPTION_KEY=64_character_hex_key_for_data_encryption

# System Configuration
export WEB_SERVER_PORT=8080
export CORS_ALLOWED_ORIGIN=http://localhost:3000
export JWT_EXPIRATION_HOURS=24
```

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L15-L35)
- [start.sh](file://start.sh#L25-L75)

## Installation Methods

### Docker Installation (Recommended)

The Docker installation method is recommended for both development and production environments as it ensures consistent dependencies and configuration.

#### Development Environment
```bash
# Clone the repository
git clone https://github.com/gaigenticai/Regulens.git
cd Regulens

# Set required environment variables
export DB_PASSWORD=$(openssl rand -base64 32)
export REDIS_PASSWORD=$(openssl rand -base64 32)
export JWT_SECRET_KEY=$(openssl rand -hex 32)

# Start the development environment
docker-compose up -d
```

#### Production Environment
For production deployment, create a `.env` file with your configuration:

```bash
# .env file for production
DB_HOST=postgres.prod.internal
DB_PORT=5432
DB_NAME=regulens_production
DB_USER=regulens_prod_user
DB_PASSWORD=your_production_db_password
REDIS_HOST=redis.prod.internal
REDIS_PASSWORD=your_production_redis_password
JWT_SECRET_KEY=your_production_jwt_secret
DATA_ENCRYPTION_KEY=your_production_encryption_key
```

Then deploy using Docker Compose:
```bash
docker-compose up -d --scale api-server=3
```

### Native Build Installation

For native installation, you'll need to compile the C++ backend and install Node.js dependencies.

#### Linux/macOS
```bash
# Install system dependencies
# Ubuntu/Debian
sudo apt-get update && sudo apt-get install build-essential cmake libpq-dev libpqxx-dev libssl-dev libcurl4-openssl-dev nlohmann-json3-dev libspdlog-dev

# macOS with Homebrew
brew install cmake postgresql libpqxx openssl spdlog

# Install Node.js dependencies
npm install
cd frontend && npm install && cd ..

# Build the C++ backend
mkdir build && cd build
cmake ..
make -j$(nproc)

# The binary will be created as ./regulens
```

#### Windows (WSL2 Recommended)
```bash
# In WSL2 Ubuntu
sudo apt-get update && sudo apt-get install build-essential cmake libpq-dev libpqxx-dev libssl-dev libcurl4-openssl-dev nlohmann-json3-dev libspdlog-dev

# Install Node.js from https://nodejs.org
npm install
cd frontend && npm install && cd ..

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)
```

**Section sources**
- [CMakeLists.txt](file://CMakeLists.txt#L1-L233)
- [package.json](file://package.json)
- [frontend/package.json](file://frontend/package.json)
- [Dockerfile](file://Dockerfile#L1-L122)

## Configuration Setup

### Environment Configuration
Regulens uses environment variables for configuration. Create a `.env` file in the project root:

```bash
# Database Configuration
DB_HOST=localhost
DB_PORT=5432
DB_NAME=regulens_compliance
DB_USER=regulens_user
DB_PASSWORD=your_secure_password

# Redis Configuration
REDIS_HOST=localhost
REDIS_PORT=6379
REDIS_PASSWORD=your_secure_redis_password

# Security
JWT_SECRET_KEY=your_32_character_jwt_secret_key_here
DATA_ENCRYPTION_KEY=64_character_hex_key_for_data_encryption_here

# System Settings
WEB_SERVER_PORT=8080
CORS_ALLOWED_ORIGIN=http://localhost:3000
SESSION_EXPIRY_HOURS=24
```

### Database Initialization
The database schema and seed data are automatically loaded when using Docker:

```bash
# Schema file location
schema.sql

# Seed data file location
seed_data.sql
```

For manual database setup:
```bash
# Create database
createdb regulens_compliance

# Apply schema
psql -d regulens_compliance -f schema.sql

# Seed data
psql -d regulens_compliance -f seed_data.sql
```

### Configuration Validation
The system validates configuration at startup. Critical requirements:
- `DB_PASSWORD`, `REDIS_PASSWORD`, and `JWT_SECRET_KEY` must be set
- `JWT_SECRET_KEY` must be at least 32 characters
- Database credentials must be valid and accessible

**Section sources**
- [schema.sql](file://schema.sql#L1-L799)
- [seed_data.sql](file://seed_data.sql#L1-L799)
- [start.sh](file://start.sh#L25-L75)

## Running Regulens

### Using Docker Compose
```bash
# Start all services
docker-compose up -d

# Check service status
docker-compose ps

# View logs
docker-compose logs -f
```

### Using the Start Script (Development)
```bash
# Make the script executable
chmod +x start.sh

# Run the development environment
./start.sh
```

The start script will:
1. Check for required environment variables
2. Start PostgreSQL and Redis containers if not running
3. Compile the backend if needed
4. Start the backend and frontend services

### Manual Execution
```bash
# Start databases
docker-compose up -d postgres redis

# Build and run backend
make clean && make
./build/regulens &

# Start frontend
cd frontend
npm run dev
```

### Stopping Regulens
```bash
# Using Docker Compose
docker-compose down

# Using the start script cleanup
pkill -f regulens
```

**Section sources**
- [start.sh](file://start.sh#L0-L194)
- [docker-compose.yml](file://docker-compose.yml#L1-L141)
- [main.cpp](file://main.cpp#L0-L410)

## Initial System Access

### Frontend Access
Once the system is running, access the frontend at:
```
http://localhost:3000
```

Default credentials:
- Username: `admin`
- Password: `Admin123`

### API Access
The API server runs on port 8080:

```bash
# Health check
curl http://localhost:8080/health

# Login to obtain JWT token
curl -X POST http://localhost:8080/api/auth/login \
  -H "Content-Type: application/json" \
  -d '{"username":"admin","password":"Admin123"}'

# Access protected endpoint with token
curl http://localhost:8080/api/decisions \
  -H "Authorization: Bearer YOUR_JWT_TOKEN"
```

### System Status
Check system status through the health endpoint:
```bash
curl http://localhost:8080/health
```

Expected response:
```json
{
  "status": "healthy",
  "service": "regulatory-monitor",
  "version": "1.0.0",
  "timestamp": 1703123456,
  "uptime_seconds": 120,
  "monitoring_active": true
}
```

**Section sources**
- [server_with_auth.cpp](file://server_with_auth.cpp#L0-L799)
- [backend-dev-server.js](file://backend-dev-server.js#L0-L201)

## Verification and Testing

### Verification Steps
After installation, verify the system is working correctly:

```bash
# 1. Check if containers are running
docker ps | grep regulens

# 2. Verify database connectivity
docker exec regulens-postgres pg_isready -U $DB_USER -d $DB_NAME

# 3. Test API health endpoint
curl -f http://localhost:8080/health

# 4. Check frontend accessibility
curl -f http://localhost:3000

# 5. Verify database schema
docker exec regulens-postgres psql -U $DB_USER -d $DB_NAME -c "\dt" | grep compliance_events
```

### UI Navigation
1. Open browser to `http://localhost:3000`
2. Log in with admin credentials
3. Navigate to Dashboard to view system status
4. Check Agent Status page to verify all agents are active
5. View Recent Decisions to confirm decision engine is working

### Automated Testing
Run the test suite:
```bash
# Run end-to-end tests
npm run test

# Run backend tests
cd tests && ./run_all_tests.sh
```

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L37-L45)
- [start.sh](file://start.sh#L150-L194)
- [package.json](file://package.json#L10-L18)

## Troubleshooting

### Common Issues

#### Port Conflicts
If ports 5432, 6379, or 8080 are in use:
```bash
# Check for conflicting processes
lsof -i :5432
lsof -i :6379
lsof -i :8080

# Stop conflicting processes or change ports in docker-compose.yml
# Modify port mappings in docker-compose.yml:
#   ports:
#     - "5433:5432"  # Map host port 5433 to container 5432
```

#### Dependency Installation Issues
```bash
# On Linux, ensure all build dependencies are installed
sudo apt-get install build-essential cmake libpq-dev libpqxx-dev libssl-dev

# If CMake can't find packages, specify paths
export POSTGRESQL_ROOT=/usr/include/postgresql
export PQXX_ROOT=/usr/include
```

#### Database Connectivity Problems
```bash
# Verify environment variables are set
echo $DB_HOST $DB_USER $DB_PASSWORD

# Test database connection manually
psql -h $DB_HOST -U $DB_USER -d $DB_NAME

# Check PostgreSQL container logs
docker logs regulens-postgres

# Recreate volumes if database is corrupted
docker-compose down -v
docker-compose up -d
```

#### Build Failures
```bash
# Clean and rebuild
make clean
rm -rf build
mkdir build && cd build
cmake ..
make

# Check for missing dependencies
cmake .. | grep -i "not found"
```

### Debugging Tips
- Check logs in the `logs/` directory
- Use `docker-compose logs` to view container output
- Enable verbose logging by setting `LOG_LEVEL=DEBUG`
- For C++ debugging, use `gdb ./build/regulens`

**Section sources**
- [start.sh](file://start.sh#L77-L149)
- [docker-compose.yml](file://docker-compose.yml#L46-L65)
- [CMakeLists.txt](file://CMakeLists.txt#L1-L233)

## Platform-Specific Considerations

### macOS
- Install dependencies using Homebrew: `brew install cmake postgresql libpqxx openssl spdlog`
- Ensure Xcode Command Line Tools are installed: `xcode-select --install`
- Docker Desktop must be running

### Linux
- Use system package manager to install dependencies
- Ensure user has permission to run Docker commands
- Consider using systemd services for production deployment

### Windows
- Use WSL2 (Windows Subsystem for Linux) for best compatibility
- Install Ubuntu from Microsoft Store
- Install Docker Desktop for Windows with WSL2 backend
- Follow Linux installation instructions within WSL2

### Production Considerations
- Use dedicated PostgreSQL and Redis instances instead of containers
- Implement proper SSL/TLS configuration
- Set up monitoring and alerting
- Configure regular backups
- Use environment-specific configuration files

**Section sources**
- [docker-compose.yml](file://docker-compose.yml#L1-L141)
- [start.sh](file://start.sh#L0-L194)
- [CMakeLists.txt](file://CMakeLists.txt#L1-L233)

## Automated Documentation System

The Regulens documentation system has been completely automated, eliminating the need for separate manual setup scripts. The wiki server now starts automatically with the frontend.

### Key Changes
- **Old Process**: Required running `install-documentation.sh` and `start-documentation.sh` separately
- **New Process**: Documentation server starts automatically with `npm run dev`
- **Benefits**: Single command startup, automatic process management, unified logging

### Development Mode Setup
```bash
# 1. Install root dependencies (for wiki server)
npm install

# 2. Install frontend dependencies
cd frontend
npm install

# 3. Start development (automatic!)
npm run dev  # Starts both wiki server and Vite dev server
```

### Architecture Overview
```
┌─────────────────────────────────────────────────────────┐
│                    Development Mode                      │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  npm run dev (frontend/)                                │
│       │                                                  │
│       ├─→ Wiki Server (port 3001)                       │
│       │   └─→ Serves wiki content from .qoder/repowiki/ │
│       │                                                  │
│       └─→ Vite Dev Server (port 3000)                   │
│           ├─→ Frontend React app                        │
│           └─→ Proxy /api/documentation/* → port 3001    │
│                                                          │
│  Browser → http://localhost:3000/documentation          │
│       │                                                  │
│       └─→ Frontend loads Documentation component        │
│           └─→ Fetches from /api/documentation/*         │
│               └─→ Vite proxy forwards to wiki server    │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

### Configuration Files
The automated system uses the following configuration files:

**`frontend/package.json`**:
```json
{
  "scripts": {
    "dev": "concurrently \"npm run wiki-server\" \"vite\"",
    "wiki-server": "cd .. && node wiki-server.js"
  },
  "devDependencies": {
    "concurrently": "^8.2.2"
  }
}
```

**`frontend/vite.config.ts`**:
```ts
proxy: {
  '/api/documentation': {
    target: 'http://localhost:3001',
    changeOrigin: true,
  },
  '/api': {
    target: 'http://localhost:8080',
    changeOrigin: true,
  }
}
```

### Accessing Documentation
Once the system is running:
- **Frontend**: `http://localhost:3000`
- **Documentation**: `http://localhost:3000/documentation`
- **Wiki API**: `http://localhost:3001` (proxied via frontend)

### Migration from Old Setup
The old manual setup scripts are now deprecated:
- `install-documentation.sh` - No longer needed
- `start-documentation.sh` - No longer needed

**New Workflow**:
1. Install dependencies once: `cd frontend && npm install`
2. Run: `npm run dev`
3. Everything starts automatically!

### Troubleshooting Automated Setup

#### Issue: Port 3001 already in use
```bash
# Kill process on port 3001
lsof -ti:3001 | xargs kill -9

# Restart
cd frontend && npm run dev
```

#### Issue: Wiki content not found
Ensure wiki content exists:
```bash
ls -la .qoder/repowiki/en/content/
```

#### Issue: Concurrently not found
```bash
cd frontend
npm install concurrently --save-dev
```

**Section sources**
- [AUTOMATED_SETUP.md](file://AUTOMATED_SETUP.md) - *Updated in recent commit*
- [frontend/package.json](file://frontend/package.json#L5-L10) - *Updated in recent commit*
- [frontend/vite.config.ts](file://frontend/vite.config.ts#L20-L30) - *Updated in recent commit*
- [frontend/src/hooks/useDocumentation.ts](file://frontend/src/hooks/useDocumentation.ts#L5) - *Updated in recent commit*
- [docker-compose.yml](file://docker-compose.yml#L100-L120) - *Updated in recent commit*
- [install-documentation.sh](file://install-documentation.sh) - *Deprecated in recent commit*
- [start-documentation.sh](file://start-documentation.sh) - *Deprecated in recent commit*
- [wiki-server.js](file://wiki-server.js) - *Updated in recent commit*