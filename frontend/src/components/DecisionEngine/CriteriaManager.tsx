/**
 * Criteria Manager Component
 * Manages decision criteria with real validation
 * NO MOCKS - Production-grade input handling
 */

import React, { useState } from 'react';
import { Plus, X, TrendingUp, TrendingDown } from 'lucide-react';
import { clsx } from 'clsx';
import type { Criterion } from '@/types/api';

interface CriteriaManagerProps {
  criteria: Criterion[];
  onChange: (criteria: Criterion[]) => void;
  disabled?: boolean;
}

export const CriteriaManager: React.FC<CriteriaManagerProps> = ({
  criteria,
  onChange,
  disabled = false,
}) => {
  const [newCriterion, setNewCriterion] = useState<Partial<Criterion>>({
    name: '',
    weight: 0,
    type: 'benefit',
    unit: '',
  });

  const [errors, setErrors] = useState<Record<string, string>>({});

  const validateCriterion = (criterion: Partial<Criterion>): Record<string, string> => {
    const validationErrors: Record<string, string> = {};

    if (!criterion.name || criterion.name.trim().length === 0) {
      validationErrors.name = 'Name is required';
    }

    if (criterion.weight === undefined || criterion.weight <= 0) {
      validationErrors.weight = 'Weight must be greater than 0';
    }

    if (criterion.weight && criterion.weight > 1) {
      validationErrors.weight = 'Weight must be between 0 and 1';
    }

    return validationErrors;
  };

  const handleAddCriterion = () => {
    const validationErrors = validateCriterion(newCriterion);

    if (Object.keys(validationErrors).length > 0) {
      setErrors(validationErrors);
      return;
    }

    // Check if total weight would exceed 1
    const totalWeight = criteria.reduce((sum, c) => sum + c.weight, 0);
    const newTotalWeight = totalWeight + (newCriterion.weight || 0);

    if (newTotalWeight > 1) {
      setErrors({ weight: `Total weight would exceed 1.0 (current: ${totalWeight.toFixed(3)})` });
      return;
    }

    onChange([...criteria, newCriterion as Criterion]);
    setNewCriterion({ name: '', weight: 0, type: 'benefit', unit: '' });
    setErrors({});
  };

  const handleRemoveCriterion = (index: number) => {
    const updated = criteria.filter((_, i) => i !== index);
    onChange(updated);
  };

  const handleUpdateCriterion = (index: number, field: keyof Criterion, value: any) => {
    const updated = [...criteria];
    updated[index] = { ...updated[index], [field]: value };
    onChange(updated);
  };

  const totalWeight = criteria.reduce((sum, c) => sum + c.weight, 0);
  const isWeightValid = Math.abs(totalWeight - 1.0) < 0.001; // Allow small floating point errors

  return (
    <div className="space-y-4">
      <div className="flex items-center justify-between">
        <div>
          <h3 className="text-lg font-semibold text-gray-900">Criteria</h3>
          <p className="text-sm text-gray-600">
            Define evaluation criteria (weights must sum to 1.0)
          </p>
        </div>
        <div className={clsx(
          'px-3 py-1 rounded-full text-sm font-medium',
          isWeightValid ? 'bg-green-100 text-green-700' : 'bg-orange-100 text-orange-700'
        )}>
          Total Weight: {totalWeight.toFixed(3)}
        </div>
      </div>

      {/* Existing Criteria */}
      <div className="space-y-2">
        {criteria.map((criterion, index) => (
          <div
            key={index}
            className="flex items-center gap-3 p-3 bg-gray-50 rounded-lg border border-gray-200"
          >
            <div className="flex-1 grid grid-cols-4 gap-3">
              <input
                type="text"
                value={criterion.name}
                onChange={(e) => handleUpdateCriterion(index, 'name', e.target.value)}
                disabled={disabled}
                className="px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100"
              />

              <input
                type="number"
                value={criterion.weight}
                onChange={(e) => handleUpdateCriterion(index, 'weight', parseFloat(e.target.value) || 0)}
                disabled={disabled}
                step="0.01"
                min="0"
                max="1"
                className="px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100"
              />

              <select
                value={criterion.type}
                onChange={(e) => handleUpdateCriterion(index, 'type', e.target.value)}
                disabled={disabled}
                className="px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100"
              >
                <option value="benefit">Benefit (Higher is better)</option>
                <option value="cost">Cost (Lower is better)</option>
              </select>

              <input
                type="text"
                value={criterion.unit || ''}
                onChange={(e) => handleUpdateCriterion(index, 'unit', e.target.value)}
                disabled={disabled}
                className="px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100"
              />
            </div>

            <div className="flex items-center gap-2">
              {criterion.type === 'benefit' ? (
                <TrendingUp className="w-5 h-5 text-green-600" />
              ) : (
                <TrendingDown className="w-5 h-5 text-red-600" />
              )}

              <button
                onClick={() => handleRemoveCriterion(index)}
                disabled={disabled}
                className="p-2 text-red-600 hover:bg-red-50 rounded-lg transition-colors disabled:opacity-50"
              >
                <X className="w-5 h-5" />
              </button>
            </div>
          </div>
        ))}
      </div>

      {/* Add New Criterion */}
      <div className="p-4 bg-blue-50 rounded-lg border border-blue-200">
        <div className="flex items-start gap-3">
          <div className="flex-1 grid grid-cols-4 gap-3">
            <div>
              <input
                type="text"
                value={newCriterion.name}
                onChange={(e) => setNewCriterion({ ...newCriterion, name: e.target.value })}
                disabled={disabled}
                className={clsx(
                  'w-full px-3 py-2 border rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100',
                  errors.name ? 'border-red-300' : 'border-gray-300'
                )}
              />
              {errors.name && (
                <p className="text-xs text-red-600 mt-1">{errors.name}</p>
              )}
            </div>

            <div>
              <input
                type="number"
                value={newCriterion.weight || ''}
                onChange={(e) => setNewCriterion({ ...newCriterion, weight: parseFloat(e.target.value) || 0 })}
                disabled={disabled}
                step="0.01"
                min="0"
                max="1"
                className={clsx(
                  'w-full px-3 py-2 border rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100',
                  errors.weight ? 'border-red-300' : 'border-gray-300'
                )}
              />
              {errors.weight && (
                <p className="text-xs text-red-600 mt-1">{errors.weight}</p>
              )}
            </div>

            <select
              value={newCriterion.type}
              onChange={(e) => setNewCriterion({ ...newCriterion, type: e.target.value as 'cost' | 'benefit' })}
              disabled={disabled}
              className="px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100"
            >
              <option value="benefit">Benefit</option>
              <option value="cost">Cost</option>
            </select>

            <input
              type="text"
              value={newCriterion.unit || ''}
              onChange={(e) => setNewCriterion({ ...newCriterion, unit: e.target.value })}
              disabled={disabled}
              className="px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 disabled:bg-gray-100"
            />
          </div>

          <button
            onClick={handleAddCriterion}
            disabled={disabled}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors disabled:opacity-50 disabled:cursor-not-allowed"
          >
            <Plus className="w-5 h-5" />
            Add
          </button>
        </div>
      </div>

      {!isWeightValid && criteria.length > 0 && (
        <div className="p-3 bg-orange-50 border border-orange-200 rounded-lg text-sm text-orange-800">
          ⚠️ Warning: Total weight must equal 1.0. Current total: {totalWeight.toFixed(3)}
        </div>
      )}
    </div>
  );
};

export default CriteriaManager;
