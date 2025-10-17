# 🚀 Automated Documentation System Setup

The wiki documentation server now starts **automatically** with the frontend - no separate shell scripts needed!

## ✨ What Changed

### Before (Manual):
```bash
# Terminal 1
npm run wiki-server

# Terminal 2
cd frontend && npm run dev
```

### Now (Automatic):
```bash
# Just run frontend dev - wiki server starts automatically!
cd frontend && npm run dev
```

## 🎯 How It Works

### Development Mode

1. **Single Command Start**:
   ```bash
   cd frontend
   npm install  # First time only
   npm run dev  # Starts BOTH wiki server AND vite dev server
   ```

2. **What Happens**:
   - `concurrently` runs both processes simultaneously
   - Wiki server starts on port 3001
   - Vite dev server starts on port 3000
   - Vite proxy routes `/api/documentation/*` to wiki server
   - Frontend automatically connects to both servers

3. **Access**:
   - Frontend: `http://localhost:3000`
   - Documentation: `http://localhost:3000/documentation`
   - Wiki API: `http://localhost:3001` (proxied via frontend)

### Production Mode (Docker)

1. **Start All Services**:
   ```bash
   docker-compose up
   ```

2. **What Happens**:
   - PostgreSQL starts on port 5432
   - Redis starts on port 6379
   - Wiki server starts on port 3001
   - API server starts on port 8080
   - Frontend starts on port 3000 (nginx)
   - All services are connected via docker network

3. **Access**:
   - Frontend: `http://localhost:3000`
   - Documentation: `http://localhost:3000/documentation`
   - API: `http://localhost:8080`

## 📦 Architecture

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

```
┌─────────────────────────────────────────────────────────┐
│                    Production Mode (Docker)              │
├─────────────────────────────────────────────────────────┤
│                                                          │
│  docker-compose up                                      │
│       │                                                  │
│       ├─→ PostgreSQL (port 5432)                        │
│       ├─→ Redis (port 6379)                             │
│       ├─→ Wiki Server Container (port 3001)             │
│       │   └─→ Node.js serving wiki-server.js            │
│       ├─→ API Server (port 8080)                        │
│       └─→ Frontend Container (port 3000)                │
│           └─→ Nginx proxies:                            │
│               ├─→ /api/documentation/* → wiki-server    │
│               └─→ /api/* → api-server                   │
│                                                          │
│  Browser → http://localhost:3000/documentation          │
│       │                                                  │
│       └─→ Nginx serves React app                        │
│           └─→ API calls proxied to wiki-server          │
│                                                          │
└─────────────────────────────────────────────────────────┘
```

## 🔧 Configuration Files

### Modified Files:

1. **`frontend/package.json`**:
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

2. **`frontend/vite.config.ts`**:
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

3. **`docker-compose.yml`**:
   ```yaml
   services:
     wiki-server:
       image: node:18-alpine
       ports:
         - "3001:3001"
       volumes:
         - ./wiki-server.js:/app/wiki-server.js
         - ./.qoder/repowiki:/app/.qoder/repowiki
     
     frontend:
       depends_on:
         - wiki-server
       environment:
         - VITE_WIKI_API_URL=http://wiki-server:3001
   ```

4. **`frontend/nginx.conf`**:
   ```nginx
   location /api/documentation {
       proxy_pass http://regulens-wiki-server:3001;
   }
   ```

5. **`frontend/src/hooks/useDocumentation.ts`**:
   ```ts
   // Changed from absolute URL to relative
   const WIKI_API_BASE = '/api/documentation';
   ```

## 🚀 Quick Start Guide

### First Time Setup

```bash
# 1. Install root dependencies (for wiki server)
npm install

# 2. Install frontend dependencies
cd frontend
npm install

# 3. Start development (automatic!)
npm run dev
```

### Daily Development

```bash
cd frontend
npm run dev  # That's it! Both servers start
```

### Docker Deployment

```bash
# Copy environment file
cp .env.example .env
# Edit .env with your values

# Start all services
docker-compose up -d

# Check status
docker-compose ps

# View logs
docker-compose logs -f wiki-server
```

## 📊 Service Dependencies

```mermaid
graph TB
    A[npm run dev] --> B[Wiki Server]
    A --> C[Vite Dev Server]
    C --> D[React App]
    D --> E[Documentation Component]
    E --> F[/api/documentation/*]
    F --> G[Vite Proxy]
    G --> B
    B --> H[.qoder/repowiki/]
```

## 🔍 Troubleshooting

### Issue: Port 3001 already in use

**Solution**:
```bash
# Kill process on port 3001
lsof -ti:3001 | xargs kill -9

# Restart
cd frontend && npm run dev
```

### Issue: Wiki content not found

**Solution**:
Ensure wiki content exists:
```bash
ls -la .qoder/repowiki/en/content/
```

### Issue: Concurrently not found

**Solution**:
```bash
cd frontend
npm install concurrently --save-dev
```

### Issue: Cannot connect to wiki server in Docker

**Solution**:
1. Check container is running:
   ```bash
   docker-compose ps
   ```

2. Check logs:
   ```bash
   docker-compose logs wiki-server
   ```

3. Verify network connectivity:
   ```bash
   docker-compose exec frontend ping wiki-server
   ```

## 🎯 Benefits

### ✅ Development:
- ✅ Single command to start everything
- ✅ No separate terminal windows needed
- ✅ Automatic process management
- ✅ Hot reload for both servers
- ✅ Unified logging output

### ✅ Production:
- ✅ All services in docker-compose
- ✅ Automatic service discovery
- ✅ Health checks for all services
- ✅ Proper networking between containers
- ✅ Production-ready nginx configuration

### ✅ Maintenance:
- ✅ No manual script management
- ✅ Consistent behavior across environments
- ✅ Easy to onboard new developers
- ✅ Infrastructure as code

## 📝 Environment Variables

### Development (.env):
```bash
WIKI_PORT=3001
VITE_WIKI_API_URL=http://localhost:3001
```

### Production (docker-compose.yml):
```yaml
environment:
  - WIKI_PORT=3001
  - VITE_WIKI_API_URL=http://wiki-server:3001
```

## 🔄 Migration from Old Setup

### Old Way:
1. Run `./install-documentation.sh`
2. Run `./start-documentation.sh`
3. Keep terminal open

### New Way:
1. Install dependencies once: `cd frontend && npm install`
2. Run: `npm run dev`
3. Everything starts automatically!

### Cleanup (Optional):
You can keep the old scripts for manual testing, or remove them:
```bash
# Optional - old scripts no longer needed
rm install-documentation.sh
rm start-documentation.sh
```

## 📚 Documentation

- Full Guide: [DOCUMENTATION_GUIDE.md](../DOCUMENTATION_GUIDE.md)
- Quick Start: [QUICKSTART.md](../QUICKSTART.md)
- README: [DOCUMENTATION_README.md](../DOCUMENTATION_README.md)

## 🎉 Summary

**Before**: Multiple terminals, manual scripts, complex setup
**Now**: `npm run dev` and you're done! 🚀

Everything runs automatically in both development and production. The documentation system is now a seamless part of the Regulens platform.
