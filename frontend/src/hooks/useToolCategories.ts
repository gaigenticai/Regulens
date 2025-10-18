import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import apiClient from '../services/api';

// Types for Tool Categories API
export interface Tool {
  name: string;
  description: string;
  category: string;
  operations: string[];
  version: string;
  status: 'active' | 'inactive' | 'maintenance';
}

export interface ToolCategory {
  id: string;
  name: string;
  description: string;
  tools: Tool[];
  status: 'active' | 'inactive';
}

export interface ToolExecutionRequest {
  tool_name: string;
  operation: string;
  parameters?: Record<string, any>;
  test_mode?: boolean;
  timeout_ms?: number;
}

export interface ToolExecutionResult {
  success: boolean;
  execution_time_ms: number;
  result_data?: any;
  error_message?: string;
  tool_name: string;
  operation: string;
  timestamp: string;
}

export interface TestSuite {
  id: string;
  name: string;
  description: string;
  category: string;
  tools: TestCase[];
  created_at: string;
  updated_at: string;
  status: 'active' | 'inactive';
}

export interface TestCase {
  id: string;
  tool_name: string;
  operation: string;
  parameters: Record<string, any>;
  expected_result?: any;
  timeout_ms?: number;
}

export interface TestExecution {
  id: string;
  suite_id: string;
  suite_name: string;
  status: 'pending' | 'running' | 'completed' | 'failed' | 'cancelled';
  start_time: string;
  end_time?: string;
  progress: number;
  total_tests: number;
  completed_tests: number;
  failed_tests: number;
  results: TestResult[];
}

export interface TestResult {
  id: string;
  execution_id: string;
  test_id: string;
  tool_name: string;
  operation: string;
  status: 'passed' | 'failed' | 'skipped' | 'error';
  start_time: string;
  end_time?: string;
  execution_time_ms?: number;
  error_message?: string;
  result_data?: any;
}

// Tool Registry Hooks
export const useAvailableTools = () => {
  return useQuery({
    queryKey: ['tools', 'available'],
    queryFn: async () => {
      const response = await apiClient.get('/tools/available');
      return response.data;
    },
  });
};

export const useToolsByCategory = (category: string) => {
  return useQuery({
    queryKey: ['tools', 'category', category],
    queryFn: async () => {
      const response = await apiClient.get(`/tools/categories/${category}`);
      return response.data;
    },
    enabled: !!category,
  });
};

export const useToolInfo = (toolName: string) => {
  return useQuery({
    queryKey: ['tools', 'info', toolName],
    queryFn: async () => {
      const response = await apiClient.get(`/tools/${toolName}/info`);
      return response.data;
    },
    enabled: !!toolName,
  });
};

// Tool Execution Hooks
export const useExecuteTool = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async ({
      category,
      request,
    }: {
      category: string;
      request: ToolExecutionRequest;
    }) => {
      const response = await apiClient.post(`/tools/categories/${category}/execute`, request);
      return response.data;
    },
    onSuccess: () => {
      // Invalidate related queries
      queryClient.invalidateQueries({ queryKey: ['tools'] });
    },
  });
};

// Test Suite Management Hooks
export const useTestSuites = (category?: string) => {
  return useQuery({
    queryKey: ['test-suites', category],
    queryFn: async () => {
      // This would be a real API call in production
      // Fetch test suites from backend API
      const authToken = localStorage.getItem('authToken');
      const response = await fetch('/api/tool-categories/test-suites', {
        headers: { 'Authorization': `Bearer ${authToken}` }
      });
      
      const mockSuites: TestSuite[] = response.ok ? (await response.json()) : [
        {
          id: 'suite_1',
          name: 'Analytics Test Suite',
          description: 'Comprehensive testing of analytics tools',
          category: 'analytics',
          tools: [],
          created_at: new Date().toISOString(),
          updated_at: new Date().toISOString(),
          status: 'active',
        },
        {
          id: 'suite_2',
          name: 'Security Test Suite',
          description: 'Security tools validation suite',
          category: 'security',
          tools: [],
          created_at: new Date().toISOString(),
          updated_at: new Date().toISOString(),
          status: 'active',
        },
      ];
      return mockSuites.filter(suite => !category || suite.category === category);
    },
  });
};

export const useTestSuite = (suiteId: string) => {
  return useQuery({
    queryKey: ['test-suite', suiteId],
    queryFn: async () => {
      // Mock implementation - would call real API
      const mockSuite: TestSuite = {
        id: suiteId,
        name: 'Mock Test Suite',
        description: 'Mock test suite for demonstration',
        category: 'analytics',
        tools: [],
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        status: 'active',
      };
      return mockSuite;
    },
    enabled: !!suiteId,
  });
};

export const useCreateTestSuite = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (suite: Omit<TestSuite, 'id' | 'created_at' | 'updated_at'>) => {
      // Mock implementation - would call real API
      const newSuite: TestSuite = {
        ...suite,
        id: `suite_${Date.now()}`,
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
      };
      return newSuite;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['test-suites'] });
    },
  });
};

export const useUpdateTestSuite = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async ({
      id,
      updates,
    }: {
      id: string;
      updates: Partial<TestSuite>;
    }) => {
      // Mock implementation - would call real API
      const updatedSuite: TestSuite = {
        id,
        name: 'Updated Suite',
        description: 'Updated description',
        category: 'analytics',
        tools: [],
        created_at: new Date().toISOString(),
        updated_at: new Date().toISOString(),
        status: 'active',
        ...updates,
      };
      return updatedSuite;
    },
    onSuccess: (_, { id }) => {
      queryClient.invalidateQueries({ queryKey: ['test-suites'] });
      queryClient.invalidateQueries({ queryKey: ['test-suite', id] });
    },
  });
};

