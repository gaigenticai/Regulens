/**
 * Decision Engine Page
 * Complete MCDA decision creation interface
 * NO MOCKS - Real backend calculations and visualizations
 */

import React, { useState } from 'react';
import { GitBranch, Play, Save, Eye, AlertCircle, Zap, BarChart3, TrendingUp } from 'lucide-react';
import { useNavigate } from 'react-router-dom';
import { AlgorithmSelector } from '@/components/DecisionEngine/AlgorithmSelector';
import { CriteriaManager } from '@/components/DecisionEngine/CriteriaManager';
import { AlternativesManager } from '@/components/DecisionEngine/AlternativesManager';
import { DecisionTreeVisualization } from '@/components/DecisionTree/DecisionTreeVisualization';
import { useCreateDecision } from '@/hooks/useDecisions';
import { useDecisionTree } from '@/hooks/useDecisionTree';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { apiClient } from '@/services/api';
import type { MCDAAlgorithm, Criterion, Alternative } from '@/types/api';

const DecisionEngine: React.FC = () => {
  const navigate = useNavigate();
  const [step, setStep] = useState<'algorithm' | 'criteria' | 'alternatives' | 'preview' | 'advanced'>('algorithm');

  // Form state
  const [title, setTitle] = useState('');
  const [description, setDescription] = useState('');
  const [algorithm, setAlgorithm] = useState<MCDAAlgorithm>('TOPSIS');
  const [criteria, setCriteria] = useState<Criterion[]>([]);
  const [alternatives, setAlternatives] = useState<Alternative[]>([]);

  // Week 2: Advanced features
  const [executionMode, setExecutionMode] = useState<'SYNCHRONOUS' | 'ASYNCHRONOUS' | 'BATCH'>('SYNCHRONOUS');
  const [useEnsemble, setUseEnsemble] = useState(false);
  const [ensembleAlgorithms, setEnsembleAlgorithms] = useState<string[]>([algorithm]);
  const [aggregationMethod, setAggregationMethod] = useState<'AVERAGE' | 'WEIGHTED' | 'MEDIAN' | 'BORDA_COUNT'>('AVERAGE');
  const [analysisJobId, setAnalysisJobId] = useState<string | null>(null);
  const [jobStatus, setJobStatus] = useState<string | null>(null);
  const [apiError, setApiError] = useState<string | null>(null);
  const [availableAlgorithms, setAvailableAlgorithms] = useState<string[]>([]);

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

  // Fetch available algorithms
  const loadAvailableAlgorithms = async () => {
    try {
      const algos = await apiClient.getAvailableAlgorithms();
      setAvailableAlgorithms(algos || []);
    } catch (err) {
      console.warn('Failed to load algorithms:', err);
      setAvailableAlgorithms(['TOPSIS', 'AHP', 'PROMETHEE', 'ELECTRE', 'WEIGHTED_SUM', 'WEIGHTED_PRODUCT', 'VIKOR']);
    }
  };

  // Launch async/ensemble analysis
  const handleAdvancedAnalysis = async () => {
    if (!canSave) return;

    setApiError(null);
    setAnalysisJobId(null);
    setJobStatus(null);

    try {
      let jobId: string;

      if (useEnsemble) {
        // Ensemble analysis
        const res = await apiClient.analyzeDecisionEnsemble({
          title: title.trim(),
          criteria: criteria.map(c => ({
            name: c.name,
            weight: c.weight,
            direction: 'maximize', // Default direction
          })),
          alternatives: alternatives.map(a => ({
            name: a.name,
            scores: a.scores,
          })),
          algorithms: ensembleAlgorithms.length > 0 ? ensembleAlgorithms : [algorithm],
          aggregation_method: aggregationMethod,
        });
        jobId = res.job_id;
      } else {
        // Async analysis
        const res = await apiClient.analyzeDecisionAsync({
          title: title.trim(),
          description: description.trim(),
          algorithm,
          criteria: criteria.map(c => ({
            name: c.name,
            weight: c.weight,
            direction: 'maximize',
          })),
          alternatives: alternatives.map(a => ({
            name: a.name,
            scores: a.scores,
          })),
          execution_mode: executionMode,
        });
        jobId = res.job_id;
      }

      setAnalysisJobId(jobId);
      setJobStatus('PENDING');

      // Poll for result
      const pollInterval = setInterval(async () => {
        try {
          const result = await apiClient.getAnalysisResult(jobId);
          if (result.status === 'COMPLETED' || result.status === 'FAILED') {
            clearInterval(pollInterval);
            setJobStatus(result.status);
            if (result.status === 'COMPLETED') {
              // Navigate to results
              setTimeout(() => {
                navigate(`/decisions/${jobId}`);
              }, 1500);
            }
          } else {
            setJobStatus(result.status);
          }
        } catch (err) {
          console.error('Poll error:', err);
        }
      }, 2000);
    } catch (err) {
      setApiError(err instanceof Error ? err.message : 'Failed to launch analysis');
    }
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
            Create multi-criteria decisions using advanced algorithms with async & ensemble support
          </p>
        </div>
      </div>

      {/* Progress Steps */}
      <div className="flex items-center gap-2">
        {[
          { id: 'algorithm', label: 'Algorithm' },
          { id: 'criteria', label: 'Criteria' },
          { id: 'alternatives', label: 'Alternatives' },
          { id: 'preview', label: 'Preview' },
          { id: 'advanced', label: 'Advanced Options' },
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
              {s.label}
            </button>
            {index < 4 && <div className="w-2 h-2 bg-gray-300 rounded-full" />}
          </React.Fragment>
        ))}
      </div>

      {/* Content */}
      <div className="bg-white rounded-lg shadow p-6">
        {step === 'algorithm' && (
          <div className="space-y-6">
            <h2 className="text-lg font-semibold text-gray-900">Select MCDA Algorithm</h2>
            <AlgorithmSelector value={algorithm} onChange={setAlgorithm} />
            <div className="flex justify-end">
              <button
                onClick={() => setStep('criteria')}
                disabled={!canProceedToCriteria}
                className="px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors font-medium disabled:opacity-50"
              >
                Next: Define Criteria
              </button>
            </div>
          </div>
        )}

        {step === 'criteria' && (
          <div className="space-y-6">
            <h2 className="text-lg font-semibold text-gray-900">Define Evaluation Criteria</h2>
            <CriteriaManager criteria={criteria} onChange={setCriteria} />
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
                className="px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors font-medium disabled:opacity-50"
              >
                Next: Add Alternatives
              </button>
            </div>
          </div>
        )}

        {step === 'alternatives' && (
          <div className="space-y-6">
            <h2 className="text-lg font-semibold text-gray-900">Add Alternatives & Scores</h2>
            <AlternativesManager alternatives={alternatives} onChange={setAlternatives} criteria={criteria} />
            <div className="flex justify-between">
              <button
                onClick={() => setStep('criteria')}
                className="px-6 py-3 bg-gray-200 text-gray-700 rounded-lg hover:bg-gray-300 transition-colors font-medium"
              >
                Back
              </button>
              <button
                onClick={() => setStep('preview')}
                disabled={!canPreview}
                className="px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 transition-colors font-medium disabled:opacity-50"
              >
                Next: Preview
              </button>
            </div>
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
                  onClick={() => setStep('advanced')}
                  className="flex items-center gap-2 px-6 py-3 bg-purple-600 text-white rounded-lg hover:bg-purple-700 transition-colors font-medium"
                >
                  <Zap className="w-5 h-5" />
                  Advanced Options
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

        {step === 'advanced' && (
          <div className="space-y-6">
            <h2 className="text-lg font-semibold text-gray-900">Week 2: Advanced Analysis Options</h2>

            {/* Execution Mode */}
            <div className="p-4 bg-blue-50 rounded-lg border border-blue-200">
              <label className="block text-sm font-medium text-gray-900 mb-3">Execution Mode</label>
              <div className="space-y-2">
                {(['SYNCHRONOUS', 'ASYNCHRONOUS', 'BATCH'] as const).map((mode) => (
                  <label key={mode} className="flex items-center gap-2">
                    <input
                      type="radio"
                      name="execution"
                      value={mode}
                      checked={executionMode === mode}
                      onChange={(e) => setExecutionMode(e.target.value as any)}
                      className="w-4 h-4"
                    />
                    <span className="text-sm text-gray-700">{mode}</span>
                  </label>
                ))}
              </div>
              <p className="text-xs text-gray-600 mt-2">
                Synchronous: Immediate result | Asynchronous: Job-based execution | Batch: Multiple analyses
              </p>
            </div>

            {/* Ensemble Analysis */}
            <div className="p-4 bg-purple-50 rounded-lg border border-purple-200">
              <label className="flex items-center gap-2 mb-3">
                <input
                  type="checkbox"
                  checked={useEnsemble}
                  onChange={(e) => setUseEnsemble(e.target.checked)}
                  className="w-4 h-4"
                />
                <span className="text-sm font-medium text-gray-900">Use Ensemble Analysis</span>
              </label>

              {useEnsemble && (
                <div className="space-y-3">
                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      Select Algorithms:
                    </label>
                    <div className="space-y-2">
                      {['TOPSIS', 'AHP', 'PROMETHEE', 'ELECTRE', 'WEIGHTED_SUM', 'WEIGHTED_PRODUCT', 'VIKOR'].map((algo) => (
                        <label key={algo} className="flex items-center gap-2">
                          <input
                            type="checkbox"
                            checked={ensembleAlgorithms.includes(algo)}
                            onChange={(e) => {
                              if (e.target.checked) {
                                setEnsembleAlgorithms([...ensembleAlgorithms, algo]);
                              } else {
                                setEnsembleAlgorithms(ensembleAlgorithms.filter(a => a !== algo));
                              }
                            }}
                            className="w-4 h-4"
                          />
                          <span className="text-sm text-gray-700">{algo}</span>
                        </label>
                      ))}
                    </div>
                  </div>

                  <div>
                    <label className="block text-sm font-medium text-gray-700 mb-2">
                      Aggregation Method:
                    </label>
                    <select
                      value={aggregationMethod}
                      onChange={(e) => setAggregationMethod(e.target.value as any)}
                      className="w-full px-3 py-2 border border-gray-300 rounded-lg"
                    >
                      <option value="AVERAGE">Average</option>
                      <option value="WEIGHTED">Weighted Average</option>
                      <option value="MEDIAN">Median</option>
                      <option value="BORDA_COUNT">Borda Count</option>
                    </select>
                  </div>
                </div>
              )}
            </div>

            {/* Job Status */}
            {analysisJobId && (
              <div className={`p-4 rounded-lg border ${
                jobStatus === 'COMPLETED' ? 'bg-green-50 border-green-200' :
                jobStatus === 'FAILED' ? 'bg-red-50 border-red-200' :
                'bg-blue-50 border-blue-200'
              }`}>
                <p className="text-sm font-medium mb-2">
                  {jobStatus === 'COMPLETED' ? '✓ Analysis Complete!' :
                   jobStatus === 'FAILED' ? '✗ Analysis Failed' :
                   `📊 Analysis Status: ${jobStatus}`}
                </p>
                <p className="text-xs text-gray-600">Job ID: {analysisJobId}</p>
              </div>
            )}

            {apiError && (
              <div className="p-4 bg-red-50 border border-red-200 rounded-lg text-red-700 text-sm">
                {apiError}
              </div>
            )}

            {/* Actions */}
            <div className="flex justify-between">
              <button
                onClick={() => setStep('preview')}
                disabled={isSaving || jobStatus === 'RUNNING'}
                className="px-6 py-3 bg-gray-200 text-gray-700 rounded-lg hover:bg-gray-300 transition-colors font-medium disabled:opacity-50"
              >
                Back
              </button>

              <button
                onClick={handleAdvancedAnalysis}
                disabled={!canSave || isSaving || jobStatus === 'RUNNING'}
                className="flex items-center gap-2 px-6 py-3 bg-purple-600 text-white rounded-lg hover:bg-purple-700 transition-colors font-medium disabled:opacity-50"
              >
                {jobStatus === 'RUNNING' ? (
                  <>
                    <div className="w-5 h-5 border-2 border-white border-t-transparent rounded-full animate-spin" />
                    Running...
                  </>
                ) : (
                  <>
                    <Zap className="w-5 h-5" />
                    {useEnsemble ? 'Run Ensemble Analysis' : 'Run Advanced Analysis'}
                  </>
                )}
              </button>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default DecisionEngine;
