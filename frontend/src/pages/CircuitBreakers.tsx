/**
 * Circuit Breakers Page - Production Implementation
 * Real-time circuit breaker monitoring and resilience management
 * NO MOCKS - Production-ready circuit breaker system
 */

import React from 'react';
import { Shield, ArrowLeft, Activity, AlertTriangle, CheckCircle, Clock, TrendingUp, RefreshCw } from 'lucide-react';
import { Link } from 'react-router-dom';
import { useCircuitBreakers, useCircuitBreakerStats } from '@/hooks/useCircuitBreakers';

const CircuitBreakers: React.FC = () => {
  const { data: circuitBreakers, isLoading, refetch } = useCircuitBreakers();
  const { data: stats } = useCircuitBreakerStats();

  const getStateColor = (state: string) => {
    switch (state) {
      case 'CLOSED':
        return 'bg-green-100 text-green-800 border-green-200';
      case 'OPEN':
        return 'bg-red-100 text-red-800 border-red-200';
      case 'HALF_OPEN':
        return 'bg-yellow-100 text-yellow-800 border-yellow-200';
      default:
        return 'bg-gray-100 text-gray-800 border-gray-200';
    }
  };

  const getStateIcon = (state: string) => {
    switch (state) {
      case 'CLOSED':
        return <CheckCircle className="w-5 h-5 text-green-600" />;
      case 'OPEN':
        return <AlertTriangle className="w-5 h-5 text-red-600" />;
      case 'HALF_OPEN':
        return <Clock className="w-5 h-5 text-yellow-600" />;
      default:
        return <Activity className="w-5 h-5 text-gray-600" />;
    }
  };

  const getHealthPercentage = (breaker: any) => {
    const total = breaker.success_count + breaker.failure_count;
    if (total === 0) return 100;
    return Math.round((breaker.success_count / total) * 100);
  };

  return (
    <div className="min-h-screen bg-gray-50">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {/* Header */}
        <div className="mb-8">
          <Link to="/" className="inline-flex items-center text-blue-600 hover:text-blue-700 mb-4">
            <ArrowLeft className="w-4 h-4 mr-2" />
            Back to Dashboard
          </Link>
          <div className="flex items-center justify-between">
            <div className="flex items-center space-x-4">
              <div className="p-3 bg-blue-100 rounded-lg">
                <Shield className="w-8 h-8 text-blue-600" />
              </div>
              <div>
                <h1 className="text-3xl font-bold text-gray-900">Circuit Breakers</h1>
                <p className="text-gray-600">Real-time service resilience monitoring</p>
              </div>
            </div>
            <button
              onClick={() => refetch()}
              className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 flex items-center"
            >
              <RefreshCw className="w-4 h-4 mr-2" />
              Refresh
            </button>
          </div>
        </div>

        {/* Stats Overview */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4 mb-6">
          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center mb-2">
              <Activity className="w-5 h-5 text-blue-600 mr-2" />
              <p className="text-sm text-gray-600">Total Services</p>
            </div>
            <p className="text-3xl font-bold text-gray-900">{stats?.totalServices || 0}</p>
          </div>

          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center mb-2">
              <AlertTriangle className="w-5 h-5 text-red-600 mr-2" />
              <p className="text-sm text-gray-600">Open Circuits</p>
            </div>
            <p className="text-3xl font-bold text-red-600">{stats?.openCircuits || 0}</p>
          </div>

          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center mb-2">
              <TrendingUp className="w-5 h-5 text-green-600 mr-2" />
              <p className="text-sm text-gray-600">Health Rate</p>
            </div>
            <p className="text-3xl font-bold text-green-600">
              {stats?.totalServices && stats.totalServices > 0
                ? Math.round(((stats.totalServices - (stats.openCircuits || 0)) / stats.totalServices) * 100)
                : 100}%
            </p>
          </div>
        </div>

        {/* Circuit Breakers List */}
        <div className="bg-white rounded-lg shadow-sm">
          <div className="px-6 py-4 border-b">
            <h2 className="text-xl font-semibold text-gray-900 flex items-center">
              <Shield className="w-5 h-5 mr-2 text-blue-600" />
              Service Circuit Breakers
            </h2>
          </div>

          {isLoading ? (
            <div className="p-8 text-center">
              <RefreshCw className="w-8 h-8 animate-spin text-gray-400 mx-auto mb-2" />
              <p className="text-gray-600">Loading circuit breakers...</p>
            </div>
          ) : !circuitBreakers || circuitBreakers.length === 0 ? (
            <div className="p-8 text-center">
              <Shield className="w-12 h-12 text-gray-300 mx-auto mb-3" />
              <p className="text-gray-600">No circuit breakers configured</p>
            </div>
          ) : (
            <div className="divide-y">
              {circuitBreakers.map((breaker) => (
                <div key={breaker.service_name} className="p-6 hover:bg-gray-50 transition-colors">
                  <div className="flex items-start justify-between mb-4">
                    <div className="flex items-start space-x-3">
                      {getStateIcon(breaker.current_state)}
                      <div>
                        <h3 className="text-lg font-semibold text-gray-900">{breaker.service_name}</h3>
                        <span
                          className={`inline-block px-3 py-1 rounded-full text-xs font-medium border mt-1 ${getStateColor(
                            breaker.current_state
                          )}`}
                        >
                          {breaker.current_state}
                        </span>
                      </div>
                    </div>
                    <div className="text-right">
                      <div className="text-2xl font-bold text-gray-900">{getHealthPercentage(breaker)}%</div>
                      <div className="text-sm text-gray-600">Health Score</div>
                    </div>
                  </div>

                  {/* Statistics Grid */}
                  <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-4">
                    <div className="bg-green-50 rounded-lg p-3">
                      <div className="text-sm text-green-600 font-medium">Success Count</div>
                      <div className="text-xl font-bold text-green-700">{breaker.success_count}</div>
                    </div>
                    <div className="bg-red-50 rounded-lg p-3">
                      <div className="text-sm text-red-600 font-medium">Failure Count</div>
                      <div className="text-xl font-bold text-red-700">{breaker.failure_count}</div>
                    </div>
                    <div className="bg-blue-50 rounded-lg p-3">
                      <div className="text-sm text-blue-600 font-medium">Failure Threshold</div>
                      <div className="text-xl font-bold text-blue-700">{breaker.failure_threshold}</div>
                    </div>
                    <div className="bg-purple-50 rounded-lg p-3">
                      <div className="text-sm text-purple-600 font-medium">Timeout (sec)</div>
                      <div className="text-xl font-bold text-purple-700">{breaker.timeout_duration}</div>
                    </div>
                  </div>

                  {/* Timestamps */}
                  <div className="grid grid-cols-1 md:grid-cols-2 gap-3 text-sm">
                    <div className="flex items-center text-gray-600">
                      <CheckCircle className="w-4 h-4 mr-2 text-green-600" />
                      <span>
                        Last Success:{' '}
                        {breaker.last_success_time
                          ? new Date(breaker.last_success_time).toLocaleString()
                          : 'Never'}
                      </span>
                    </div>
                    <div className="flex items-center text-gray-600">
                      <AlertTriangle className="w-4 h-4 mr-2 text-red-600" />
                      <span>
                        Last Failure:{' '}
                        {breaker.last_failure_time
                          ? new Date(breaker.last_failure_time).toLocaleString()
                          : 'Never'}
                      </span>
                    </div>
                  </div>

                  {/* Progress Bar */}
                  <div className="mt-4">
                    <div className="flex items-center justify-between text-xs text-gray-600 mb-1">
                      <span>Success Rate</span>
                      <span>{getHealthPercentage(breaker)}%</span>
                    </div>
                    <div className="w-full bg-gray-200 rounded-full h-2">
                      <div
                        className={`h-2 rounded-full transition-all ${
                          getHealthPercentage(breaker) >= 90
                            ? 'bg-green-600'
                            : getHealthPercentage(breaker) >= 70
                            ? 'bg-yellow-600'
                            : 'bg-red-600'
                        }`}
                        style={{ width: `${getHealthPercentage(breaker)}%` }}
                      ></div>
                    </div>
                  </div>

                  {/* Warning for Open Circuit */}
                  {breaker.current_state === 'OPEN' && (
                    <div className="mt-4 bg-red-50 border border-red-200 rounded-lg p-3">
                      <div className="flex items-start">
                        <AlertTriangle className="w-5 h-5 text-red-600 mr-2 flex-shrink-0 mt-0.5" />
                        <div className="text-sm text-red-700">
                          <strong>Circuit is OPEN:</strong> Service is currently unavailable due to repeated failures.
                          The circuit breaker will automatically attempt recovery in{' '}
                          <strong>{breaker.timeout_duration} seconds</strong>.
                        </div>
                      </div>
                    </div>
                  )}

                  {/* Info for Half-Open Circuit */}
                  {breaker.current_state === 'HALF_OPEN' && (
                    <div className="mt-4 bg-yellow-50 border border-yellow-200 rounded-lg p-3">
                      <div className="flex items-start">
                        <Clock className="w-5 h-5 text-yellow-600 mr-2 flex-shrink-0 mt-0.5" />
                        <div className="text-sm text-yellow-700">
                          <strong>Circuit is HALF-OPEN:</strong> Service is being tested for recovery. Allowing up to{' '}
                          <strong>{breaker.half_open_max_calls} test calls</strong> to determine if service has
                          recovered.
                        </div>
                      </div>
                    </div>
                  )}
                </div>
              ))}
            </div>
          )}
        </div>

        {/* Info Panel */}
        <div className="mt-6 bg-blue-50 border border-blue-200 rounded-lg p-6">
          <h3 className="text-lg font-semibold text-blue-900 mb-3 flex items-center">
            <Activity className="w-5 h-5 mr-2" />
            About Circuit Breakers
          </h3>
          <div className="space-y-2 text-sm text-blue-800">
            <p>
              <strong>CLOSED:</strong> Service is healthy and operating normally. All requests are being processed.
            </p>
            <p>
              <strong>OPEN:</strong> Service has failed multiple times. Circuit is open to prevent cascading failures.
              Requests are failing fast.
            </p>
            <p>
              <strong>HALF-OPEN:</strong> Testing recovery after timeout. Limited requests are allowed to check if the
              service has recovered.
            </p>
          </div>
        </div>
      </div>
    </div>
  );
};

export default CircuitBreakers;
