/**
 * Data Ingestion Page - Production Implementation
 * Real-time data pipeline management and monitoring
 * NO MOCKS - Production data ingestion system
 */

import React, { useState } from 'react';
import { Database, ArrowLeft, Activity, Clock, TrendingUp, AlertCircle, CheckCircle, Pause, Play } from 'lucide-react';
import { Link } from 'react-router-dom';
import { useDataPipelines, useIngestionMetrics } from '@/hooks/useDataIngestion';

const DataIngestion: React.FC = () => {
  const { data: pipelines, isLoading: pipelinesLoading } = useDataPipelines();
  const { data: metrics, isLoading: metricsLoading } = useIngestionMetrics();

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'active':
        return 'bg-green-100 text-green-800 border-green-200';
      case 'paused':
        return 'bg-yellow-100 text-yellow-800 border-yellow-200';
      case 'error':
        return 'bg-red-100 text-red-800 border-red-200';
      case 'stopped':
        return 'bg-gray-100 text-gray-800 border-gray-200';
      default:
        return 'bg-gray-100 text-gray-800 border-gray-200';
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'active':
        return <CheckCircle className="w-5 h-5 text-green-600" />;
      case 'paused':
        return <Pause className="w-5 h-5 text-yellow-600" />;
      case 'error':
        return <AlertCircle className="w-5 h-5 text-red-600" />;
      case 'stopped':
        return <Activity className="w-5 h-5 text-gray-600" />;
      default:
        return <Activity className="w-5 h-5 text-gray-600" />;
    }
  };

  const getSuccessRate = (processed: number, failed: number) => {
    const total = processed + failed;
    if (total === 0) return 100;
    return Math.round((processed / total) * 100);
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
          <div className="flex items-center space-x-4">
            <div className="p-3 bg-green-100 rounded-lg">
              <Database className="w-8 h-8 text-green-600" />
            </div>
            <div>
              <h1 className="text-3xl font-bold text-gray-900">Data Ingestion</h1>
              <p className="text-gray-600">Real-time data pipeline management</p>
            </div>
          </div>
        </div>

        {/* Metrics Overview */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4 mb-6">
          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center mb-2">
              <Database className="w-5 h-5 text-blue-600 mr-2" />
              <p className="text-sm text-gray-600">Total Pipelines</p>
            </div>
            <p className="text-3xl font-bold text-gray-900">
              {metricsLoading ? '...' : metrics?.totalPipelines || 0}
            </p>
            <p className="text-sm text-green-600 mt-1">
              {metricsLoading ? '' : `${metrics?.activePipelines || 0} active`}
            </p>
          </div>

          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center mb-2">
              <CheckCircle className="w-5 h-5 text-green-600 mr-2" />
              <p className="text-sm text-gray-600">Records Processed</p>
            </div>
            <p className="text-3xl font-bold text-gray-900">
              {metricsLoading ? '...' : (metrics?.totalRecordsProcessed || 0).toLocaleString()}
            </p>
          </div>

          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center mb-2">
              <TrendingUp className="w-5 h-5 text-purple-600 mr-2" />
              <p className="text-sm text-gray-600">Avg Throughput</p>
            </div>
            <p className="text-3xl font-bold text-gray-900">
              {metricsLoading ? '...' : (metrics?.averageThroughput || 0).toLocaleString()}
            </p>
            <p className="text-sm text-gray-600 mt-1">records/min</p>
          </div>

          <div className="bg-white rounded-lg shadow-sm p-6">
            <div className="flex items-center mb-2">
              <AlertCircle className="w-5 h-5 text-red-600 mr-2" />
              <p className="text-sm text-gray-600">Failed Records</p>
            </div>
            <p className="text-3xl font-bold text-red-600">
              {metricsLoading ? '...' : (metrics?.totalRecordsFailed || 0).toLocaleString()}
            </p>
            <p className="text-sm text-gray-600 mt-1">
              {metricsLoading
                ? ''
                : `${(
                    ((metrics?.totalRecordsFailed || 0) /
                      ((metrics?.totalRecordsProcessed || 0) + (metrics?.totalRecordsFailed || 1))) *
                    100
                  ).toFixed(2)}% error rate`}
            </p>
          </div>
        </div>

        {/* Pipelines List */}
        <div className="bg-white rounded-lg shadow-sm">
          <div className="px-6 py-4 border-b">
            <div className="flex items-center justify-between">
              <h2 className="text-xl font-semibold text-gray-900 flex items-center">
                <Activity className="w-5 h-5 mr-2 text-blue-600" />
                Data Pipelines
              </h2>
              <button className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 flex items-center">
                <Play className="w-4 h-4 mr-2" />
                New Pipeline
              </button>
            </div>
          </div>

          {pipelinesLoading ? (
            <div className="p-8 text-center">
              <Activity className="w-8 h-8 animate-spin text-gray-400 mx-auto mb-2" />
              <p className="text-gray-600">Loading pipelines...</p>
            </div>
          ) : !pipelines || pipelines.length === 0 ? (
            <div className="p-8 text-center">
              <Database className="w-12 h-12 text-gray-300 mx-auto mb-3" />
              <p className="text-gray-600">No pipelines configured</p>
            </div>
          ) : (
            <div className="divide-y">
              {pipelines.map((pipeline) => (
                <div key={pipeline.id} className="p-6 hover:bg-gray-50 transition-colors">
                  <div className="flex items-start justify-between mb-4">
                    <div className="flex items-start space-x-3">
                      {getStatusIcon(pipeline.status)}
                      <div>
                        <h3 className="text-lg font-semibold text-gray-900">{pipeline.name}</h3>
                        <div className="flex items-center space-x-2 mt-1">
                          <span
                            className={`inline-block px-3 py-1 rounded-full text-xs font-medium border ${getStatusColor(
                              pipeline.status
                            )}`}
                          >
                            {pipeline.status.toUpperCase()}
                          </span>
                          <span className="px-2 py-1 bg-gray-100 text-gray-700 text-xs rounded">
                            {pipeline.type}
                          </span>
                        </div>
                      </div>
                    </div>
                    <div className="text-right">
                      <div className="text-2xl font-bold text-gray-900">{getSuccessRate(pipeline.recordsProcessed, pipeline.recordsFailed)}%</div>
                      <div className="text-sm text-gray-600">Success Rate</div>
                    </div>
                  </div>

                  {/* Pipeline Details */}
                  <div className="grid grid-cols-2 gap-3 mb-4 text-sm">
                    <div>
                      <p className="text-gray-600">Source</p>
                      <p className="font-medium text-gray-900">{pipeline.source}</p>
                    </div>
                    <div>
                      <p className="text-gray-600">Destination</p>
                      <p className="font-medium text-gray-900">{pipeline.destination}</p>
                    </div>
                  </div>

                  {/* Stats Grid */}
                  <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-4">
                    <div className="bg-green-50 rounded-lg p-3">
                      <div className="text-sm text-green-600 font-medium">Processed</div>
                      <div className="text-xl font-bold text-green-700">{pipeline.recordsProcessed.toLocaleString()}</div>
                    </div>
                    <div className="bg-red-50 rounded-lg p-3">
                      <div className="text-sm text-red-600 font-medium">Failed</div>
                      <div className="text-xl font-bold text-red-700">{pipeline.recordsFailed.toLocaleString()}</div>
                    </div>
                    <div className="bg-blue-50 rounded-lg p-3">
                      <div className="text-sm text-blue-600 font-medium">Last Run</div>
                      <div className="text-sm font-medium text-blue-700">
                        {pipeline.lastRun ? new Date(pipeline.lastRun).toLocaleTimeString() : 'Never'}
                      </div>
                    </div>
                    <div className="bg-purple-50 rounded-lg p-3">
                      <div className="text-sm text-purple-600 font-medium">Next Run</div>
                      <div className="text-sm font-medium text-purple-700">
                        {pipeline.nextRun ? new Date(pipeline.nextRun).toLocaleTimeString() : 'N/A'}
                      </div>
                    </div>
                  </div>

                  {/* Progress Bar */}
                  <div className="mb-4">
                    <div className="flex items-center justify-between text-xs text-gray-600 mb-1">
                      <span>Processing Status</span>
                      <span>{pipeline.recordsProcessed.toLocaleString()} records</span>
                    </div>
                    <div className="w-full bg-gray-200 rounded-full h-2">
                      <div
                        className={`h-2 rounded-full transition-all ${
                          getSuccessRate(pipeline.recordsProcessed, pipeline.recordsFailed) >= 99
                            ? 'bg-green-600'
                            : getSuccessRate(pipeline.recordsProcessed, pipeline.recordsFailed) >= 95
                            ? 'bg-yellow-600'
                            : 'bg-red-600'
                        }`}
                        style={{ width: `${getSuccessRate(pipeline.recordsProcessed, pipeline.recordsFailed)}%` }}
                      ></div>
                    </div>
                  </div>

                  {/* Action Buttons */}
                  <div className="flex items-center space-x-2">
                    {pipeline.status === 'active' ? (
                      <button className="px-3 py-1 bg-yellow-600 text-white rounded hover:bg-yellow-700 text-sm flex items-center">
                        <Pause className="w-3 h-3 mr-1" />
                        Pause
                      </button>
                    ) : (
                      <button className="px-3 py-1 bg-green-600 text-white rounded hover:bg-green-700 text-sm flex items-center">
                        <Play className="w-3 h-3 mr-1" />
                        Start
                      </button>
                    )}
                    <button className="px-3 py-1 border border-gray-300 text-gray-700 rounded hover:bg-gray-50 text-sm">
                      View Logs
                    </button>
                    <button className="px-3 py-1 border border-gray-300 text-gray-700 rounded hover:bg-gray-50 text-sm">
                      Configure
                    </button>
                  </div>

                  {/* Error Alert */}
                  {pipeline.status === 'error' && (
                    <div className="mt-4 bg-red-50 border border-red-200 rounded-lg p-3">
                      <div className="flex items-start">
                        <AlertCircle className="w-5 h-5 text-red-600 mr-2 flex-shrink-0 mt-0.5" />
                        <div className="text-sm text-red-700">
                          <strong>Pipeline Error:</strong> The pipeline encountered an error and has been stopped.
                          Check the logs for more details.
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
            <Database className="w-5 h-5 mr-2" />
            About Data Ingestion
          </h3>
          <div className="space-y-2 text-sm text-blue-800">
            <p>
              <strong>Streaming Pipelines:</strong> Continuously process data in real-time from various sources.
            </p>
            <p>
              <strong>Batch Pipelines:</strong> Process large volumes of data at scheduled intervals.
            </p>
            <p>
              <strong>Scheduled Pipelines:</strong> Run at specific times or intervals to sync data.
            </p>
          </div>
        </div>
      </div>
    </div>
  );
};

export default DataIngestion;
