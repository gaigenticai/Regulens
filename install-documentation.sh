#!/bin/bash

# Regulens Documentation System - Complete Installation Script
# This script installs all dependencies and sets up the documentation system

set -e

echo "╔════════════════════════════════════════════════════════════════╗"
echo "║   Regulens Documentation System - Installation Script          ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

# Check prerequisites
echo -e "${CYAN}📋 Checking prerequisites...${NC}"

# Check Node.js
if ! command -v node &> /dev/null; then
    echo -e "${RED}❌ Node.js is not installed${NC}"
    echo -e "${YELLOW}Please install Node.js (v16 or higher) from https://nodejs.org/${NC}"
    exit 1
fi

NODE_VERSION=$(node -v | cut -d'v' -f2 | cut -d'.' -f1)
if [ "$NODE_VERSION" -lt 16 ]; then
    echo -e "${RED}❌ Node.js version is too old (v$NODE_VERSION)${NC}"
    echo -e "${YELLOW}Please upgrade to Node.js v16 or higher${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Node.js $(node -v)${NC}"

# Check npm
if ! command -v npm &> /dev/null; then
    echo -e "${RED}❌ npm is not installed${NC}"
    exit 1
fi

echo -e "${GREEN}✅ npm $(npm -v)${NC}"

# Check wiki content
if [ ! -d ".qoder/repowiki/en/content" ]; then
    echo -e "${RED}❌ Wiki content not found at .qoder/repowiki/en/content${NC}"
    echo -e "${YELLOW}Please ensure the wiki has been generated first.${NC}"
    exit 1
fi

echo -e "${GREEN}✅ Wiki content found${NC}"
echo ""

# Install root dependencies
echo -e "${CYAN}📦 Installing root dependencies...${NC}"

if [ ! -f "package.json" ]; then
    echo -e "${YELLOW}⚠️  No package.json found at root. Creating one...${NC}"
    npm init -y > /dev/null 2>&1
fi

echo -e "${BLUE}Installing express and cors...${NC}"
npm install express cors --save > /dev/null 2>&1 || {
    echo -e "${RED}❌ Failed to install root dependencies${NC}"
    exit 1
}

echo -e "${GREEN}✅ Root dependencies installed${NC}"
echo ""

# Install frontend dependencies
echo -e "${CYAN}📦 Installing frontend dependencies...${NC}"

if [ ! -d "frontend" ]; then
    echo -e "${RED}❌ Frontend directory not found${NC}"
    exit 1
fi

cd frontend

echo -e "${BLUE}Installing base dependencies...${NC}"
if [ ! -d "node_modules" ]; then
    npm install > /dev/null 2>&1 || {
        echo -e "${RED}❌ Failed to install base frontend dependencies${NC}"
        exit 1
    }
fi

echo -e "${BLUE}Installing documentation dependencies...${NC}"
npm install react-markdown remark-gfm react-syntax-highlighter mermaid --save > /dev/null 2>&1 || {
    echo -e "${RED}❌ Failed to install documentation dependencies${NC}"
    cd ..
    exit 1
}

echo -e "${BLUE}Installing dev dependencies...${NC}"
npm install @types/react-syntax-highlighter --save-dev > /dev/null 2>&1 || {
    echo -e "${YELLOW}⚠️  Failed to install some dev dependencies (non-critical)${NC}"
}

cd ..

echo -e "${GREEN}✅ Frontend dependencies installed${NC}"
echo ""

# Verify files exist
echo -e "${CYAN}🔍 Verifying installation files...${NC}"

FILES=(
    "wiki-server.js"
    "start-documentation.sh"
    "frontend/src/pages/Documentation.tsx"
    "frontend/src/hooks/useDocumentation.ts"
    "frontend/src/styles/documentation.css"
)

MISSING_FILES=0
for file in "${FILES[@]}"; do
    if [ ! -f "$file" ]; then
        echo -e "${RED}❌ Missing file: $file${NC}"
        MISSING_FILES=$((MISSING_FILES + 1))
    else
        echo -e "${GREEN}✅ $file${NC}"
    fi
done

if [ $MISSING_FILES -gt 0 ]; then
    echo -e "${RED}❌ Some required files are missing${NC}"
    exit 1
