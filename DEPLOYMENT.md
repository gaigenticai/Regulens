# Regulens Agentic AI Compliance System - Deployment Guide

## Overview

This document provides comprehensive deployment instructions for the Regulens Agentic AI Compliance System across different environments (local development, staging, production) following @rule.mdc compliance standards.

## Prerequisites

### System Requirements
- **OS**: Linux (Ubuntu 20.04+), macOS (10.15+), or Windows (WSL2)
- **CPU**: 4+ cores recommended for production
- **RAM**: 8GB minimum, 16GB+ recommended
- **Storage**: 50GB+ for database and logs
- **Network**: Stable internet connection for API integrations

### Software Dependencies
- **Docker**: 20.10+ with docker-compose 2.0+
- **PostgreSQL**: 15+ (provided via Docker)
- **Redis**: 7+ (provided via Docker)
- **Git**: For version control

### API Keys (Optional but Recommended)
- **OpenAI API Key**: For LLM-powered features
- **Anthropic API Key**: Alternative LLM provider

## Quick Start (Docker Compose)

### 1. Environment Setup

```bash
# Clone the repository
git clone https://github.com/regulens/regulens.git
cd regulens

# Create environment file
cp .env.example .env

# Edit environment variables (required)
nano .env
```

### 2. Configure Environment Variables

Edit `.env` with your specific values:

```bash
# Database (auto-configured with docker-compose defaults)
DATABASE_PASSWORD=your_strong_password_here

# Redis (auto-configured)
REDIS_PASSWORD=your_redis_password_here

# Optional: LLM API Keys
OPENAI_API_KEY=sk-proj-your-key-here
# ANTHROPIC_API_KEY=your-anthropic-key-here

# Agent Capabilities
AGENT_ENABLE_WEB_SEARCH=true
AGENT_ENABLE_AUTONOMOUS_INTEGRATION=true
AGENT_ENABLE_ADVANCED_DISCOVERY=true

# Security
JWT_SECRET_KEY=your_64_character_jwt_secret_key_here
```

### 3. Deploy with Docker Compose

```bash
# Basic deployment (database + application)
docker-compose up -d

# Full deployment with monitoring
docker-compose --profile monitoring up -d

# Development deployment with debugging tools
docker-compose -f docker-compose.yml -f docker-compose.dev.yml up -d
```

### 4. Verify Deployment

```bash
# Check service health
docker-compose ps

# View logs
docker-compose logs -f regulens

# Test API endpoint
curl http://localhost:8080/health

# Access web interface (when implemented)
open http://localhost:8080
```

## Manual Deployment (Advanced)

### 1. Build from Source

```bash
# Install system dependencies
sudo apt-get update
sudo apt-get install -y build-essential cmake libpq-dev libssl-dev libcurl4-openssl-dev nlohmann-json3-dev pkg-config

# Build the application
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### 2. Database Setup

```bash
# Start PostgreSQL (or use existing instance)
docker run -d \
  --name regulens-postgres \
  -e POSTGRES_DB=regulens_compliance \
  -e POSTGRES_USER=regulens_user \
  -e POSTGRES_PASSWORD=your_password \
  -p 5432:5432 \
  postgres:15-alpine

# Initialize schema
psql -h localhost -U regulens_user -d regulens_compliance -f ../schema.sql
psql -h localhost -U regulens_user -d regulens_compliance -f ../seed_data.sql
```

### 3. Run Application

```bash
# Set environment variables
export DATABASE_HOST=localhost
export DATABASE_PASSWORD=your_password
# ... other required variables

# Run the application
./regulens
```

## Environment Configurations

### Development Environment

```yaml
# docker-compose.dev.yml
version: '3.8'
services:
  regulens:
    build:
      target: development
    environment:
      REGULENS_ENVIRONMENT: development
    volumes:
      - .:/app/src:ro
      - ./logs:/app/logs
```

### Production Environment

```yaml
# docker-compose.prod.yml
version: '3.8'
services:
  regulens:
    deploy:
      replicas: 3
      resources:
        limits:
          cpus: '2.0'
          memory: 4G
        reservations:
          cpus: '1.0'
          memory: 2G
    environment:
      REGULENS_ENVIRONMENT: production
