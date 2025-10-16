import React, { useState, useMemo } from 'react';
import { BarChart3, PieChart, TrendingUp, Download, Calendar, Filter, FileText, AlertTriangle, CheckCircle, XCircle } from 'lucide-react';

interface TestReport {
  id: string;
  title: string;
  date_range: {
    start: string;
    end: string;
  };
  summary: {
    total_tests: number;
    passed_tests: number;
    failed_tests: number;
    error_tests: number;
    skipped_tests: number;
    avg_execution_time: number;
    total_execution_time: number;
  };
  category_breakdown: {
    [category: string]: {
      total: number;
      passed: number;
      failed: number;
      error: number;
      skipped: number;
    };
  };
  trends: {
    daily: Array<{
      date: string;
      total: number;
      passed: number;
      failed: number;
    }>;
    weekly: Array<{
      week: string;
      total: number;
      passed: number;
      failed: number;
    }>;
  };
  top_failures: Array<{
    tool_name: string;
    operation: string;
    failure_count: number;
    last_failure: string;
    error_message: string;
  }>;
}

interface TestReportingProps {
  reports?: TestReport[];
  onGenerateReport?: (dateRange: { start: string; end: string }) => void;
}

export const TestReporting: React.FC<TestReportingProps> = ({
  reports = [],
  onGenerateReport
}) => {
  const [selectedReport, setSelectedReport] = useState<TestReport | null>(null);
  const [dateRange, setDateRange] = useState({
    start: new Date(Date.now() - 30 * 24 * 60 * 60 * 1000).toISOString().split('T')[0],
    end: new Date().toISOString().split('T')[0]
  });
  const [reportType, setReportType] = useState<'summary' | 'detailed' | 'trends'>('summary');

  // Mock data for demonstration
  const mockReports: TestReport[] = [
    {
      id: 'report_1',
      title: 'Monthly Test Report - January 2024',
      date_range: {
        start: '2024-01-01',
        end: '2024-01-31'
      },
      summary: {
        total_tests: 1250,
        passed_tests: 1100,
        failed_tests: 120,
        error_tests: 20,
        skipped_tests: 10,
        avg_execution_time: 2500,
        total_execution_time: 3125000
      },
      category_breakdown: {
        analytics: { total: 400, passed: 380, failed: 15, error: 3, skipped: 2 },
        workflow: { total: 300, passed: 270, failed: 25, error: 3, skipped: 2 },
        security: { total: 350, passed: 320, failed: 20, error: 8, skipped: 2 },
        monitoring: { total: 200, passed: 130, failed: 60, error: 6, skipped: 4 }
      },
      trends: {
        daily: [
          { date: '2024-01-15', total: 45, passed: 42, failed: 3 },
          { date: '2024-01-16', total: 52, passed: 48, failed: 4 },
          { date: '2024-01-17', total: 38, passed: 35, failed: 3 }
        ],
        weekly: [
          { week: 'Week 1', total: 280, passed: 260, failed: 20 },
          { week: 'Week 2', total: 310, passed: 285, failed: 25 },
          { week: 'Week 3', total: 295, passed: 270, failed: 25 },
          { week: 'Week 4', total: 365, passed: 285, failed: 80 }
        ]
      },
      top_failures: [
        {
          tool_name: 'data_analyzer',
          operation: 'analyze_dataset',
          failure_count: 15,
          last_failure: '2024-01-30T15:30:00Z',
          error_message: 'Dataset validation failed'
        },
        {
          tool_name: 'task_automator',
          operation: 'automate_task',
          failure_count: 12,
          last_failure: '2024-01-29T12:15:00Z',
          error_message: 'External service timeout'
        },
        {
          tool_name: 'vulnerability_scanner',
          operation: 'scan_vulnerabilities',
          failure_count: 8,
          last_failure: '2024-01-28T09:45:00Z',
          error_message: 'Scan configuration error'
        }
      ]
    }
  ];

  const displayReports = reports.length > 0 ? reports : mockReports;
  const currentReport = selectedReport || displayReports[0];

  const generateReport = () => {
    if (onGenerateReport) {
      onGenerateReport(dateRange);
    } else {
      // Mock report generation
      const newReport: TestReport = {
        id: `report_${Date.now()}`,
        title: `Test Report - ${new Date(dateRange.start).toLocaleDateString()} to ${new Date(dateRange.end).toLocaleDateString()}`,
        date_range: dateRange,
        summary: {
          total_tests: Math.floor(Math.random() * 500) + 500,
          passed_tests: 0,
          failed_tests: 0,
          error_tests: 0,
          skipped_tests: 0,
          avg_execution_time: Math.floor(Math.random() * 2000) + 1000,
          total_execution_time: 0
        },
        category_breakdown: {
          analytics: { total: 200, passed: 180, failed: 15, error: 3, skipped: 2 },
          workflow: { total: 150, passed: 135, failed: 12, error: 2, skipped: 1 },
          security: { total: 100, passed: 85, failed: 10, error: 4, skipped: 1 },
          monitoring: { total: 50, passed: 35, failed: 12, error: 2, skipped: 1 }
        },
        trends: {
          daily: [],
          weekly: []
        },
        top_failures: []
      };

      // Calculate totals
      const totalPassed = Object.values(newReport.category_breakdown).reduce((sum, cat) => sum + cat.passed, 0);
      const totalFailed = Object.values(newReport.category_breakdown).reduce((sum, cat) => sum + cat.failed, 0);
      const totalError = Object.values(newReport.category_breakdown).reduce((sum, cat) => sum + cat.error, 0);
      const totalSkipped = Object.values(newReport.category_breakdown).reduce((sum, cat) => sum + cat.skipped, 0);

      newReport.summary.passed_tests = totalPassed;
      newReport.summary.failed_tests = totalFailed;
      newReport.summary.error_tests = totalError;
      newReport.summary.skipped_tests = totalSkipped;
      newReport.summary.total_execution_time = newReport.summary.total_tests * newReport.summary.avg_execution_time;

      setSelectedReport(newReport);
    }
  };

  const exportReport = (format: 'pdf' | 'csv' | 'json') => {
    if (!currentReport) return;

    let content = '';
    let filename = '';
    let mimeType = '';

    switch (format) {
      case 'json':
        content = JSON.stringify(currentReport, null, 2);
        filename = `${currentReport.title.replace(/[^a-zA-Z0-9]/g, '_')}.json`;
        mimeType = 'application/json';
        break;
      case 'csv':
        content = [
          ['Metric', 'Value'],
          ['Total Tests', currentReport.summary.total_tests.toString()],
          ['Passed Tests', currentReport.summary.passed_tests.toString()],
          ['Failed Tests', currentReport.summary.failed_tests.toString()],
          ['Error Tests', currentReport.summary.error_tests.toString()],
          ['Skipped Tests', currentReport.summary.skipped_tests.toString()],
          ['Average Execution Time', `${currentReport.summary.avg_execution_time}ms`],
          ['Total Execution Time', `${currentReport.summary.total_execution_time}ms`]
        ].map(row => row.join(',')).join('\n');
        filename = `${currentReport.title.replace(/[^a-zA-Z0-9]/g, '_')}.csv`;
        mimeType = 'text/csv';
        break;
      default:
        return;
    }

    const blob = new Blob([content], { type: mimeType });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = filename;
    a.click();
    window.URL.revokeObjectURL(url);
  };

  const successRate = currentReport ? (currentReport.summary.passed_tests / currentReport.summary.total_tests) * 100 : 0;
  const failureRate = currentReport ? (currentReport.summary.failed_tests / currentReport.summary.total_tests) * 100 : 0;

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex justify-between items-center">
        <div>
          <h3 className="text-lg font-semibold text-gray-900">Test Reporting</h3>
          <p className="text-gray-600 mt-1">
            Comprehensive test execution reports and analytics
          </p>
        </div>
        <div className="flex gap-2">
          <button
            onClick={() => exportReport('pdf')}
            className="flex items-center gap-2 bg-red-600 text-white px-4 py-2 rounded-lg hover:bg-red-700 transition-colors"
          >
            <Download className="w-4 h-4" />
            PDF
          </button>
          <button
            onClick={() => exportReport('csv')}
            className="flex items-center gap-2 bg-green-600 text-white px-4 py-2 rounded-lg hover:bg-green-700 transition-colors"
          >
            <Download className="w-4 h-4" />
            CSV
          </button>
          <button
            onClick={() => exportReport('json')}
            className="flex items-center gap-2 bg-blue-600 text-white px-4 py-2 rounded-lg hover:bg-blue-700 transition-colors"
          >
            <Download className="w-4 h-4" />
            JSON
          </button>
        </div>
      </div>

      {/* Report Generation */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <h4 className="text-md font-semibold text-gray-900 mb-4">Generate New Report</h4>
        <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">Start Date</label>
            <input
              type="date"
              value={dateRange.start}
              onChange={(e) => setDateRange(prev => ({ ...prev, start: e.target.value }))}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">End Date</label>
            <input
              type="date"
              value={dateRange.end}
              onChange={(e) => setDateRange(prev => ({ ...prev, end: e.target.value }))}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            />
          </div>
          <div>
            <label className="block text-sm font-medium text-gray-700 mb-1">Report Type</label>
            <select
              value={reportType}
              onChange={(e) => setReportType(e.target.value as any)}
              className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
            >
              <option value="summary">Summary Report</option>
              <option value="detailed">Detailed Report</option>
              <option value="trends">Trends Analysis</option>
            </select>
          </div>
          <div className="flex items-end">
            <button
              onClick={generateReport}
              className="w-full bg-blue-600 text-white px-4 py-2 rounded-lg hover:bg-blue-700 transition-colors"
            >
              Generate Report
            </button>
          </div>
        </div>
      </div>

      {/* Report Selector */}
      <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
        <h4 className="text-md font-semibold text-gray-900 mb-4">Available Reports</h4>
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
          {displayReports.map((report) => (
            <div
              key={report.id}
              onClick={() => setSelectedReport(report)}
              className={`p-4 border rounded-lg cursor-pointer transition-colors ${
                selectedReport?.id === report.id
                  ? 'border-blue-500 bg-blue-50'
                  : 'border-gray-200 hover:border-gray-300'
              }`}
            >
              <div className="flex items-start justify-between">
                <div className="flex-1">
                  <h5 className="font-medium text-gray-900">{report.title}</h5>
                  <p className="text-sm text-gray-600 mt-1">
                    {new Date(report.date_range.start).toLocaleDateString()} - {new Date(report.date_range.end).toLocaleDateString()}
                  </p>
                  <div className="flex gap-4 mt-2 text-sm">
                    <span className="text-green-600">{report.summary.passed_tests} passed</span>
                    <span className="text-red-600">{report.summary.failed_tests} failed</span>
                    <span className="text-gray-600">{report.summary.total_tests} total</span>
                  </div>
                </div>
                <FileText className="w-5 h-5 text-gray-400" />
              </div>
            </div>
          ))}
        </div>
      </div>

      {/* Current Report Display */}
      {currentReport && (
        <>
          {/* Summary Cards */}
          <div className="grid grid-cols-2 md:grid-cols-4 gap-4">
            <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
              <div className="flex items-center gap-2">
                <BarChart3 className="w-5 h-5 text-blue-500" />
                <span className="text-sm font-medium text-gray-700">Total Tests</span>
              </div>
              <div className="text-2xl font-bold text-gray-900 mt-1">{currentReport.summary.total_tests}</div>
            </div>
            <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
              <div className="flex items-center gap-2">
                <CheckCircle className="w-5 h-5 text-green-500" />
                <span className="text-sm font-medium text-gray-700">Success Rate</span>
              </div>
              <div className="text-2xl font-bold text-green-600 mt-1">{successRate.toFixed(1)}%</div>
            </div>
            <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
              <div className="flex items-center gap-2">
                <XCircle className="w-5 h-5 text-red-500" />
                <span className="text-sm font-medium text-gray-700">Failure Rate</span>
              </div>
              <div className="text-2xl font-bold text-red-600 mt-1">{failureRate.toFixed(1)}%</div>
            </div>
            <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-4">
              <div className="flex items-center gap-2">
                <TrendingUp className="w-5 h-5 text-purple-500" />
                <span className="text-sm font-medium text-gray-700">Avg Time</span>
              </div>
              <div className="text-lg font-bold text-purple-600 mt-1">
                {Math.round(currentReport.summary.avg_execution_time)}ms
              </div>
            </div>
          </div>

          {/* Category Breakdown */}
          <div className="bg-white rounded-lg shadow-sm border border-gray-200">
            <div className="p-6 border-b border-gray-200">
              <h4 className="text-lg font-semibold text-gray-900">Category Breakdown</h4>
            </div>
            <div className="p-6">
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
                {Object.entries(currentReport.category_breakdown).map(([category, stats]) => {
                  const categorySuccessRate = (stats.passed / stats.total) * 100;
                  return (
                    <div key={category} className="text-center">
                      <h5 className="font-medium text-gray-900 capitalize mb-2">{category}</h5>
                      <div className="space-y-2">
                        <div className="text-2xl font-bold text-blue-600">{stats.total}</div>
                        <div className="text-sm text-gray-600">Total Tests</div>
                        <div className="text-lg font-semibold text-green-600">{categorySuccessRate.toFixed(1)}%</div>
                        <div className="text-sm text-gray-600">Success Rate</div>
                        <div className="flex justify-center gap-2 text-xs">
                          <span className="text-green-600">{stats.passed} passed</span>
                          <span className="text-red-600">{stats.failed} failed</span>
                          {stats.error > 0 && <span className="text-orange-600">{stats.error} errors</span>}
                        </div>
                      </div>
                    </div>
                  );
                })}
              </div>
            </div>
          </div>

          {/* Top Failures */}
          <div className="bg-white rounded-lg shadow-sm border border-gray-200">
            <div className="p-6 border-b border-gray-200">
              <h4 className="text-lg font-semibold text-gray-900">Top Failures</h4>
            </div>
            <div className="divide-y divide-gray-200">
              {currentReport.top_failures.map((failure, index) => (
                <div key={index} className="p-4">
                  <div className="flex justify-between items-start">
                    <div className="flex-1">
                      <div className="flex items-center gap-2 mb-2">
                        <span className="font-medium text-gray-900">
                          {failure.tool_name} → {failure.operation}
                        </span>
                        <span className="inline-flex items-center px-2 py-1 rounded-full text-xs font-medium bg-red-100 text-red-800">
                          {failure.failure_count} failures
                        </span>
                      </div>
                      <p className="text-sm text-red-600 mb-1">{failure.error_message}</p>
                      <p className="text-xs text-gray-500">
                        Last failure: {new Date(failure.last_failure).toLocaleString()}
                      </p>
                    </div>
                    <AlertTriangle className="w-5 h-5 text-red-500" />
                  </div>
                </div>
              ))}
            </div>
          </div>

          {/* Trends Chart Placeholder */}
          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
            <h4 className="text-lg font-semibold text-gray-900 mb-4">Execution Trends</h4>
            <div className="h-64 flex items-center justify-center bg-gray-50 rounded-lg">
              <div className="text-center">
                <BarChart3 className="w-12 h-12 text-gray-400 mx-auto mb-2" />
                <p className="text-gray-600">Trends chart would be displayed here</p>
                <p className="text-sm text-gray-500 mt-1">
                  Integration with charting library needed for visualization
                </p>
              </div>
            </div>
          </div>
        </>
      )}
    </div>
  );
};
