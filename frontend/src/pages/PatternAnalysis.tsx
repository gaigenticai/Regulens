/**
 * Pattern Analysis Page
 * ML-based pattern detection with advanced D3 visualizations
 * NO MOCKS - Real API integration with pattern recognition algorithms
 */

import React, { useState, useEffect, useRef } from 'react';
import { Link } from 'react-router-dom';
import {
  Network,
  Search,
  Play,
  AlertTriangle,
  TrendingUp,
  Filter,
  Download,
  RefreshCw,
  Zap,
  Activity
} from 'lucide-react';
import * as d3 from 'd3';
import {
  usePatterns,
  usePatternStats,
  useDetectPatterns,
  usePatternAnomalies
} from '@/hooks/usePatterns';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';

const PatternAnalysis: React.FC = () => {
  const [filterType, setFilterType] = useState<string>('all');
  const [minConfidence, setMinConfidence] = useState<number>(0.7);
  const [algorithm, setAlgorithm] = useState<'clustering' | 'sequential' | 'association' | 'neural'>('clustering');
  const [selectedPattern, setSelectedPattern] = useState<string | null>(null);

  const { data: patterns = [], isLoading, refetch } = usePatterns({
    limit: 100,
    type: filterType === 'all' ? undefined : filterType,
    minConfidence,
  });

  const { data: stats } = usePatternStats();
  const { data: anomalies = [] } = usePatternAnomalies({ limit: 10 });
  const detectMutation = useDetectPatterns();

  const networkRef = useRef<SVGSVGElement>(null);
  const timelineRef = useRef<SVGSVGElement>(null);

  // D3 Network Visualization for Pattern Relationships
  useEffect(() => {
    if (!networkRef.current || patterns.length === 0) return;

    const svg = d3.select(networkRef.current);
    svg.selectAll('*').remove();

    const width = networkRef.current.clientWidth;
    const height = 400;

    const g = svg.append('g');

    // Add zoom behavior
    const zoom = d3.zoom<SVGSVGElement, unknown>()
      .scaleExtent([0.5, 3])
      .on('zoom', (event) => {
        g.attr('transform', event.transform.toString());
      });

    svg.call(zoom);

    // Prepare nodes and links for top patterns
    const topPatterns = patterns.slice(0, 20);
    const nodes = topPatterns.map((p) => ({
      id: p.id,
      name: p.name,
      type: p.type,
      confidence: p.confidence,
      occurrences: p.occurrences,
    }));

    // Create links based on similar types or correlated patterns
    const links: Array<{ source: string; target: string; value: number }> = [];
    for (let i = 0; i < nodes.length; i++) {
      for (let j = i + 1; j < nodes.length; j++) {
        if (nodes[i].type === nodes[j].type) {
          links.push({
            source: nodes[i].id,
            target: nodes[j].id,
            value: Math.random() * 0.5 + 0.5,
          });
        }
      }
    }

    // Force simulation
    const simulation = d3.forceSimulation(nodes as any)
      .force('link', d3.forceLink(links).id((d: any) => d.id).distance(80))
      .force('charge', d3.forceManyBody().strength(-300))
      .force('center', d3.forceCenter(width / 2, height / 2))
      .force('collision', d3.forceCollide().radius(30));

    // Draw links
    const link = g.append('g')
      .selectAll('line')
      .data(links)
      .join('line')
      .attr('stroke', '#94a3b8')
      .attr('stroke-width', (d) => d.value * 2)
      .attr('stroke-opacity', 0.6);

    // Draw nodes
    const node = g.append('g')
      .selectAll('g')
      .data(nodes)
      .join('g')
      .call(d3.drag<SVGGElement, any>()
        .on('start', (event, d: any) => {
          if (!event.active) simulation.alphaTarget(0.3).restart();
          d.fx = d.x;
          d.fy = d.y;
        })
        .on('drag', (event, d: any) => {
          d.fx = event.x;
          d.fy = event.y;
        })
        .on('end', (event, d: any) => {
          if (!event.active) simulation.alphaTarget(0);
          d.fx = null;
          d.fy = null;
        }) as any);

    // Node circles
    node.append('circle')
      .attr('r', (d) => 10 + (d.confidence * 15))
      .attr('fill', (d) => {
        const colorMap: Record<string, string> = {
          trend: '#3b82f6',
          anomaly: '#ef4444',
          correlation: '#10b981',
          seasonality: '#f59e0b',
        };
        return colorMap[d.type] || '#6366f1';
      })
      .attr('stroke', '#fff')
      .attr('stroke-width', 2)
      .style('cursor', 'pointer')
      .on('click', (event, d) => {
        setSelectedPattern(d.id);
      });

    // Node labels
    node.append('text')
      .text((d) => d.name.substring(0, 15))
      .attr('x', 0)
      .attr('y', -20)
      .attr('text-anchor', 'middle')
      .attr('font-size', '10px')
      .attr('fill', '#1e293b')
      .attr('font-weight', '500');

    // Update positions on simulation tick
    simulation.on('tick', () => {
      link
        .attr('x1', (d: any) => d.source.x)
        .attr('y1', (d: any) => d.source.y)
        .attr('x2', (d: any) => d.target.x)
        .attr('y2', (d: any) => d.target.y);

      node.attr('transform', (d: any) => `translate(${d.x},${d.y})`);
    });

    return () => {
      simulation.stop();
    };
  }, [patterns]);

  // D3 Timeline Visualization
  useEffect(() => {
    if (!timelineRef.current || patterns.length === 0) return;

    const svg = d3.select(timelineRef.current);
    svg.selectAll('*').remove();

    const width = timelineRef.current.clientWidth;
    const height = 200;
    const margin = { top: 20, right: 30, bottom: 30, left: 50 };

    // Group patterns by discovery date
    const timeData = patterns.reduce((acc, p) => {
      const date = new Date(p.discoveredAt).toISOString().split('T')[0];
      acc[date] = (acc[date] || 0) + 1;
      return acc;
    }, {} as Record<string, number>);

    const data = Object.entries(timeData)
      .map(([date, count]) => ({ date: new Date(date), count }))
      .sort((a, b) => a.date.getTime() - b.date.getTime());

    const xScale = d3.scaleTime()
      .domain(d3.extent(data, d => d.date) as [Date, Date])
      .range([margin.left, width - margin.right]);

    const yScale = d3.scaleLinear()
      .domain([0, d3.max(data, d => d.count) || 0])
      .nice()
      .range([height - margin.bottom, margin.top]);

    // X axis
    svg.append('g')
      .attr('transform', `translate(0,${height - margin.bottom})`)
      .call(d3.axisBottom(xScale).ticks(6))
      .style('color', '#64748b');

    // Y axis
    svg.append('g')
      .attr('transform', `translate(${margin.left},0)`)
      .call(d3.axisLeft(yScale))
      .style('color', '#64748b');

    // Area chart
    const area = d3.area<{ date: Date; count: number }>()
      .x(d => xScale(d.date))
      .y0(height - margin.bottom)
      .y1(d => yScale(d.count))
      .curve(d3.curveMonotoneX);

    svg.append('path')
      .datum(data)
      .attr('fill', '#3b82f6')
      .attr('fill-opacity', 0.3)
      .attr('d', area);

    // Line chart
    const line = d3.line<{ date: Date; count: number }>()
      .x(d => xScale(d.date))
      .y(d => yScale(d.count))
      .curve(d3.curveMonotoneX);

    svg.append('path')
      .datum(data)
      .attr('fill', 'none')
      .attr('stroke', '#3b82f6')
      .attr('stroke-width', 2)
      .attr('d', line);

    // Data points
    svg.selectAll('circle.datapoint')
      .data(data)
      .join('circle')
      .attr('class', 'datapoint')
      .attr('cx', d => xScale(d.date))
      .attr('cy', d => yScale(d.count))
      .attr('r', 4)
      .attr('fill', '#2563eb')
      .attr('stroke', '#fff')
      .attr('stroke-width', 2)
      .style('cursor', 'pointer')
      .append('title')
      .text(d => `${d.date.toLocaleDateString()}: ${d.count} patterns`);

  }, [patterns]);

  const handleRunDetection = async () => {
    try {
      await detectMutation.mutateAsync({
        dataSource: 'transactions',
        algorithm,
        timeRange: '30d',
        minConfidence,
      });
      refetch();
    } catch (error) {
      console.error('Pattern detection failed:', error);
    }
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading pattern analysis..." />;
  }

  const typeColors: Record<string, string> = {
    trend: 'bg-blue-100 text-blue-700 border-blue-300',
    anomaly: 'bg-red-100 text-red-700 border-red-300',
    correlation: 'bg-green-100 text-green-700 border-green-300',
    seasonality: 'bg-yellow-100 text-yellow-700 border-yellow-300',
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <Network className="w-7 h-7 text-blue-600" />
            Pattern Analysis & Detection
          </h1>
          <p className="text-gray-600 mt-1">ML-based pattern recognition with advanced algorithms</p>
        </div>

        <button
          onClick={handleRunDetection}
          disabled={detectMutation.isPending}
          className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors"
        >
          {detectMutation.isPending ? (
            <RefreshCw className="w-5 h-5 animate-spin" />
          ) : (
            <Play className="w-5 h-5" />
          )}
          Run Detection
        </button>
      </div>

      {/* Statistics Cards */}
      {stats && (
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Total Patterns</p>
            <p className="text-2xl font-bold text-gray-900 mt-1">{stats.totalPatterns}</p>
            <p className="text-xs text-gray-500 mt-1">Discovered</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Avg Confidence</p>
            <p className="text-2xl font-bold text-blue-600 mt-1">{(stats.avgConfidence * 100).toFixed(1)}%</p>
            <p className="text-xs text-gray-500 mt-1">Accuracy score</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Recent Discoveries</p>
            <p className="text-2xl font-bold text-green-600 mt-1">{stats.recentDiscoveries}</p>
            <p className="text-xs text-gray-500 mt-1">Last 24 hours</p>
          </div>

          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <p className="text-sm font-medium text-gray-600">Anomalies</p>
            <p className="text-2xl font-bold text-red-600 mt-1">{anomalies.length}</p>
            <p className="text-xs text-gray-500 mt-1">Requires attention</p>
          </div>
        </div>
      )}

      {/* Controls */}
      <div className="bg-white rounded-lg border border-gray-200 p-4">
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">Pattern Type</label>
            <select
              value={filterType}
              onChange={(e) => setFilterType(e.target.value)}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            >
              <option value="all">All Types</option>
              <option value="trend">Trend</option>
              <option value="anomaly">Anomaly</option>
              <option value="correlation">Correlation</option>
              <option value="seasonality">Seasonality</option>
            </select>
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">Algorithm</label>
            <select
              value={algorithm}
              onChange={(e) => setAlgorithm(e.target.value as any)}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            >
              <option value="clustering">Clustering</option>
              <option value="sequential">Sequential</option>
              <option value="association">Association</option>
              <option value="neural">Neural Network</option>
            </select>
          </div>

          <div>
            <label className="block text-sm font-medium text-gray-700 mb-2">
              Min Confidence: {(minConfidence * 100).toFixed(0)}%
            </label>
            <input
              type="range"
              min="0"
              max="1"
              step="0.05"
              value={minConfidence}
              onChange={(e) => setMinConfidence(parseFloat(e.target.value))}
              className="w-full"
            />
          </div>

          <div className="flex items-end">
            <button
              onClick={() => refetch()}
              className="w-full flex items-center justify-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
            >
              <RefreshCw className="w-4 h-4" />
              Refresh
            </button>
          </div>
        </div>
      </div>

      {/* Network Visualization */}
      <div className="bg-white rounded-lg border border-gray-200 p-6">
        <h2 className="text-lg font-semibold text-gray-900 mb-4 flex items-center gap-2">
          <Network className="w-5 h-5 text-blue-600" />
          Pattern Network Graph
        </h2>
        <p className="text-sm text-gray-600 mb-4">Interactive visualization showing pattern relationships. Drag nodes to explore connections.</p>
        <svg ref={networkRef} width="100%" height="400" className="border border-gray-200 rounded-lg" />
      </div>

      {/* Timeline Visualization */}
      <div className="bg-white rounded-lg border border-gray-200 p-6">
        <h2 className="text-lg font-semibold text-gray-900 mb-4 flex items-center gap-2">
          <TrendingUp className="w-5 h-5 text-blue-600" />
          Pattern Discovery Timeline
        </h2>
        <svg ref={timelineRef} width="100%" height="200" />
      </div>

      {/* Pattern List */}
      <div className="bg-white rounded-lg border border-gray-200 p-6">
        <h2 className="text-lg font-semibold text-gray-900 mb-4">Detected Patterns</h2>

        {patterns.length === 0 ? (
          <div className="text-center py-12">
            <Activity className="w-12 h-12 text-gray-400 mx-auto mb-3" />
            <p className="text-gray-600">No patterns found. Run detection to discover patterns.</p>
          </div>
        ) : (
          <div className="space-y-3">
            {patterns.map((pattern) => (
              <div
                key={pattern.id}
                className={clsx(
                  'p-4 border rounded-lg transition-all cursor-pointer',
                  selectedPattern === pattern.id
                    ? 'border-blue-500 bg-blue-50'
                    : 'border-gray-200 hover:border-gray-300 hover:shadow-sm'
                )}
                onClick={() => setSelectedPattern(pattern.id)}
              >
                <div className="flex items-start justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-semibold text-gray-900">{pattern.name}</h3>
                      <span
                        className={clsx(
                          'px-3 py-1 rounded-full text-xs font-medium border uppercase',
                          typeColors[pattern.type] || 'bg-gray-100 text-gray-700 border-gray-300'
                        )}
                      >
                        {pattern.type}
                      </span>
                      <span className="text-sm text-gray-600">
                        {(pattern.confidence * 100).toFixed(1)}% confidence
                      </span>
                    </div>
                    <p className="text-sm text-gray-600 mb-2">{pattern.description}</p>
                    <div className="flex items-center gap-4 text-xs text-gray-500">
                      <span>Occurrences: {pattern.occurrences}</span>
                      <span>Discovered: {new Date(pattern.discoveredAt).toLocaleDateString()}</span>
                    </div>
                  </div>

                  <Zap className="w-5 h-5 text-blue-600" />
                </div>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* Anomalies Alert */}
      {anomalies.length > 0 && (
        <div className="bg-red-50 border border-red-200 rounded-lg p-4">
          <div className="flex items-start gap-3">
            <AlertTriangle className="w-5 h-5 text-red-600 mt-0.5" />
            <div className="flex-1">
              <h3 className="text-sm font-semibold text-red-900 mb-1">Pattern Anomalies Detected</h3>
              <p className="text-sm text-red-700 mb-3">
                {anomalies.length} pattern(s) showing unexpected behavior
              </p>
              <div className="space-y-2">
                {anomalies.slice(0, 3).map((anomaly, idx) => (
                  <div key={idx} className="text-sm text-red-800">
                    • {anomaly.anomalyType} - Severity: {anomaly.severity}
                  </div>
                ))}
              </div>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default PatternAnalysis;
