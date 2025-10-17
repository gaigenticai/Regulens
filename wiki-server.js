import express from 'express';
import fs from 'fs/promises';
import path from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const app = express();
const PORT = process.env.WIKI_PORT || 3001;

// Middleware
app.use(express.json());
app.use((req, res, next) => {
  res.header('Access-Control-Allow-Origin', '*');
  res.header('Access-Control-Allow-Headers', 'Origin, X-Requested-With, Content-Type, Accept');
  next();
});

const WIKI_BASE_PATH = path.join(__dirname, '../.qoder/repowiki/en/content');
const WIKI_META_PATH = path.join(__dirname, '../.qoder/repowiki/en/meta/repowiki-metadata.json');

// Build tree structure from directory
async function buildTreeStructure(dirPath, basePath = '') {
  const entries = await fs.readdir(dirPath, { withFileTypes: true });
  const nodes = [];

  for (const entry of entries) {
    const fullPath = path.join(dirPath, entry.name);
    const relativePath = path.join(basePath, entry.name);
    const id = relativePath.replace(/\//g, '-').replace(/\s+/g, '-').toLowerCase();

    if (entry.isDirectory()) {
      const children = await buildTreeStructure(fullPath, relativePath);
      nodes.push({
        id,
        title: entry.name,
        path: relativePath,
        children,
        isFile: false,
      });
    } else if (entry.name.endsWith('.md')) {
      nodes.push({
        id,
        title: entry.name.replace('.md', ''),
        path: relativePath,
        isFile: true,
      });
    }
  }

  // Sort: directories first, then files, both alphabetically
  return nodes.sort((a, b) => {
    if (a.isFile === b.isFile) {
      return a.title.localeCompare(b.title);
    }
    return a.isFile ? 1 : -1;
  });
}

// API Endpoints

// Get documentation structure
app.get('/api/documentation/structure', async (req, res) => {
  try {
    const structure = await buildTreeStructure(WIKI_BASE_PATH);
    res.json(structure);
  } catch (error) {
    console.error('Error building documentation structure:', error);
    res.status(500).json({ error: 'Failed to load documentation structure' });
  }
});

// Get document content
app.get('/api/documentation/content', async (req, res) => {
  try {
    const { path: docPath } = req.query;
    
    if (!docPath || typeof docPath !== 'string') {
      return res.status(400).json({ error: 'Path parameter is required' });
    }

    // Security: prevent directory traversal
    const safePath = path.normalize(docPath).replace(/^(\.\.[\/\\])+/, '');
    const fullPath = path.join(WIKI_BASE_PATH, safePath);

    // Ensure the path is within the wiki directory
    if (!fullPath.startsWith(WIKI_BASE_PATH)) {
      return res.status(403).json({ error: 'Access denied' });
    }

    const content = await fs.readFile(fullPath, 'utf-8');
    res.type('text/markdown').send(content);
  } catch (error) {
    console.error('Error reading document:', error);
    res.status(404).json({ error: 'Document not found' });
  }
});

// Get metadata
app.get('/api/documentation/metadata', async (req, res) => {
  try {
    const metadata = await fs.readFile(WIKI_META_PATH, 'utf-8');
    res.json(JSON.parse(metadata));
  } catch (error) {
    console.error('Error reading metadata:', error);
    res.status(500).json({ error: 'Failed to load metadata' });
  }
});

// Search documentation
app.get('/api/documentation/search', async (req, res) => {
  try {
    const { q } = req.query;
    
    if (!q || typeof q !== 'string') {
      return res.status(400).json({ error: 'Query parameter is required' });
    }

    const results = await searchDocuments(WIKI_BASE_PATH, q.toLowerCase());
    res.json(results);
  } catch (error) {
    console.error('Error searching documents:', error);
    res.status(500).json({ error: 'Search failed' });
  }
});

// Recursive search function
async function searchDocuments(dirPath, query, basePath = '') {
  const entries = await fs.readdir(dirPath, { withFileTypes: true });
  const results = [];

  for (const entry of entries) {
    const fullPath = path.join(dirPath, entry.name);
    const relativePath = path.join(basePath, entry.name);

    if (entry.isDirectory()) {
      const subResults = await searchDocuments(fullPath, query, relativePath);
      results.push(...subResults);
    } else if (entry.name.endsWith('.md')) {
      const content = await fs.readFile(fullPath, 'utf-8');
      const title = entry.name.replace('.md', '');
      
      if (
        title.toLowerCase().includes(query) ||
        content.toLowerCase().includes(query)
      ) {
        // Find matching lines
        const lines = content.split('\n');
        const matches = lines
          .map((line, index) => ({ line, index }))
          .filter(({ line }) => line.toLowerCase().includes(query))
          .slice(0, 3)
          .map(({ line, index }) => ({
            lineNumber: index + 1,
            text: line.trim(),
          }));

        results.push({
          title,
          path: relativePath,
          matches,
        });
      }
    }
  }

  return results;
}

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'ok', service: 'wiki-server' });
});

app.listen(PORT, () => {
  console.log(`Wiki documentation server running on port ${PORT}`);
  console.log(`Wiki content path: ${WIKI_BASE_PATH}`);
});
