/**
 * Algorithm Selector Component
 * Allows selection of MCDA algorithm with descriptions
 * NO MOCKS - All 5 real algorithms from backend
 */

import React from 'react';
import { Check } from 'lucide-react';
import { clsx } from 'clsx';
import type { MCDAAlgorithm } from '@/types/api';

interface AlgorithmSelectorProps {
  selected: MCDAAlgorithm;
  onChange: (algorithm: MCDAAlgorithm) => void;
  disabled?: boolean;
}

interface AlgorithmInfo {
  id: MCDAAlgorithm;
  name: string;
  description: string;
  bestFor: string;
  complexity: 'Low' | 'Medium' | 'High';
}

const algorithms: AlgorithmInfo[] = [
  {
    id: 'TOPSIS',
    name: 'TOPSIS',
    description: 'Technique for Order of Preference by Similarity to Ideal Solution',
    bestFor: 'Finding alternatives closest to ideal and farthest from negative-ideal',
    complexity: 'Medium',
  },
  {
    id: 'ELECTRE_III',
    name: 'ELECTRE III',
    description: 'Elimination and Choice Expressing Reality',
    bestFor: 'Outranking methods with indifference and preference thresholds',
    complexity: 'High',
  },
  {
    id: 'PROMETHEE_II',
    name: 'PROMETHEE II',
    description: 'Preference Ranking Organization Method for Enrichment Evaluations',
    bestFor: 'Complete ranking with pairwise comparisons and preference functions',
    complexity: 'High',
  },
  {
    id: 'AHP',
    name: 'AHP',
    description: 'Analytic Hierarchy Process',
    bestFor: 'Hierarchical decision making with pairwise comparisons',
    complexity: 'Medium',
  },
  {
    id: 'VIKOR',
    name: 'VIKOR',
    description: 'VIseKriterijumska Optimizacija I Kompromisno Resenje',
    bestFor: 'Finding compromise solutions with conflicting criteria',
    complexity: 'Medium',
  },
];

const complexityColors = {
  Low: 'bg-green-100 text-green-700',
  Medium: 'bg-yellow-100 text-yellow-700',
  High: 'bg-orange-100 text-orange-700',
};

export const AlgorithmSelector: React.FC<AlgorithmSelectorProps> = ({
  selected,
  onChange,
  disabled = false,
}) => {
  return (
    <div className="space-y-4">
      <div>
        <h3 className="text-lg font-semibold text-gray-900">Select MCDA Algorithm</h3>
        <p className="text-sm text-gray-600">
          Choose the multi-criteria decision analysis method
        </p>
      </div>

      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
        {algorithms.map((algorithm) => {
          const isSelected = selected === algorithm.id;

          return (
            <button
              key={algorithm.id}
              onClick={() => onChange(algorithm.id)}
              disabled={disabled}
              className={clsx(
                'relative p-4 rounded-lg border-2 text-left transition-all',
                isSelected
                  ? 'border-blue-600 bg-blue-50'
                  : 'border-gray-200 bg-white hover:border-blue-300 hover:bg-blue-50',
                disabled && 'opacity-50 cursor-not-allowed'
              )}
            >
              {isSelected && (
                <div className="absolute top-2 right-2 w-6 h-6 bg-blue-600 rounded-full flex items-center justify-center">
                  <Check className="w-4 h-4 text-white" />
                </div>
              )}

              <div className="space-y-2">
                <div className="flex items-start justify-between">
                  <h4 className="font-bold text-gray-900 text-lg">{algorithm.name}</h4>
                  <span
                    className={clsx(
                      'px-2 py-0.5 rounded-full text-xs font-medium',
                      complexityColors[algorithm.complexity]
                    )}
                  >
                    {algorithm.complexity}
                  </span>
                </div>

                <p className="text-xs text-gray-600 font-medium">
                  {algorithm.description}
                </p>

                <div className="pt-2 border-t border-gray-200">
                  <p className="text-xs text-gray-700">
                    <span className="font-medium">Best for:</span> {algorithm.bestFor}
                  </p>
                </div>
              </div>
            </button>
          );
        })}
      </div>

      <div className="p-4 bg-blue-50 rounded-lg border border-blue-200">
        <h4 className="font-semibold text-blue-900 mb-2">About {algorithms.find(a => a.id === selected)?.name}</h4>
        <p className="text-sm text-blue-800">
          {algorithms.find(a => a.id === selected)?.description}
        </p>
        <p className="text-sm text-blue-800 mt-2">
          <span className="font-medium">Best for:</span> {algorithms.find(a => a.id === selected)?.bestFor}
        </p>
      </div>
    </div>
  );
};

export default AlgorithmSelector;
