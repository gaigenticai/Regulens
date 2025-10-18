/**
 * Collaboration Session Detail Page
 * Real-time agent reasoning stream and human override controls
 * Production-grade implementation - NO MOCKS
 */

import React, { useState } from 'react';
import { useParams, Link } from 'react-router-dom';
import {
  ArrowLeft,
  Brain,
  Users,
  AlertCircle,
  CheckCircle,
  Activity,
  TrendingUp,
  RefreshCw,
} from 'lucide-react';
import {
  useCollaborationSession,
  useSessionReasoningSteps,
  useRecordHumanOverride,
} from '@/hooks/useCollaborationSessions';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { useAuth } from '@/contexts/AuthContext';
import { toast } from 'react-hot-toast';

const CollaborationSessionDetail: React.FC = () => {
  const { sessionId } = useParams<{ sessionId: string }>();
  const [showOverrideModal, setShowOverrideModal] = useState(false);
  const [selectedStep, setSelectedStep] = useState<any>(null);
  const { user } = useAuth();

  const { data: session, isLoading: sessionLoading, refetch: refetchSession } = useCollaborationSession(sessionId!);
  const { data: steps = [], isLoading: stepsLoading, refetch: refetchSteps } = useSessionReasoningSteps(sessionId!);
  const recordOverride = useRecordHumanOverride();

  const handleOverride = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();

    if (!user) {
      toast.error('You must be logged in to create overrides');
      return;
    }

    const formData = new FormData(e.currentTarget);

    await recordOverride.mutateAsync({
      session_id: sessionId,
      decision_id: selectedStep?.stream_id,
      user_id: user.userId,
      user_name: user.fullName,
      original_decision: selectedStep?.reasoning_step || '',
      override_decision: formData.get('override_decision') as string,
      reason: formData.get('reason') as string,
      justification: formData.get('justification') as string,
    });

    setShowOverrideModal(false);
    setSelectedStep(null);
    e.currentTarget.reset();
  };

  if (sessionLoading || stepsLoading) {
    return <LoadingSpinner fullScreen message="Loading session..." />;
  }

  if (!session) {
    return (
      <div className="text-center py-12">
        <AlertCircle className="w-12 h-12 text-gray-400 mx-auto mb-4" />
        <h3 className="text-lg font-medium text-gray-900 mb-2">Session not found</h3>
        <Link to="/collaboration" className="text-blue-600 hover:text-blue-700">
          Back to dashboard
        </Link>
      </div>
    );
  }

  const getStepTypeColor = (stepType: string) => {
    switch (stepType) {
      case 'thinking': return 'text-blue-600 bg-blue-100';
      case 'analyzing': return 'text-purple-600 bg-purple-100';
      case 'deciding': return 'text-green-600 bg-green-100';
      case 'executing': return 'text-orange-600 bg-orange-100';
      case 'completed': return 'text-green-600 bg-green-100';
      case 'error': return 'text-red-600 bg-red-100';
      default: return 'text-gray-600 bg-gray-100';
    }
  };

  const getConfidenceColor = (score: number) => {
    if (score >= 0.8) return 'text-green-600';
    if (score >= 0.6) return 'text-yellow-600';
    return 'text-red-600';
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div className="flex items-center gap-4">
          <Link
            to="/collaboration"
            className="p-2 hover:bg-gray-100 rounded-lg transition-colors"
          >
            <ArrowLeft className="w-5 h-5" />
          </Link>
          <div>
            <div className="flex items-center gap-3">
              <h1 className="text-2xl font-bold text-gray-900">{session.title}</h1>
              <span
                className={`px-2 py-1 text-xs font-medium rounded-full ${
                  session.status === 'active'
                    ? 'bg-green-100 text-green-800'
                    : session.status === 'completed'
                    ? 'bg-blue-100 text-blue-800'
                    : 'bg-gray-100 text-gray-800'
                }`}
              >
                {session.status}
              </span>
            </div>
            <p className="text-gray-600 mt-1">{session.description}</p>
          </div>
        </div>

        <button
          onClick={() => {
            refetchSession();
            refetchSteps();
          }}
          className="flex items-center gap-2 px-4 py-2 bg-white border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
        >
          <RefreshCw className={`w-5 h-5 ${(sessionLoading || stepsLoading) ? 'animate-spin' : ''}`} />
          Refresh
        </button>
      </div>

      {/* Session Info */}
      <div className="grid grid-cols-1 md:grid-cols-3 gap-6">
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-2 mb-2">
            <Users className="w-5 h-5 text-gray-600" />
            <h3 className="text-sm font-medium text-gray-600">Created By</h3>
          </div>
          <p className="text-lg font-semibold text-gray-900">{session.created_by}</p>
          <p className="text-xs text-gray-500 mt-1">
            {new Date(session.created_at).toLocaleString()}
          </p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-2 mb-2">
            <Activity className="w-5 h-5 text-gray-600" />
            <h3 className="text-sm font-medium text-gray-600">Reasoning Steps</h3>
          </div>
          <p className="text-lg font-semibold text-gray-900">{steps.length}</p>
          <p className="text-xs text-gray-500 mt-1">Total steps recorded</p>
        </div>

        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <div className="flex items-center gap-2 mb-2">
            <TrendingUp className="w-5 h-5 text-gray-600" />
            <h3 className="text-sm font-medium text-gray-600">Avg Confidence</h3>
          </div>
          <p className={`text-lg font-semibold ${
            steps.length > 0
              ? getConfidenceColor(
                  steps.reduce((sum, s) => sum + s.confidence_score, 0) / steps.length
                )
              : 'text-gray-900'
          }`}>
            {steps.length > 0
              ? `${((steps.reduce((sum, s) => sum + s.confidence_score, 0) / steps.length) * 100).toFixed(1)}%`
              : 'N/A'}
          </p>
          <p className="text-xs text-gray-500 mt-1">Based on {steps.length} steps</p>
        </div>
      </div>

      {/* Real-time Reasoning Stream */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200 flex items-center justify-between">
          <div>
            <h2 className="text-lg font-semibold text-gray-900 flex items-center gap-2">
              <Brain className="w-5 h-5 text-purple-600" />
              Agent Reasoning Stream
            </h2>
            <p className="text-sm text-gray-600 mt-1">
              Real-time agent decision-making process (updates every 5s)
            </p>
          </div>
          {session.status === 'active' && (
            <div className="flex items-center gap-2 text-sm text-green-600">
              <div className="w-2 h-2 bg-green-500 rounded-full animate-pulse" />
              Live
            </div>
          )}
        </div>

        <div className="divide-y divide-gray-200 max-h-[600px] overflow-y-auto">
          {steps.length === 0 ? (
            <div className="p-12 text-center">
              <Brain className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No reasoning steps yet</h3>
              <p className="text-gray-600">
                Reasoning steps will appear here as agents analyze and make decisions
              </p>
            </div>
          ) : (
            steps.map((step, index) => (
              <div key={step.stream_id} className="p-6 hover:bg-gray-50 transition-colors">
                <div className="flex items-start justify-between mb-3">
                  <div className="flex items-center gap-3">
                    <div className={`px-2 py-1 text-xs font-medium rounded-full ${getStepTypeColor(step.step_type)}`}>
                      {step.step_type}
                    </div>
                    <span className="text-sm text-gray-600">
                      Step {step.step_number}
                    </span>
                    <span className="text-sm font-medium text-gray-900">
                      {step.agent_name}
                    </span>
                    <span className="text-xs text-gray-500">
                      ({step.agent_type})
                    </span>
                  </div>
                  <div className="flex items-center gap-3">
                    <div className="text-right">
                      <p className={`text-sm font-semibold ${getConfidenceColor(step.confidence_score)}`}>
                        {(step.confidence_score * 100).toFixed(1)}% confidence
                      </p>
                      <p className="text-xs text-gray-500">
                        {new Date(step.timestamp).toLocaleTimeString()}
                      </p>
                    </div>
                    <button
                      onClick={() => {
                        setSelectedStep(step);
                        setShowOverrideModal(true);
                      }}
                      className="px-3 py-1 text-sm text-orange-600 hover:bg-orange-50 rounded-lg transition-colors"
                    >
                      Override
                    </button>
                  </div>
                </div>
                <p className="text-gray-700 leading-relaxed">{step.reasoning_step}</p>
                {step.duration_ms && (
                  <p className="text-xs text-gray-500 mt-2">
                    Processed in {step.duration_ms}ms
                  </p>
                )}
              </div>
            ))
          )}
        </div>
      </div>

      {/* Human Override Modal */}
      {showOverrideModal && selectedStep && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-2xl w-full mx-4 max-h-[90vh] overflow-y-auto">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-xl font-semibold text-gray-900">Human Override</h2>
              <p className="text-sm text-gray-600 mt-1">
                Override agent decision with human judgment
              </p>
            </div>

            <form onSubmit={handleOverride} className="p-6 space-y-4">
              <div className="bg-gray-50 p-4 rounded-lg">
                <h3 className="text-sm font-medium text-gray-900 mb-2">Original Decision</h3>
                <p className="text-sm text-gray-700">{selectedStep.reasoning_step}</p>
                <div className="flex items-center gap-4 mt-2 text-xs text-gray-600">
                  <span>Agent: {selectedStep.agent_name}</span>
                  <span>Confidence: {(selectedStep.confidence_score * 100).toFixed(1)}%</span>
                  <span>{selectedStep.step_type}</span>
                </div>
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Override Decision *
                </label>
                <textarea
                  name="override_decision"
                  required
                  rows={3}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-orange-500 focus:border-orange-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Reason for Override *
                </label>
                <input
                  type="text"
                  name="reason"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-orange-500 focus:border-orange-500"
                />
              </div>

              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">
                  Justification (Optional)
                </label>
                <textarea
                  name="justification"
                  rows={2}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-orange-500 focus:border-orange-500"
                />
              </div>

              <div className="flex gap-3 pt-4">
                <button
                  type="button"
                  onClick={() => {
                    setShowOverrideModal(false);
                    setSelectedStep(null);
                  }}
                  className="flex-1 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={recordOverride.isPending}
                  className="flex-1 px-4 py-2 bg-orange-600 text-white rounded-lg hover:bg-orange-700 transition-colors disabled:opacity-50"
                >
                  {recordOverride.isPending ? 'Recording...' : 'Record Override'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};

export default CollaborationSessionDetail;