export const useDeleteTestSuite = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (id: string) => {
      // Mock implementation - would call real API
      return { success: true };
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['test-suites'] });
    },
  });
};

// Test Execution Hooks
export const useRunTestSuite = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (suiteId: string) => {
      // Mock implementation - would call real API
      const execution: TestExecution = {
        id: `exec_${Date.now()}`,
        suite_id: suiteId,
        suite_name: 'Mock Suite',
        status: 'running',
        start_time: new Date().toISOString(),
        progress: 0,
        total_tests: 5,
        completed_tests: 0,
        failed_tests: 0,
        results: [],
      };
      return execution;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['test-executions'] });
    },
  });
};

export const useTestExecution = (executionId: string) => {
  return useQuery({
    queryKey: ['test-execution', executionId],
    queryFn: async () => {
      // Mock implementation - would call real API with polling
      const execution: TestExecution = {
        id: executionId,
        suite_id: 'suite_1',
        suite_name: 'Mock Test Suite',
        status: 'completed',
        start_time: new Date(Date.now() - 60000).toISOString(),
        end_time: new Date().toISOString(),
        progress: 100,
        total_tests: 5,
        completed_tests: 4,
        failed_tests: 1,
        results: [],
      };
      return execution;
    },
    enabled: !!executionId,
    refetchInterval: (data) => {
      // Refetch every 5 seconds if execution is still running
      if (data?.status === 'pending' || data?.status === 'running') {
        return 5000;
      }
      return false;
    },
  });
};

export const useCancelTestExecution = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (executionId: string) => {
      // Mock implementation - would call real API
      return { success: true };
    },
    onSuccess: (_, executionId) => {
      queryClient.invalidateQueries({ queryKey: ['test-execution', executionId] });
      queryClient.invalidateQueries({ queryKey: ['test-executions'] });
    },
  });
};

// Test Results Hooks
export const useTestResults = (params?: {
  suite_id?: string;
  execution_id?: string;
  status?: string;
  limit?: number;
  offset?: number;
}) => {
  return useQuery({
    queryKey: ['test-results', params],
    queryFn: async () => {
      // Mock implementation - would call real API
      const mockResults: TestResult[] = [
        {
          id: 'result_1',
          execution_id: params?.execution_id || 'exec_1',
          test_id: 'test_1',
          tool_name: 'data_analyzer',
          operation: 'analyze_dataset',
          status: 'passed',
          start_time: new Date().toISOString(),
          end_time: new Date().toISOString(),
          execution_time_ms: 1500,
          result_data: { processed: 1000 },
        },
        {
          id: 'result_2',
          execution_id: params?.execution_id || 'exec_1',
          test_id: 'test_2',
          tool_name: 'report_generator',
          operation: 'generate_report',
          status: 'failed',
          start_time: new Date().toISOString(),
          end_time: new Date().toISOString(),
          execution_time_ms: 2000,
          error_message: 'Report generation failed',
        },
      ];
      return mockResults;
    },
  });
};

// Test Reporting Hooks
export const useTestReports = (params?: {
  date_from?: string;
  date_to?: string;
  category?: string;
  suite_id?: string;
}) => {
  return useQuery({
    queryKey: ['test-reports', params],
    queryFn: async () => {
      // Mock implementation - would call real API
      const mockReports = [
        {
          id: 'report_1',
          title: 'Monthly Test Report',
          date_range: {
            start: params?.date_from || '2024-01-01',
            end: params?.date_to || '2024-01-31',
          },
          summary: {
            total_tests: 1250,
            passed_tests: 1100,
            failed_tests: 120,
            error_tests: 20,
            skipped_tests: 10,
            avg_execution_time: 2500,
            total_execution_time: 3125000,
          },
          category_breakdown: {},
          trends: { daily: [], weekly: [] },
          top_failures: [],
        },
      ];
      return mockReports;
    },
  });
};

export const useGenerateTestReport = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (params: {
      date_from: string;
      date_to: string;
      category?: string;
      suite_id?: string;
    }) => {
      // Mock implementation - would call real API
      const report = {
        id: `report_${Date.now()}`,
        title: `Test Report ${params.date_from} to ${params.date_to}`,
        date_range: {
          start: params.date_from,
          end: params.date_to,
        },
        summary: {
          total_tests: Math.floor(Math.random() * 1000) + 500,
          passed_tests: 0,
          failed_tests: 0,
          error_tests: 0,
          skipped_tests: 0,
          avg_execution_time: Math.floor(Math.random() * 2000) + 1000,
          total_execution_time: 0,
        },
      };

      // Calculate derived values
      const totalPassed = Math.floor(report.summary.total_tests * 0.8);
      const totalFailed = Math.floor(report.summary.total_tests * 0.15);
      const totalError = Math.floor(report.summary.total_tests * 0.03);
      const totalSkipped = report.summary.total_tests - totalPassed - totalFailed - totalError;

      report.summary.passed_tests = totalPassed;
      report.summary.failed_tests = totalFailed;
      report.summary.error_tests = totalError;
      report.summary.skipped_tests = totalSkipped;
      report.summary.total_execution_time = report.summary.total_tests * report.summary.avg_execution_time;

      return report;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['test-reports'] });
    },
  });
};
