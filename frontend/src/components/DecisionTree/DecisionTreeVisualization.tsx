/**
 * Decision Tree D3.js Visualization Component
 * Real D3 force-directed graph - NO MOCKS
 * Renders actual decision tree data from backend
 */

import React, { useEffect, useRef } from 'react';
import * as d3 from 'd3';
import type { DecisionTree } from '@/types/api';

interface DecisionTreeVisualizationProps {
  tree: DecisionTree;
  width?: number;
  height?: number;
  onNodeClick?: (nodeId: string) => void;
}

export const DecisionTreeVisualization: React.FC<DecisionTreeVisualizationProps> = ({
  tree,
  width = 800,
  height = 600,
  onNodeClick,
}) => {
  const svgRef = useRef<SVGSVGElement>(null);

  useEffect(() => {
    if (!svgRef.current || !tree.visualization) return;

    // Clear previous visualization
    d3.select(svgRef.current).selectAll('*').remove();

    const svg = d3.select(svgRef.current);
    const { nodes: rawNodes, links: rawLinks } = tree.visualization;

    // Create a map for quick node lookup
    const nodeMap = new Map(rawNodes.map((node) => [node.id, node]));

    // Convert backend data to D3 format with proper typing
    interface D3Node extends d3.SimulationNodeDatum {
      id: string;
      name: string;
      value?: number;
      type: string;
    }

    interface D3Link extends d3.SimulationLinkDatum<D3Node> {
      value?: number;
    }

    const nodes: D3Node[] = rawNodes.map((node) => ({
      id: node.id,
      name: node.name,
      value: node.value,
      type: node.type,
    }));

    const links: D3Link[] = rawLinks.map((link) => {
      const sourceNode = nodeMap.get(link.source);
      const targetNode = nodeMap.get(link.target);

      if (!sourceNode || !targetNode) {
        console.warn(`Missing node for link: ${link.source} -> ${link.target}`);
      }

      return {
        source: link.source,
        target: link.target,
        value: link.value,
      };
    });

    // Create force simulation
    const simulation = d3
      .forceSimulation<D3Node>(nodes)
      .force(
        'link',
        d3
          .forceLink<D3Node, D3Link>(links)
          .id((d) => d.id)
          .distance(100)
      )
      .force('charge', d3.forceManyBody().strength(-300))
      .force('center', d3.forceCenter(width / 2, height / 2))
      .force('collision', d3.forceCollide().radius(40));

    // Create container group
    const g = svg.append('g');

    // Add zoom behavior
    const zoom = d3.zoom<SVGSVGElement, unknown>()
      .scaleExtent([0.1, 4])
      .on('zoom', (event) => {
        g.attr('transform', event.transform);
      });

    svg.call(zoom);

    // Create links
    const link = g
      .append('g')
      .selectAll('line')
      .data(links)
      .join('line')
      .attr('stroke', '#999')
      .attr('stroke-opacity', 0.6)
      .attr('stroke-width', (d) => Math.sqrt(d.value || 1));

    // Create nodes
    const node = g
      .append('g')
      .selectAll('g')
      .data(nodes)
      .join('g')
      .call(
        d3.drag<any, D3Node>()
          .on('start', (event, d) => {
            if (!event.active) simulation.alphaTarget(0.3).restart();
            d.fx = d.x;
            d.fy = d.y;
          })
          .on('drag', (event, d) => {
            d.fx = event.x;
            d.fy = event.y;
          })
          .on('end', (event, d) => {
            if (!event.active) simulation.alphaTarget(0);
            d.fx = null;
            d.fy = null;
          })
      );

    // Add circles to nodes with type-based coloring
    const getNodeColor = (type: string): string => {
      switch (type) {
        case 'criterion':
          return '#3b82f6'; // blue
        case 'alternative':
          return '#10b981'; // green
        case 'score':
          return '#f59e0b'; // amber
        default:
          return '#6b7280'; // gray
      }
    };

    node
      .append('circle')
      .attr('r', (d) => {
        // Size based on value if available
        if (d.value !== undefined) {
          return 10 + Math.sqrt(d.value) * 10;
        }
        return 20;
      })
      .attr('fill', (d) => getNodeColor(d.type))
      .attr('stroke', '#fff')
      .attr('stroke-width', 2)
      .style('cursor', 'pointer')
      .on('click', (_event, d) => {
        if (onNodeClick) {
          onNodeClick(d.id);
        }
      });

    // Add labels to nodes
    node
      .append('text')
      .text((d) => d.name)
      .attr('x', 0)
      .attr('y', (d) => {
        // Position text below larger nodes
        if (d.value !== undefined) {
          return 15 + Math.sqrt(d.value) * 10;
        }
        return 25;
      })
      .attr('text-anchor', 'middle')
      .attr('font-size', '12px')
      .attr('font-weight', 'bold')
      .attr('fill', '#374151')
      .style('pointer-events', 'none');

    // Add value labels if present
    node
      .filter((d) => d.value !== undefined)
      .append('text')
      .text((d) => d.value!.toFixed(3))
      .attr('x', 0)
      .attr('y', 5)
      .attr('text-anchor', 'middle')
      .attr('font-size', '10px')
      .attr('fill', '#fff')
      .style('pointer-events', 'none');

    // Update positions on simulation tick
    simulation.on('tick', () => {
      link
        .attr('x1', (d) => (d.source as D3Node).x || 0)
        .attr('y1', (d) => (d.source as D3Node).y || 0)
        .attr('x2', (d) => (d.target as D3Node).x || 0)
        .attr('y2', (d) => (d.target as D3Node).y || 0);

      node.attr('transform', (d) => `translate(${d.x || 0},${d.y || 0})`);
    });

    // Add legend
    const legend = svg
      .append('g')
      .attr('transform', `translate(20, ${height - 100})`);

    const legendData = [
      { type: 'criterion', label: 'Criterion', color: '#3b82f6' },
      { type: 'alternative', label: 'Alternative', color: '#10b981' },
      { type: 'score', label: 'Score', color: '#f59e0b' },
    ];

    legend
      .selectAll('g')
      .data(legendData)
      .join('g')
      .attr('transform', (_d, i) => `translate(0, ${i * 25})`)
      .each(function (legendItem) {
        const g = d3.select(this);
        g.append('circle')
          .attr('r', 8)
          .attr('fill', legendItem.color)
          .attr('stroke', '#fff')
          .attr('stroke-width', 2);
        g.append('text')
          .attr('x', 20)
          .attr('y', 5)
          .text(legendItem.label)
          .attr('font-size', '12px')
          .attr('fill', '#374151');
      });

    // Cleanup
    return () => {
      simulation.stop();
    };
  }, [tree, width, height, onNodeClick]);

  return (
    <div className="relative bg-white rounded-lg border border-gray-200 p-4">
      <div className="absolute top-4 right-4 bg-white bg-opacity-90 rounded-lg p-2 text-xs text-gray-600 shadow-sm">
        <div>Algorithm: <span className="font-semibold">{tree.algorithm}</span></div>
        <div className="mt-1">Nodes: {tree.visualization.nodes.length}</div>
        <div>Links: {tree.visualization.links.length}</div>
      </div>

      <svg
        ref={svgRef}
        width={width}
        height={height}
        style={{ border: '1px solid #e5e7eb', borderRadius: '0.5rem' }}
      />

      <div className="mt-4 text-sm text-gray-600">
        <p><strong>Tip:</strong> Drag nodes to rearrange, scroll to zoom, click nodes for details</p>
      </div>
    </div>
  );
};

export default DecisionTreeVisualization;
