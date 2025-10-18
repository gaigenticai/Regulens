import React, { useState, useEffect } from 'react';
import { ChevronRight, ChevronDown, FileText, Folder, FolderOpen, Search, ZoomIn, ZoomOut, Download, AlertCircle } from 'lucide-react';
import ReactMarkdown from 'react-markdown';
import remarkGfm from 'remark-gfm';
import { Prism as SyntaxHighlighter } from 'react-syntax-highlighter';
import { vscDarkPlus } from 'react-syntax-highlighter/dist/esm/styles/prism';
import mermaid from 'mermaid';
import { useDocumentation, DocNode } from '@/hooks/useDocumentation';

// Initialize Mermaid
mermaid.initialize({
  startOnLoad: true,
  theme: 'dark',
  securityLevel: 'loose',
  fontFamily: 'monospace',
});

interface DocumentationProps {}

const Documentation: React.FC<DocumentationProps> = () => {
  const { structure, loading: structureLoading, error, loadContent } = useDocumentation();
  const [selectedDoc, setSelectedDoc] = useState<DocNode | null>(null);
  const [expandedNodes, setExpandedNodes] = useState<Set<string>>(new Set());
  const [searchQuery, setSearchQuery] = useState('');
  const [zoomLevel, setZoomLevel] = useState(100);
  const [loading, setLoading] = useState(false);

  // Re-render mermaid diagrams when content changes
  useEffect(() => {
    if (selectedDoc?.content) {
      setTimeout(() => {
        mermaid.contentLoaded();
      }, 100);
    }
  }, [selectedDoc]);

  const toggleNode = (nodeId: string) => {
    setExpandedNodes((prev) => {
      const newSet = new Set(prev);
      if (newSet.has(nodeId)) {
        newSet.delete(nodeId);
      } else {
        newSet.add(nodeId);
      }
      return newSet;
    });
  };

  const handleNodeClick = async (node: DocNode) => {
    if (node.isFile) {
      setLoading(true);
      try {
        const content = await loadContent(node.path);
        setSelectedDoc({ ...node, content });
      } catch (error) {
        console.error('Failed to load document:', error);
      }
      setLoading(false);
    } else {
      toggleNode(node.id);
    }
  };

  const renderTreeNode = (node: DocNode, level: number = 0): JSX.Element => {
    const isExpanded = expandedNodes.has(node.id);
    const hasChildren = node.children && node.children.length > 0;
    const isSelected = selectedDoc?.id === node.id;

    return (
      <div key={node.id} className="select-none">
        <div
          className={`flex items-center gap-2 px-3 py-2 hover:bg-gray-700 cursor-pointer rounded-md transition-colors ${
            isSelected ? 'bg-blue-600 text-white' : ''
          }`}
          style={{ paddingLeft: `${level * 1.5 + 0.75}rem` }}
          onClick={() => handleNodeClick(node)}
        >
          {hasChildren && (
            <span className="flex-shrink-0">
              {isExpanded ? (
                <ChevronDown className="w-4 h-4" />
              ) : (
                <ChevronRight className="w-4 h-4" />
              )}
            </span>
          )}
          {!hasChildren && <span className="w-4" />}
          <span className="flex-shrink-0">
            {node.isFile ? (
              <FileText className="w-4 h-4 text-blue-400" />
            ) : isExpanded ? (
              <FolderOpen className="w-4 h-4 text-yellow-400" />
            ) : (
              <Folder className="w-4 h-4 text-yellow-400" />
            )}
          </span>
          <span className="text-sm truncate">{node.title}</span>
        </div>
        {hasChildren && isExpanded && (
          <div>
            {node.children?.map((child) => renderTreeNode(child, level + 1))}
          </div>
        )}
      </div>
    );
  };

  const filterNodes = (nodes: DocNode[], query: string): DocNode[] => {
    if (!query) return nodes;
    return nodes
      .map((node) => {
        if (node.title.toLowerCase().includes(query.toLowerCase())) {
          return node;
        }
        if (node.children) {
          const filteredChildren = filterNodes(node.children, query);
          if (filteredChildren.length > 0) {
            return { ...node, children: filteredChildren };
          }
        }
        return null;
      })
      .filter(Boolean) as DocNode[];
  };

  const handleZoomIn = () => setZoomLevel((prev) => Math.min(prev + 10, 200));
  const handleZoomOut = () => setZoomLevel((prev) => Math.max(prev - 10, 50));

  const exportToPDF = () => {
    // Implementation for PDF export
    console.log('Export to PDF');
  };

  const wikiStructure = structure;
  const filteredStructure = filterNodes(wikiStructure, searchQuery);

  return (
    <div className="flex h-screen bg-gray-900 text-gray-100">
      {/* Sidebar - Tree Navigation */}
      <div className="w-80 bg-gray-800 border-r border-gray-700 flex flex-col">
        <div className="p-4 border-b border-gray-700">
          <h2 className="text-xl font-bold mb-4">Documentation</h2>
          <div className="relative">
            <Search className="absolute left-3 top-2.5 w-4 h-4 text-gray-400" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              className="w-full pl-10 pr-4 py-2 bg-gray-700 border border-gray-600 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500 text-sm"
            />
          </div>
        </div>
        <div className="flex-1 overflow-y-auto p-2">
          {filteredStructure.map((node) => renderTreeNode(node))}
        </div>
      </div>

      {/* Main Content Area */}
      <div className="flex-1 flex flex-col">
        {/* Toolbar */}
        <div className="bg-gray-800 border-b border-gray-700 px-6 py-3 flex items-center justify-between">
          <div className="flex items-center gap-4">
            {selectedDoc && (
              <h1 className="text-xl font-semibold">{selectedDoc.title}</h1>
            )}
          </div>
          <div className="flex items-center gap-2">
            <button
              onClick={handleZoomOut}
              className="p-2 hover:bg-gray-700 rounded-md transition-colors"
              title="Zoom Out"
            >
              <ZoomOut className="w-5 h-5" />
            </button>
            <span className="text-sm text-gray-400 min-w-[4rem] text-center">{zoomLevel}%</span>
            <button
              onClick={handleZoomIn}
              className="p-2 hover:bg-gray-700 rounded-md transition-colors"
              title="Zoom In"
            >
              <ZoomIn className="w-5 h-5" />
            </button>
            <div className="w-px h-6 bg-gray-700 mx-2" />
            <button
              onClick={exportToPDF}
              className="p-2 hover:bg-gray-700 rounded-md transition-colors"
              title="Export to PDF"
            >
              <Download className="w-5 h-5" />
            </button>
          </div>
        </div>

        {/* Content Display */}
        <div className="flex-1 overflow-y-auto">
          {loading ? (
            <div className="flex items-center justify-center h-full">
              <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-500"></div>
            </div>
          ) : selectedDoc?.content ? (
            <div 
              className="max-w-5xl mx-auto p-8 documentation-content"
              style={{ fontSize: `${zoomLevel}%` }}
            >
              <ReactMarkdown
                remarkPlugins={[remarkGfm]}
                components={{
                  code({ node, inline, className, children, ...props }) {
                    const match = /language-(\w+)/.exec(className || '');
                    const language = match ? match[1] : '';
                    
                    if (!inline && language === 'mermaid') {
                      return (
                        <div className="mermaid-container my-6 overflow-x-auto">
                          <div className="mermaid bg-gray-800 p-4 rounded-lg">
                            {String(children).replace(/\n$/, '')}
                          </div>
                        </div>
                      );
                    }
                    
                    return !inline && match ? (
                      <div className="my-4">
                        <SyntaxHighlighter
                          style={vscDarkPlus}
                          language={language}
                          PreTag="div"
                          customStyle={{
                            borderRadius: '0.5rem',
                            padding: '1rem',
                          }}
                          {...props}
                        >
                          {String(children).replace(/\n$/, '')}
                        </SyntaxHighlighter>
                      </div>
                    ) : (
                      <code className="bg-gray-800 px-1.5 py-0.5 rounded text-sm" {...props}>
                        {children}
                      </code>
                    );
                  },
                  h1: ({ children }) => (
                    <h1 className="text-4xl font-bold mt-8 mb-4 text-white border-b border-gray-700 pb-2">
                      {children}
                    </h1>
                  ),
                  h2: ({ children }) => (
                    <h2 className="text-3xl font-semibold mt-6 mb-3 text-white">
                      {children}
                    </h2>
                  ),
                  h3: ({ children }) => (
                    <h3 className="text-2xl font-semibold mt-5 mb-2 text-white">
                      {children}
                    </h3>
                  ),
                  h4: ({ children }) => (
                    <h4 className="text-xl font-semibold mt-4 mb-2 text-gray-200">
                      {children}
                    </h4>
                  ),
                  p: ({ children }) => (
                    <p className="my-4 text-gray-300 leading-relaxed">
                      {children}
                    </p>
                  ),
                  ul: ({ children }) => (
                    <ul className="my-4 ml-6 list-disc text-gray-300 space-y-2">
                      {children}
                    </ul>
                  ),
                  ol: ({ children }) => (
                    <ol className="my-4 ml-6 list-decimal text-gray-300 space-y-2">
                      {children}
                    </ol>
                  ),
                  li: ({ children }) => (
                    <li className="leading-relaxed">{children}</li>
                  ),
                  a: ({ href, children }) => (
                    <a
                      href={href}
                      className="text-blue-400 hover:text-blue-300 underline"
                      target={href?.startsWith('http') ? '_blank' : undefined}
                      rel={href?.startsWith('http') ? 'noopener noreferrer' : undefined}
                    >
                      {children}
                    </a>
                  ),
                  blockquote: ({ children }) => (
                    <blockquote className="border-l-4 border-blue-500 pl-4 my-4 italic text-gray-400">
                      {children}
                    </blockquote>
                  ),
                  table: ({ children }) => (
                    <div className="overflow-x-auto my-6">
                      <table className="min-w-full border border-gray-700 rounded-lg">
                        {children}
                      </table>
                    </div>
                  ),
                  thead: ({ children }) => (
                    <thead className="bg-gray-800">{children}</thead>
                  ),
                  th: ({ children }) => (
                    <th className="border border-gray-700 px-4 py-2 text-left font-semibold">
                      {children}
                    </th>
                  ),
                  td: ({ children }) => (
                    <td className="border border-gray-700 px-4 py-2">
                      {children}
                    </td>
                  ),
                }}
              >
                {selectedDoc.content}
              </ReactMarkdown>
            </div>
          ) : (
            <div className="flex items-center justify-center h-full text-gray-500">
              <div className="text-center">
                <FileText className="w-16 h-16 mx-auto mb-4 opacity-50" />
                <p className="text-lg">Select a document to view</p>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default Documentation;