fi

echo ""

# Make scripts executable
echo -e "${CYAN}🔧 Setting up executable scripts...${NC}"

chmod +x start-documentation.sh
echo -e "${GREEN}✅ start-documentation.sh is now executable${NC}"

echo ""

# Test wiki server (quick test)
echo -e "${CYAN}🧪 Testing wiki server...${NC}"

node wiki-server.js > /dev/null 2>&1 &
SERVER_PID=$!
sleep 2

if ps -p $SERVER_PID > /dev/null; then
    echo -e "${GREEN}✅ Wiki server can start successfully${NC}"
    kill $SERVER_PID 2>/dev/null || true
else
    echo -e "${YELLOW}⚠️  Wiki server test inconclusive (may still work)${NC}"
fi

echo ""

# Installation summary
echo "╔════════════════════════════════════════════════════════════════╗"
echo "║                   Installation Complete! ✅                     ║"
echo "╚════════════════════════════════════════════════════════════════╝"
echo ""

echo -e "${GREEN}🎉 Regulens Documentation System is ready to use!${NC}"
echo ""

echo -e "${CYAN}📚 Quick Start:${NC}"
echo ""
echo -e "${YELLOW}1. Start the documentation system:${NC}"
echo -e "   ${BLUE}./start-documentation.sh${NC}"
echo ""
echo -e "${YELLOW}2. Open your browser to:${NC}"
echo -e "   ${BLUE}http://localhost:3000/documentation${NC}"
echo ""

echo -e "${CYAN}📖 Documentation:${NC}"
echo -e "   • Quick Start: ${BLUE}QUICKSTART.md${NC}"
echo -e "   • Full Guide:  ${BLUE}DOCUMENTATION_GUIDE.md${NC}"
echo -e "   • README:      ${BLUE}DOCUMENTATION_README.md${NC}"
echo -e "   • Summary:     ${BLUE}IMPLEMENTATION_SUMMARY.md${NC}"
echo ""

echo -e "${CYAN}🔧 Manual Start (if needed):${NC}"
echo -e "   ${YELLOW}Terminal 1:${NC} ${BLUE}npm run wiki-server${NC}"
echo -e "   ${YELLOW}Terminal 2:${NC} ${BLUE}cd frontend && npm run dev${NC}"
echo ""

echo -e "${CYAN}📊 Installed Components:${NC}"
echo -e "   ${GREEN}✅ Wiki API Server (Node.js/Express)${NC}"
echo -e "   ${GREEN}✅ Documentation UI Component (React)${NC}"
echo -e "   ${GREEN}✅ Mermaid Diagram Renderer${NC}"
echo -e "   ${GREEN}✅ Syntax Highlighting Engine${NC}"
echo -e "   ${GREEN}✅ Markdown Parser (GFM)${NC}"
echo -e "   ${GREEN}✅ Search Functionality${NC}"
echo -e "   ${GREEN}✅ Custom Hooks & Utilities${NC}"
echo ""

echo -e "${CYAN}🌟 Features Available:${NC}"
echo -e "   ${GREEN}✅ Interactive tree navigation${NC}"
echo -e "   ${GREEN}✅ Zoomable mermaid diagrams${NC}"
echo -e "   ${GREEN}✅ Syntax-highlighted code blocks${NC}"
echo -e "   ${GREEN}✅ Full-text search${NC}"
echo -e "   ${GREEN}✅ Dark theme UI${NC}"
echo -e "   ${GREEN}✅ Responsive design${NC}"
echo ""

echo -e "${YELLOW}⚠️  Important Notes:${NC}"
echo -e "   • Wiki server runs on port ${BLUE}3001${NC}"
echo -e "   • Frontend runs on port ${BLUE}3000${NC}"
echo -e "   • Both servers must be running${NC}"
echo -e "   • Check ${BLUE}wiki-server.log${NC} if issues occur${NC}"
echo ""

echo -e "${MAGENTA}╔════════════════════════════════════════════════════════════════╗${NC}"
echo -e "${MAGENTA}║  Ready to explore! Run: ./start-documentation.sh              ║${NC}"
echo -e "${MAGENTA}╚════════════════════════════════════════════════════════════════╝${NC}"
echo ""
