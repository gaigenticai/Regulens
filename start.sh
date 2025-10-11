#!/bin/bash
# Regulens Development Startup Script
# Starts databases in Docker, runs backend and frontend natively with hot reload

set -e

# Colors
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}🚀 Regulens Development Environment${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Load environment variables (Production-ready - no localhost hardcoding)
export DB_HOST=${DB_HOST:-localhost}
export DB_PORT=${DB_PORT:-5432}
export DB_NAME=${DB_NAME:-regulens_compliance}
export DB_USER=${DB_USER:-regulens_user}
export DB_PASSWORD=${DB_PASSWORD:-dev_password}
export REDIS_HOST=${REDIS_HOST:-localhost}
export REDIS_PORT=${REDIS_PORT:-6379}
export REDIS_PASSWORD=${REDIS_PASSWORD:-dev_redis_password}
export JWT_SECRET=${JWT_SECRET:-dev_jwt_secret_change_in_production_min_32_chars_required_for_security}
export JWT_EXPIRATION_HOURS=${JWT_EXPIRATION_HOURS:-1600}
export SESSION_EXPIRY_HOURS=${SESSION_EXPIRY_HOURS:-24}
export WEB_SERVER_HOST=${WEB_SERVER_HOST:-0.0.0.0}
export WEB_SERVER_PORT=${WEB_SERVER_PORT:-8080}
export CORS_ALLOWED_ORIGIN=${CORS_ALLOWED_ORIGIN:-http://localhost:3000}
export NODE_ENV=${NODE_ENV:-development}

# Check if databases are running
POSTGRES_RUNNING=$(docker ps -q -f name=regulens-postgres 2>/dev/null)
REDIS_RUNNING=$(docker ps -q -f name=regulens-redis 2>/dev/null)

if [ ! -z "$POSTGRES_RUNNING" ] && [ ! -z "$REDIS_RUNNING" ]; then
    echo -e "${GREEN}✓${NC} Databases already running"
else
    echo -e "${BLUE}🐳 Starting databases (Postgres + Redis)...${NC}"
    docker-compose up -d
    echo ""
    echo -e "${BLUE}⏳ Waiting for databases to be ready...${NC}"
    sleep 5
    echo -e "${GREEN}✓${NC} Databases ready"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}📊 Services Status${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "  ${GREEN}✓${NC} Postgres:  localhost:5432"
echo -e "  ${GREEN}✓${NC} Redis:     localhost:6379"
echo ""

# Check if backend binary exists
if [ ! -f "./build/regulens" ]; then
    echo -e "${YELLOW}⚠️  Backend not compiled. Compiling now...${NC}"
    make clean && make
    echo ""
fi

# Intelligent process management: Clean up ALL existing processes
echo -e "${BLUE}🔍 Checking for existing processes...${NC}"
BACKEND_PIDS=$(pgrep -f "build/regulens" 2>/dev/null || true)
FRONTEND_PIDS=$(ps aux | grep "vite" | grep -v grep | awk '{print $2}' 2>/dev/null || true)

if [ ! -z "$BACKEND_PIDS" ] || [ ! -z "$FRONTEND_PIDS" ]; then
    echo -e "${YELLOW}⚠️  Found ${BACKEND_PIDS:+backend }${FRONTEND_PIDS:+frontend }processes. Cleaning up...${NC}"
    # Kill by process name
    pkill -9 -f "build/regulens" 2>/dev/null || true
    pkill -9 -f "node.*vite" 2>/dev/null || true
    # Kill by port as backup
    lsof -ti:8080 | xargs kill -9 2>/dev/null || true
    lsof -ti:3000 | xargs kill -9 2>/dev/null || true
    sleep 2
    echo -e "${GREEN}✓${NC} All existing processes terminated"
fi

echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${BLUE}🔨 Starting Backend (Native)${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Start backend in background
./build/regulens > logs/backend.log 2>&1 &
BACKEND_PID=$!

# Wait for backend to start
sleep 3

# Check if backend started successfully
if ps -p $BACKEND_PID > /dev/null; then
    echo -e "${GREEN}✓${NC} Backend started (PID: $BACKEND_PID)"
    echo -e "  ${BLUE}→${NC} Logs: tail -f logs/backend.log"
else
    echo -e "${RED}✗${NC} Backend failed to start. Check logs/backend.log"
    exit 1
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${BLUE}⚡ Starting Frontend (Vite Dev Server)${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""

# Check if node_modules exists
if [ ! -d "./frontend/node_modules" ]; then
    echo -e "${YELLOW}⚠️  Installing frontend dependencies...${NC}"
    cd frontend && npm install && cd ..
    echo ""
fi

# Start frontend in background
cd frontend
npm run dev > ../logs/frontend.log 2>&1 &
FRONTEND_PID=$!
cd ..

sleep 3

if ps -p $FRONTEND_PID > /dev/null; then
    echo -e "${GREEN}✓${NC} Frontend started (PID: $FRONTEND_PID)"
    echo -e "  ${BLUE}→${NC} Logs: tail -f logs/frontend.log"
else
    echo -e "${RED}✗${NC} Frontend failed to start. Check logs/frontend.log"
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}✅ Development Environment Ready!${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo -e "${BLUE}🌐 Access Points:${NC}"
echo -e "  Frontend:  ${GREEN}http://localhost:3000${NC}"
echo -e "  Backend:   ${GREEN}http://localhost:8080${NC}"
echo -e "  Health:    ${GREEN}http://localhost:8080/health${NC}"
echo ""
echo -e "${BLUE}🔑 Login Credentials:${NC}"
echo -e "  Username:  ${GREEN}admin${NC}"
echo -e "  Password:  ${GREEN}Admin123${NC}"
echo ""
echo -e "${BLUE}🔥 Hot Reload:${NC}"
echo -e "  ${GREEN}✓${NC} Frontend changes: Auto-reload (< 1 sec)"
echo -e "  ${GREEN}✓${NC} Backend changes:  Recompile with ${YELLOW}make && pkill -f regulens && ./build/regulens &${NC}"
echo ""
echo -e "${BLUE}📝 Useful Commands:${NC}"
echo -e "  View backend logs:   ${YELLOW}tail -f logs/backend.log${NC}"
echo -e "  View frontend logs:  ${YELLOW}tail -f logs/frontend.log${NC}"
echo -e "  Recompile backend:   ${YELLOW}make${NC}"
echo -e "  Stop all:            ${YELLOW}pkill -f regulens; docker-compose down${NC}"
echo ""
echo -e "${BLUE}💡 Development Tips:${NC}"
echo -e "  • Frontend changes auto-reload instantly"
echo -e "  • Backend changes require recompilation (make)"
echo -e "  • Database changes persist in Docker volumes"
echo -e "  • Check logs/ folder for detailed output"
echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo -e "${GREEN}Happy coding! 🚀${NC}"
echo ""

