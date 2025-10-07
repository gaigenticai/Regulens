/**
 * Decision Detail Page
 * Shows complete decision analysis with D3 visualization
 * NO MOCKS - All data from backend API
 */

import React from 'react';
import { useParams, Link } from 'react-router-dom';
import { ArrowLeft, GitBranch, TrendingUp, Award } from 'lucide-react';
import { formatDistanceToNow } from 'date-fns';
import { useDecisionDetail } from '@/hooks/useDecisions';
import { useDecisionTree } from '@/hooks/useDecisionTree';
import { DecisionTreeVisualization } from '@/components/DecisionTree/DecisionTreeVisualization';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';

const DecisionDetail: React.FC = () => {
  const { id } = useParams<{ id: string }>();

  const { decision, isLoading, isError } = useDecisionDetail(id!);
  const { tree, isLoading: isTreeLoading } = useDecisionTree({ decisionId: id });

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading decision..." />;
  }

  if (isError || !decision) {
    return (
      <div className="flex items-center justify-center min-h-[60vh]">
        <div className="text-center">
          <h2 className="text-2xl font-bold text-gray-900 mb-2">Decision Not Found</h2>
          <p className="text-gray-600 mb-6">The decision you're looking for doesn't exist.</p>
          <Link
            to="/decisions"
            className="inline-flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
          >
            <ArrowLeft className="w-5 h-5" />
            Back to Decisions
          </Link>
        </div>
      </div>
    );
  }

  // Find the selected alternative
  const selectedAlt = decision.alternatives.find(a => a.id === decision.selectedAlternative);

  // Calculate ranks if not present
  const rankedAlternatives = [...decision.alternatives].sort((a, b) => {
    if (a.rank !== undefined && b.rank !== undefined) {
      return a.rank - b.rank;
    }
    if (a.similarity !== undefined && b.similarity !== undefined) {
      return b.similarity - a.similarity;
    }
    if (a.netFlow !== undefined && b.netFlow !== undefined) {
      return b.netFlow - a.netFlow;
    }
    return 0;
  });

  const statusColors = {
    draft: 'bg-gray-100 text-gray-700',
    pending: 'bg-yellow-100 text-yellow-700',
    approved: 'bg-green-100 text-green-700',
    rejected: 'bg-red-100 text-red-700',
    implemented: 'bg-blue-100 text-blue-700',
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div>
        <Link
          to="/decisions"
          className="inline-flex items-center gap-2 text-blue-600 hover:text-blue-700 mb-4"
        >
          <ArrowLeft className="w-5 h-5" />
          Back to Decisions
        </Link>

        <div className="flex items-start justify-between">
          <div className="flex-1">
            <h1 className="text-2xl font-bold text-gray-900 mb-2">{decision.title}</h1>
            <p className="text-gray-600">{decision.description}</p>
          </div>

          <div className={clsx('px-3 py-1 rounded-full text-sm font-medium capitalize', statusColors[decision.status])}>
            {decision.status}
          </div>
        </div>

        <div className="flex items-center gap-4 mt-4 text-sm text-gray-600">
          <span>Created {formatDistanceToNow(new Date(decision.timestamp), { addSuffix: true })}</span>
          <span>•</span>
          <span>By {decision.decisionMaker}</span>
          {decision.approvedBy && (
            <>
              <span>•</span>
              <span>Approved by {decision.approvedBy}</span>
            </>
          )}
        </div>
      </div>

      {/* Key Metrics */}
      <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
        <div className="bg-white rounded-lg border border-gray-200 p-4">
          <div className="flex items-center gap-2 mb-2">
            <GitBranch className="w-5 h-5 text-blue-600" />
            <p className="text-sm font-medium text-gray-600">Algorithm</p>
          </div>
          <p className="text-xl font-bold text-gray-900">{decision.algorithm}</p>
        </div>

        <div className="bg-white rounded-lg border border-gray-200 p-4">
          <div className="flex items-center gap-2 mb-2">
            <TrendingUp className="w-5 h-5 text-green-600" />
            <p className="text-sm font-medium text-gray-600">Confidence</p>
          </div>
          <p className="text-xl font-bold text-green-700">
            {(decision.confidenceScore * 100).toFixed(1)}%
          </p>
        </div>

        <div className="bg-white rounded-lg border border-gray-200 p-4">
          <p className="text-sm font-medium text-gray-600 mb-2">Criteria</p>
          <p className="text-xl font-bold text-gray-900">{decision.criteria.length}</p>
        </div>

        <div className="bg-white rounded-lg border border-gray-200 p-4">
          <p className="text-sm font-medium text-gray-600 mb-2">Alternatives</p>
          <p className="text-xl font-bold text-gray-900">{decision.alternatives.length}</p>
        </div>
      </div>

      {/* Selected Alternative */}
      {selectedAlt && (
        <div className="bg-gradient-to-r from-green-50 to-blue-50 rounded-lg border-2 border-green-300 p-6">
          <div className="flex items-center gap-3 mb-3">
            <Award className="w-6 h-6 text-green-600" />
            <h2 className="text-xl font-bold text-gray-900">Recommended Alternative</h2>
          </div>
          <p className="text-2xl font-bold text-green-700 mb-2">{selectedAlt.name}</p>
          {selectedAlt.rank !== undefined && (
            <p className="text-sm text-gray-600">Rank: #{selectedAlt.rank}</p>
          )}
          {selectedAlt.similarity !== undefined && (
            <p className="text-sm text-gray-600">Similarity Score: {selectedAlt.similarity.toFixed(4)}</p>
          )}
          {selectedAlt.netFlow !== undefined && (
            <p className="text-sm text-gray-600">Net Flow: {selectedAlt.netFlow.toFixed(4)}</p>
          )}
        </div>
      )}

      {/* Decision Tree Visualization */}
      {isTreeLoading ? (
        <LoadingSpinner message="Loading decision tree..." />
      ) : tree ? (
        <DecisionTreeVisualization tree={tree} width={1000} height={700} />
      ) : null}

      {/* Criteria Table */}
      <div className="bg-white rounded-lg border border-gray-200 p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Criteria</h3>
        <div className="overflow-x-auto">
          <table className="w-full">
            <thead>
              <tr className="border-b border-gray-200">
                <th className="px-4 py-3 text-left text-sm font-semibold text-gray-900">Name</th>
                <th className="px-4 py-3 text-center text-sm font-semibold text-gray-900">Weight</th>
                <th className="px-4 py-3 text-center text-sm font-semibold text-gray-900">Type</th>
                <th className="px-4 py-3 text-center text-sm font-semibold text-gray-900">Unit</th>
              </tr>
            </thead>
            <tbody>
              {decision.criteria.map((criterion, index) => (
                <tr key={index} className="border-b border-gray-100">
                  <td className="px-4 py-3 text-sm text-gray-900">{criterion.name}</td>
                  <td className="px-4 py-3 text-sm text-gray-900 text-center font-mono">
                    {criterion.weight.toFixed(3)}
                  </td>
                  <td className="px-4 py-3 text-center">
                    <span className={clsx(
                      'px-2 py-1 rounded-full text-xs font-medium',
                      criterion.type === 'benefit' ? 'bg-green-100 text-green-700' : 'bg-red-100 text-red-700'
                    )}>
                      {criterion.type}
                    </span>
                  </td>
                  <td className="px-4 py-3 text-sm text-gray-600 text-center">
                    {criterion.unit || '-'}
                  </td>
                </tr>
              ))}
            </tbody>
          </table>
        </div>
      </div>

      {/* Alternatives Ranking */}
      <div className="bg-white rounded-lg border border-gray-200 p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Alternatives Ranking</h3>
        <div className="space-y-3">
          {rankedAlternatives.map((alt, index) => {
            const isSelected = alt.id === decision.selectedAlternative;

            return (
              <div
                key={alt.id}
                className={clsx(
                  'p-4 rounded-lg border-2',
                  isSelected ? 'border-green-500 bg-green-50' : 'border-gray-200 bg-white'
                )}
              >
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-3">
                    <div className={clsx(
                      'w-8 h-8 rounded-full flex items-center justify-center font-bold',
                      isSelected ? 'bg-green-600 text-white' : 'bg-gray-200 text-gray-700'
                    )}>
                      {index + 1}
                    </div>
                    <div>
                      <h4 className="font-semibold text-gray-900">{alt.name}</h4>
                      {isSelected && (
                        <span className="text-xs font-medium text-green-600">Recommended</span>
                      )}
                    </div>
                  </div>

                  <div className="text-right">
                    {alt.similarity !== undefined && (
                      <p className="text-sm text-gray-600">Similarity: {alt.similarity.toFixed(4)}</p>
                    )}
                    {alt.netFlow !== undefined && (
                      <p className="text-sm text-gray-600">Net Flow: {alt.netFlow.toFixed(4)}</p>
                    )}
                    {alt.rank !== undefined && (
                      <p className="text-sm text-gray-600">Rank: {alt.rank}</p>
                    )}
                  </div>
                </div>

                {/* Scores */}
                <div className="mt-3 grid grid-cols-2 md:grid-cols-4 gap-2">
                  {Object.entries(alt.scores).map(([criterionName, score]) => (
                    <div key={criterionName} className="text-xs">
                      <span className="text-gray-600">{criterionName}:</span>{' '}
                      <span className="font-mono font-medium text-gray-900">{score.toFixed(2)}</span>
                    </div>
                  ))}
                </div>
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
};

export default DecisionDetail;