```

## Configuration Reference

### Core Application Settings

| Variable | Required | Default | Description |
|----------|----------|---------|-------------|
| `REGULENS_ENVIRONMENT` | Yes | `production` | Runtime environment |
| `REGULENS_VERSION` | No | `1.0.0` | Application version |
| `REGULENS_INSTANCE_ID` | No | `default` | Instance identifier |
| `DATABASE_HOST` | Yes | `localhost` | PostgreSQL host |
| `DATABASE_PORT` | No | `5432` | PostgreSQL port |
| `DATABASE_NAME` | Yes | `regulens_compliance` | Database name |
| `DATABASE_USER` | Yes | `regulens_user` | Database username |
| `DATABASE_PASSWORD` | Yes | - | Database password |

### Agent Capabilities

| Variable | Default | Description |
|----------|---------|-------------|
| `AGENT_ENABLE_WEB_SEARCH` | `false` | Enable web search integration |
| `AGENT_ENABLE_AUTONOMOUS_INTEGRATION` | `false` | Enable automatic tool integration |
| `AGENT_ENABLE_ADVANCED_DISCOVERY` | `false` | Enable advanced agent discovery |
| `AGENT_ENABLE_WORKFLOW_OPTIMIZATION` | `true` | Enable workflow optimization |
| `AGENT_ENABLE_TOOL_COMPOSITION` | `true` | Enable tool composition |

### Security Settings

| Variable | Required | Description |
|----------|----------|-------------|
| `JWT_SECRET_KEY` | Prod only | JWT signing secret (32+ chars) |
| `DATABASE_SSL_MODE` | No | SSL mode for database connections |

## Monitoring and Observability

### Health Checks

The application provides several health check endpoints:

```bash
# Application health
curl http://localhost:8080/health

# Database connectivity
curl http://localhost:8080/health/database

# Agent status
curl http://localhost:8080/health/agents
```

### Metrics and Monitoring

When using the monitoring profile:

```bash
# Prometheus metrics
open http://localhost:9090

# Grafana dashboards
open http://localhost:3000
```

### Logging

Logs are written to:
- **Application logs**: `/app/logs/regulens.log`
- **Audit logs**: PostgreSQL `audit_logs` table
- **System logs**: Docker container logs

## Troubleshooting

### Common Issues

#### Database Connection Failed
```bash
# Check database container
docker-compose logs postgres

# Test connection manually
psql -h localhost -U regulens_user -d regulens_compliance
```

#### Application Won't Start
```bash
# Check environment validation
docker-compose logs regulens

# Validate configuration
docker-compose exec regulens /app/regulens --validate-config
```

#### High Memory Usage
```yaml
# Adjust container limits in docker-compose.yml
services:
  regulens:
    deploy:
      resources:
        limits:
          memory: 2G
        reservations:
          memory: 1G
```

### Performance Tuning

#### Database Optimization
```sql
-- Enable connection pooling
ALTER SYSTEM SET max_connections = '200';

-- Configure shared buffers
ALTER SYSTEM SET shared_buffers = '256MB';
```

#### Application Tuning
```bash
# Adjust thread pools
export DATABASE_CONNECTION_POOL_SIZE=20

# Enable caching
export REDIS_HOST=redis
```

## Backup and Recovery

### Database Backup
```bash
# Automated backup script
docker-compose exec postgres pg_dump -U regulens_user regulens_compliance > backup_$(date +%Y%m%d_%H%M%S).sql
```

### Application Backup
```bash
# Backup configuration and logs
tar -czf backup_$(date +%Y%m%d_%H%M%S).tar.gz .env logs/ config/
```

## Security Considerations

### Production Deployment Checklist

- [ ] Change all default passwords
- [ ] Enable SSL/TLS for all connections
- [ ] Configure firewall rules
- [ ] Set up log aggregation
- [ ] Enable monitoring and alerting
- [ ] Configure backup procedures
- [ ] Review and test disaster recovery

### Security Headers

The application includes security headers:
- Content Security Policy (CSP)
- HTTP Strict Transport Security (HSTS)
- X-Frame-Options
- X-Content-Type-Options

## Support and Maintenance

### Regular Maintenance Tasks

1. **Database Maintenance**
   ```sql
   -- Vacuum and analyze tables weekly
   VACUUM ANALYZE;

   -- Reindex tables monthly
   REINDEX DATABASE regulens_compliance;
   ```

2. **Log Rotation**
   ```bash
   # Rotate logs weekly
   logrotate /etc/logrotate.d/regulens
   ```

3. **Security Updates**
   ```bash
   # Update containers monthly
   docker-compose pull
   docker-compose up -d
   ```

### Monitoring Alerts

Set up alerts for:
- Application downtime
- Database connection failures
- High memory/CPU usage
- Failed API integrations
- Security violations

## Version History

- **v1.0.0**: Initial production release
  - Core agentic AI functionality
  - Regulatory monitoring
  - REST API
  - Docker deployment

---

For additional support, please refer to the project documentation or create an issue in the GitHub repository.
