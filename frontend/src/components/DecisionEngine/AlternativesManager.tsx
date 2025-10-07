/**
 * Alternatives Manager Component
 * Manages decision alternatives with score matrix
 * NO MOCKS - Real score calculations and validation
 */

import React, { useState } from 'react';
import { Plus, X } from 'lucide-react';
import { clsx } from 'clsx';
import type { Alternative, Criterion } from '@/types/api';

interface AlternativesManagerProps {
  alternatives: Alternative[];
  criteria: Criterion[];
  onChange: (alternatives: Alternative[]) => void;
  disabled?: boolean;
}

export const AlternativesManager: React.FC<AlternativesManagerProps> = ({
  alternatives,
  criteria,
  onChange,
  disabled = false,
}) => {
  const [newAlternativeName, setNewAlternativeName] = useState('');
  const [errors, setErrors] = useState<Record<string, string>>({});

  const generateId = (): string => {
    return `alt_${Date.now()}_${Math.random().toString(36).substr(2, 9)}`;
  };

  const handleAddAlternative = () => {
    if (!newAlternativeName.trim()) {
      setErrors({ name: 'Name is required' });
      return;
    }

    // Initialize scores for all criteria
    const scores: Record<string, number> = {};
    criteria.forEach((criterion) => {
      scores[criterion.name] = 0;
    });

    const newAlternative: Alternative = {
      id: generateId(),
      name: newAlternativeName.trim(),
      scores,
    };

    onChange([...alternatives, newAlternative]);
    setNewAlternativeName('');
    setErrors({});
  };

  const handleRemoveAlternative = (index: number) => {
    const updated = alternatives.filter((_, i) => i !== index);
    onChange(updated);
  };

  const handleUpdateName = (index: number, name: string) => {
    const updated = [...alternatives];
    updated[index] = { ...updated[index], name };
    onChange(updated);
  };

  const handleUpdateScore = (alternativeIndex: number, criterionName: string, score: number) => {
    const updated = [...alternatives];
    updated[alternativeIndex] = {
      ...updated[alternativeIndex],
      scores: {
        ...updated[alternativeIndex].scores,
        [criterionName]: score,
      },
    };
    onChange(updated);
  };

  // Update all alternatives when criteria change
  React.useEffect(() => {
    if (alternatives.length === 0) return;

    let needsUpdate = false;
    const updated = alternatives.map((alt) => {
      const updatedScores = { ...alt.scores };

      // Add scores for new criteria
      criteria.forEach((criterion) => {
        if (!(criterion.name in updatedScores)) {
          updatedScores[criterion.name] = 0;
          needsUpdate = true;
        }
      });

      // Remove scores for deleted criteria
      Object.keys(updatedScores).forEach((scoreName) => {
        if (!criteria.find((c) => c.name === scoreName)) {
          delete updatedScores[scoreName];
          needsUpdate = true;
        }
      });

      return { ...alt, scores: updatedScores };
    });

    if (needsUpdate) {
      onChange(updated);
    }
  }, [criteria]); // eslint-disable-line react-hooks/exhaustive-deps

  if (criteria.length === 0) {
    return (
      <div className="p-8 bg-gray-50 rounded-lg border border-gray-200 text-center">
        <p className="text-gray-600">Please add criteria first before defining alternatives</p>
      </div>
    );
  }

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <div>
          <h3 className="text-lg font-semibold text-gray-900">Alternatives</h3>
          <p className="text-sm text-gray-600">
            Define alternatives and score them against each criterion
          </p>
        </div>
        <div className="px-3 py-1 rounded-full text-sm font-medium bg-blue-100 text-blue-700">
          {alternatives.length} alternative{alternatives.length !== 1 ? 's' : ''}
        </div>
      </div>

      {/* Score Matrix */}
      {alternatives.length > 0 && (
        <div className="overflow-x-auto">
          <table className="w-full border-collapse">
            <thead>
              <tr className="bg-gray-50">
                <th className="px-4 py-3 text-left text-sm font-semibold text-gray-900 border border-gray-200">
                  Alternative
                </th>
                {criteria.map((criterion) => (
                  <th
                    key={criterion.name}
                    className="px-4 py-3 text-center text-sm font-semibold text-gray-900 border border-gray-200"
                  >
                    <div>{criterion.name}</div>
                    <div className="text-xs font-normal text-gray-600">
                      Weight: {criterion.weight.toFixed(2)}
                      {criterion.unit && ` (${criterion.unit})`}
                    </div>
                  </th>
                ))}
                <th className="px-4 py-3 border border-gray-200"></th>
              </tr>
            </thead>
            <tbody>
              {alternatives.map((alternative, altIndex) => (
                <tr key={alternative.id} className="hover:bg-gray-50">
                  <td className="px-4 py-3 border border-gray-200">
                    <input
                      type="text"
                      value={alternative.name}
                      onChange={(e) => handleUpdateName(altIndex, e.target.value)}
                      disabled={disabled}
                      className="w-full px-2 py-1 border border-gray-300 rounded focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100"
                    />
                  </td>
                  {criteria.map((criterion) => (
                    <td key={criterion.name} className="px-4 py-3 border border-gray-200">
                      <input
                        type="number"
                        value={alternative.scores[criterion.name] || 0}
                        onChange={(e) =>
                          handleUpdateScore(
                            altIndex,
                            criterion.name,
                            parseFloat(e.target.value) || 0
                          )
                        }
                        disabled={disabled}
                        step="0.01"
                        className="w-full px-2 py-1 border border-gray-300 rounded text-center focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100"
                      />
                    </td>
                  ))}
                  <td className="px-4 py-3 border border-gray-200 text-center">
                    <button
                      onClick={() => handleRemoveAlternative(altIndex)}
                      disabled={disabled}
                      className="p-1 text-red-600 hover:bg-red-50 rounded transition-colors disabled:opacity-50"
                    >
                      <X className="w-5 h-5" />
                    </button>
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      )}

      {/* Add New Alternative */}
      <div className="p-4 bg-green-50 rounded-lg border border-green-200">
        <div className="flex items-start gap-3">
          <div className="flex-1">
            <input
              type="text"
              value={newAlternativeName}
              onChange={(e) => setNewAlternativeName(e.target.value)}
              onKeyPress={(e) => e.key === 'Enter' && handleAddAlternative()}
              disabled={disabled}
              className={clsx(
                'w-full px-3 py-2 border rounded-lg focus:outline-none focus:ring-2 focus:ring-green-500 disabled:bg-gray-100',
                errors.name ? 'border-red-300' : 'border-gray-300'
              )}
              placeholder="Alternative name (e.g., Option A, Solution 1)"
            />
            {errors.name && (
              <p className="text-xs text-red-600 mt-1">{errors.name}</p>
            )}
          </div>

          <button
            onClick={handleAddAlternative}
            disabled={disabled}
            className="flex items-center gap-2 px-4 py-2 bg-green-600 text-white rounded-lg hover:bg-green-700 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <Plus className="w-5 h-5" />
            Add Alternative
          </button>
        </div>
      </div>

      {alternatives.length === 0 && (
        <div className="p-6 bg-gray-50 rounded-lg border border-gray-200 text-center text-gray-600">
          No alternatives added yet. Add your first alternative to begin scoring.
        </div>
      )}
    </div>
  );
};

export default AlternativesManager;
