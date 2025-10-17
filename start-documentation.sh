#!/bin/bash

# Regulens Documentation Server Startup Script
# This script starts both the wiki API server and the frontend development server

set -e

echo "🚀 Starting Regulens Documentation System..."
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to check if port is in use
check_port() {
    local port=$1
    if lsof -Pi :$port -sTCP:LISTEN -t >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# Function to kill process on port
kill_port() {
    local port=$1
    echo -e "${YELLOW}Killing process on port $port...${NC}"
    lsof -ti:$port | xargs kill -9 2>/dev/null || true
    sleep 1
}

# Check if node is installed
if ! command -v node &> /dev/null; then
    echo -e "${RED}❌ Node.js is not installed. Please install Node.js first.${NC}"
    exit 1
fi

# Check if npm is installed
if ! command -v npm &> /dev/null; then
    echo -e "${RED}❌ npm is not installed. Please install npm first.${NC}"
    exit 1
fi

echo -e "${BLUE}📦 Checking dependencies...${NC}"

# Check if wiki content exists
if [ ! -d ".qoder/repowiki/en/content" ]; then
    echo -e "${RED}❌ Wiki content not found at .qoder/repowiki/en/content${NC}"
    echo -e "${YELLOW}Please ensure the wiki has been generated.${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Wiki content found${NC}"

# Check if frontend dependencies are installed
if [ ! -d "frontend/node_modules" ]; then
    echo -e "${YELLOW}⚠️  Frontend dependencies not found. Installing...${NC}"
    cd frontend
    npm install
    cd ..
fi

echo -e "${GREEN}✅ Frontend dependencies ready${NC}"

# Check if root dependencies are installed (for wiki server)
if [ ! -d "node_modules" ]; then
    echo -e "${YELLOW}⚠️  Root dependencies not found. Installing...${NC}"
    npm install
fi

echo -e "${GREEN}✅ Root dependencies ready${NC}"

# Check and kill processes on required ports
if check_port 3001; then
    echo -e "${YELLOW}⚠️  Port 3001 is already in use${NC}"
    kill_port 3001
fi

if check_port 3000; then
    echo -e "${YELLOW}⚠️  Port 3000 is already in use${NC}"
    read -p "Do you want to kill the process on port 3000? (y/n) " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        kill_port 3000
    fi
fi

echo ""
echo -e "${BLUE}🚀 Starting Wiki API Server on port 3001...${NC}"

# Start wiki server in background
node wiki-server.js > wiki-server.log 2>&1 &
WIKI_PID=$!

# Wait for wiki server to start
sleep 2

# Check if wiki server is running
if ! check_port 3001; then
    echo -e "${RED}❌ Failed to start wiki server${NC}"
    echo -e "${YELLOW}Check wiki-server.log for details${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Wiki API Server running (PID: $WIKI_PID)${NC}"
echo -e "${GREEN}   http://localhost:3001${NC}"
echo ""

echo -e "${BLUE}🚀 Starting Frontend Development Server...${NC}"

# Start frontend dev server
cd frontend
npm run dev &
FRONTEND_PID=$!
cd ..

# Wait for frontend to start
sleep 3

echo ""
echo -e "${GREEN}✅ Frontend Development Server running (PID: $FRONTEND_PID)${NC}"
echo -e "${GREEN}   http://localhost:3000${NC}"
echo ""

echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo -e "${GREEN}✨ Regulens Documentation System is ready!${NC}"
echo -e "${GREEN}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${NC}"
echo ""
echo -e "${BLUE}📖 Access the documentation at:${NC}"
echo -e "   ${GREEN}http://localhost:3000/documentation${NC}"
echo ""
echo -e "${BLUE}📊 Wiki API endpoints:${NC}"
echo -e "   ${GREEN}http://localhost:3001/api/documentation/structure${NC}"
echo -e "   ${GREEN}http://localhost:3001/api/documentation/content?path=...${NC}"
echo -e "   ${GREEN}http://localhost:3001/api/documentation/search?q=...${NC}"
echo ""
echo -e "${YELLOW}📝 Logs:${NC}"
echo -e "   Wiki Server: ${BLUE}wiki-server.log${NC}"
echo -e "   Frontend: ${BLUE}Console output above${NC}"
echo ""
echo -e "${YELLOW}⚠️  To stop the servers, press Ctrl+C${NC}"
echo ""

# Function to cleanup on exit
cleanup() {
    echo ""
    echo -e "${YELLOW}🛑 Stopping servers...${NC}"
    kill $WIKI_PID 2>/dev/null || true
    kill $FRONTEND_PID 2>/dev/null || true
    echo -e "${GREEN}✅ Servers stopped${NC}"
    exit 0
}

# Trap Ctrl+C
trap cleanup INT TERM

# Keep script running
wait
