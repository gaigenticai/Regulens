import { useState, useEffect } from 'react';

export interface DocNode {
  id: string;
  title: string;
  path: string;
  children?: DocNode[];
  isFile?: boolean;
  content?: string;
}

export interface SearchResult {
  title: string;
  path: string;
  matches: {
    lineNumber: number;
    text: string;
  }[];
}

const WIKI_API_BASE = '/api/documentation';

export function useDocumentation() {
  const [structure, setStructure] = useState<DocNode[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    loadStructure();
  }, []);

  const loadStructure = async () => {
    setLoading(true);
    setError(null);
    try {
      const response = await fetch(`${WIKI_API_BASE}/structure`);
      if (!response.ok) {
        throw new Error('Failed to load documentation structure');
      }
      const data = await response.json();
      setStructure(data);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Unknown error');
      console.error('Error loading documentation structure:', err);
    } finally {
      setLoading(false);
    }
  };

  const loadContent = async (path: string): Promise<string> => {
    const response = await fetch(`${WIKI_API_BASE}/content?path=${encodeURIComponent(path)}`);
    if (!response.ok) {
      throw new Error('Failed to load document content');
    }
    return await response.text();
  };

  const search = async (query: string): Promise<SearchResult[]> => {
    const response = await fetch(`${WIKI_API_BASE}/search?q=${encodeURIComponent(query)}`);
    if (!response.ok) {
      throw new Error('Search failed');
    }
    return await response.json();
  };

  return {
    structure,
    loading,
    error,
    loadContent,
    search,
  };
}
