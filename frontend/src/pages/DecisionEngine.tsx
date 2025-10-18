/**
 * Decision Engine Page
 * Complete MCDA decision creation interface
 * NO MOCKS - Real backend calculations and visualizations
 */

import React, { useState } from 'react';
import { GitBranch, Play, Save, Eye, AlertCircle } from 'lucide-react';
import { useNavigate } from 'react-router-dom';
import { AlgorithmSelector } from '@/components/DecisionEngine/AlgorithmSelector';
import { CriteriaManager } from '@/components/DecisionEngine/CriteriaManager';
import { AlternativesManager } from '@/components/DecisionEngine/AlternativesManager';
import { DecisionTreeVisualization } from '@/components/DecisionTree/DecisionTreeVisualization';
import { useCreateDecision } from '@/hooks/useDecisions';
import { useDecisionTree } from '@/hooks/useDecisionTree';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import type { MCDAAlgorithm, Criterion, Alternative } from '@/types/api';

const DecisionEngine: React.FC = () => {
  const navigate = useNavigate();
  const [step, setStep] = useState<'algorithm' | 'criteria' | 'alternatives' | 'preview'>('algorithm');

  // Form state
  const [title, setTitle] = useState('');
  const [description, setDescription] = useState('');
  const [algorithm, setAlgorithm] = useState<MCDAAlgorithm>('TOPSIS');
  const [criteria, setCriteria] = useState<Criterion[]>([]);
  const [alternatives, setAlternatives] = useState<Alternative[]>([]);

  // API hooks
  const { createDecision, isLoading: isSaving, isError: isSaveError, error: saveError } = useCreateDecision();

  // Preview visualization
  const [showPreview, setShowPreview] = useState(false);
  const { tree, isLoading: isLoadingTree, isError: isTreeError } = useDecisionTree({
    visualizeData: showPreview && criteria.length > 0 && alternatives.length > 0
      ? { algorithm, criteria, alternatives }
      : undefined,
  });

  // Validation
  const canProceedToCriteria = algorithm !== undefined;
  const canProceedToAlternatives = criteria.length > 0 && Math.abs(criteria.reduce((sum, c) => sum + c.weight, 0) - 1.0) < 0.001;
  const canPreview = alternatives.length >= 2 && criteria.every(c => alternatives.every(a => c.name in a.scores));
  const canSave = canPreview && title.trim().length > 0 && description.trim().length > 0;

  const handlePreview = () => {
    setShowPreview(true);
    setStep('preview');
  };

  const handleSave = async () => {
    if (!canSave) return;

    try {
      const decision = await createDecision({
        title: title.trim(),
        description: description.trim(),
        algorithm,
        criteria,
        alternatives,
      });

      // Navigate to the decision detail page
      navigate(`/decisions/${decision.id}`);
    } catch (error) {
      console.error('Failed to create decision:', error);
    }
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900 flex items-center gap-2">
            <GitBranch className="w-7 h-7 text-blue-600" />
            MCDA Decision Engine
          </h1>
          <p className="text-gray-600 mt-1">
            Create multi-criteria decisions using advanced algorithms
          </p>
        </div>
      </div>

      {/* Progress Steps */}
      <div className="flex items-center gap-2">
        {[
          { id: 'algorithm', label: 'Algorithm' },
          { id: 'criteria', label: 'Criteria' },
          { id: 'alternatives', label: 'Alternatives' },
          { id: 'preview', label: 'Preview & Save' },
        ].map((s, index) => (
          <React.Fragment key={s.id}>
            <button
              onClick={() => setStep(s.id as any)}
              className={`px-4 py-2 rounded-lg font-medium transition-colors ${
                step === s.id
                  ? 'bg-blue-600 text-white'
                  : 'bg-gray-100 text-gray-600 hover:bg-gray-200'
              }`}
            >
              {index + 1}. {s.label}
            </button>
            {index < 3 && <div className="w-8 h-0.5 bg-gray-300" />}
          </React.Fragment>
        ))}
      </div>

      {/* Step Content */}
      <div className="bg-white rounded-lg border border-gray-200 p-6">
        {step === 'algorithm' && (
          <div className="space-y-6">
            <AlgorithmSelector
              selected={algorithm}
              onChange={setAlgorithm}
            />

            <div className="flex justify-end">
              <button
                onClick={() => setStep('criteria')}
                disabled={!canProceedToCriteria}
                className="px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors disabled:opacity-50 disabled:cursor-not-allowed font-medium"
              >
                Continue to Criteria
              </button>
            </div>
          </div>
        )}

        {step === 'criteria' && (
          <div className="space-y-6">
            <CriteriaManager
              criteria={criteria}
              onChange={setCriteria}
            />

            <div className="flex justify-between">
              <button
                onClick={() => setStep('algorithm')}
                className="px-6 py-3 bg-gray-200 text-gray-700 rounded-lg hover:bg-gray-300 transition-colors font-medium"
              >
                Back
              </button>

              <button
                onClick={() => setStep('alternatives')}
                disabled={!canProceedToAlternatives}
                className="px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors disabled:opacity-50 disabled:cursor-not-allowed font-medium"
              >
                Continue to Alternatives
              </button>
            </div>

            {!canProceedToAlternatives && criteria.length > 0 && (
              <div className="p-3 bg-orange-50 border border-orange-200 rounded-lg text-sm text-orange-800">
                <AlertCircle className="w-4 h-4 inline mr-2" />
                Criteria weights must sum to exactly 1.0 before proceeding
              </div>
            )}
          </div>
        )}

        {step === 'alternatives' && (
          <div className="space-y-6">
            <AlternativesManager
              alternatives={alternatives}
              criteria={criteria}
              onChange={setAlternatives}
            />

            <div className="flex justify-between">
              <button
                onClick={() => setStep('criteria')}
                className="px-6 py-3 bg-gray-200 text-gray-700 rounded-lg hover:bg-gray-300 transition-colors font-medium"
              >
                Back
              </button>

              <button
                onClick={handlePreview}
                disabled={!canPreview}
                className="flex items-center gap-2 px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors disabled:opacity-50 disabled:cursor-not-allowed font-medium"
              >
                <Eye className="w-5 h-5" />
                Preview Decision
              </button>
            </div>

            {!canPreview && alternatives.length > 0 && (
              <div className="p-3 bg-orange-50 border border-orange-200 rounded-lg text-sm text-orange-800">
                <AlertCircle className="w-4 h-4 inline mr-2" />
                You need at least 2 alternatives with all scores filled before previewing
              </div>
            )}
          </div>
        )}

        {step === 'preview' && (
          <div className="space-y-6">
            {/* Title and Description */}
            <div className="space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Decision Title *
                </label>
                <input
                  type="text"
                  value={title}
                  onChange={(e) => setTitle(e.target.value)}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Description *
                </label>
                <textarea
                  value={description}
                  onChange={(e) => setDescription(e.target.value)}
                  rows={3}
                  className="w-full px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
                />
              </div>
            </div>

            {/* Visualization */}
            {isLoadingTree && (
              <LoadingSpinner message="Generating decision tree visualization..." />
            )}

            {isTreeError && (
              <div className="p-4 bg-red-50 border border-red-200 rounded-lg">
                <p className="text-red-800">Failed to generate visualization. Please check your inputs.</p>
              </div>
            )}

            {tree && (
              <DecisionTreeVisualization
                tree={tree}
                width={900}
                height={600}
              />
            )}

            {/* Summary */}
            <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
              <div className="p-4 bg-blue-50 rounded-lg border border-blue-200">
                <p className="text-sm font-medium text-blue-900">Algorithm</p>
                <p className="text-lg font-bold text-blue-700">{algorithm}</p>
              </div>

              <div className="p-4 bg-green-50 rounded-lg border border-green-200">
                <p className="text-sm font-medium text-green-900">Criteria</p>
                <p className="text-lg font-bold text-green-700">{criteria.length}</p>
              </div>

              <div className="p-4 bg-purple-50 rounded-lg border border-purple-200">
                <p className="text-sm font-medium text-purple-900">Alternatives</p>
                <p className="text-lg font-bold text-purple-700">{alternatives.length}</p>
              </div>
            </div>

            {/* Actions */}
            <div className="flex justify-between">
              <button
                onClick={() => setStep('alternatives')}
                disabled={isSaving}
                className="px-6 py-3 bg-gray-200 text-gray-700 rounded-lg hover:bg-gray-300 transition-colors font-medium disabled:opacity-50"
              >
                Back
              </button>

              <div className="flex gap-3">
                <button
                  onClick={handlePreview}
                  disabled={isLoadingTree || isSaving}
                  className="flex items-center gap-2 px-6 py-3 bg-white border border-gray-300 text-gray-700 rounded-lg hover:bg-gray-50 transition-colors font-medium disabled:opacity-50"
                >
                  <Play className="w-5 h-5" />
                  Recalculate
                </button>

                <button
                  onClick={handleSave}
                  disabled={!canSave || isSaving}
                  className="flex items-center gap-2 px-6 py-3 bg-green-600 text-white rounded-lg hover:bg-green-700 transition-colors disabled:opacity-50 disabled:cursor-not-allowed font-medium"
                >
                  {isSaving ? (
                    <>
                      <div className="w-5 h-5 border-2 border-white border-t-transparent rounded-full animate-spin" />
                      Saving...
                    </>
                  ) : (
                    <>
                      <Save className="w-5 h-5" />
                      Save Decision
                    </>
                  )}
                </button>
              </div>
            </div>

            {isSaveError && (
              <div className="p-4 bg-red-50 border border-red-200 rounded-lg">
                <p className="text-red-800 font-medium">Failed to save decision</p>
                <p className="text-red-700 text-sm mt-1">{saveError?.message || 'Unknown error'}</p>
              </div>
            )}

            {!canSave && (
              <div className="p-3 bg-orange-50 border border-orange-200 rounded-lg text-sm text-orange-800">
                <AlertCircle className="w-4 h-4 inline mr-2" />
                Please provide a title and description before saving
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

export default DecisionEngine;
