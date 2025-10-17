# Quick Start Guide - Regulens Documentation System

## Prerequisites

- Node.js (v16 or higher)
- npm or yarn
- Git

## Installation Steps

### 1. Install Frontend Dependencies

```bash
cd frontend
npm install react-markdown remark-gfm react-syntax-highlighter @types/react-syntax-highlighter mermaid
cd ..
```

### 2. Install Root Dependencies (for Wiki Server)

```bash
npm install express cors
```

## Running the Documentation System

### Option 1: Using the Startup Script (Recommended)

```bash
./start-documentation.sh
```

This will:
- Check all dependencies
- Start the wiki API server on port 3001
- Start the frontend dev server on port 3000
- Open the documentation automatically

### Option 2: Manual Start

**Terminal 1 - Wiki Server:**
```bash
npm run wiki-server
```

**Terminal 2 - Frontend:**
```bash
cd frontend
npm run dev
```

## Access the Documentation

Open your browser and navigate to:
```
http://localhost:3000/documentation
```

## What You'll See

```
┌─────────────────────────────────────────────────────┐
│  Documentation                                       │
│  ┌──────────────────────────────────────────┐       │
│  │ [Search]                                  │       │
│  └──────────────────────────────────────────┘       │
│                                                      │
│  📁 System Overview                                  │
│  📁 Getting Started                                  │
│  📁 Technology Stack                                 │
│  📁 Agent System                                     │
│     📄 Agent Types                                   │
│     📄 Communication Protocol                        │
│     📄 Lifecycle Management                          │
│  📁 Decision Engine                                  │
│     📄 MCDA Algorithms                               │
│     📄 Decision Process                              │
│     📄 Human-AI Collaboration                        │
│  📁 Knowledge Base                                   │
│  📁 LLM Integration                                  │
│  📁 Regulatory Monitor                               │
│  ... and more                                        │
└─────────────────────────────────────────────────────┘
```

## Features

✅ **Hierarchical Navigation** - Click folders to expand/collapse  
✅ **Interactive Mermaid Diagrams** - All charts render with zoom/scroll  
✅ **Syntax Highlighting** - Code blocks with language support  
✅ **Search** - Find content across all documentation  
✅ **Responsive Design** - Optimized for desktop viewing  
✅ **Dark Theme** - Easy on the eyes  

## Troubleshooting

### Port Already in Use

If you see "Port 3001 or 3000 already in use":

```bash
# Kill process on port 3001
lsof -ti:3001 | xargs kill -9

# Kill process on port 3000
lsof -ti:3000 | xargs kill -9
```

### Dependencies Not Found

```bash
# Install all dependencies
cd frontend && npm install && cd ..
npm install
```

### Wiki Content Not Found

Ensure the wiki content exists:
```bash
ls -la .qoder/repowiki/en/content/
```

## Next Steps

- Explore the documentation tree
- Use the search to find specific topics
- Zoom in/out on diagrams for better visibility
- Click source file links to view code references

## Support

For issues:
1. Check the browser console for errors
2. Review `wiki-server.log` for server errors
3. Ensure all dependencies are installed
4. Verify wiki content exists

Enjoy exploring the Regulens documentation! 📚✨
