import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import apiClient from '../services/api';

// Types for Simulator API
export interface Scenario {
  id: string;
  name: string;
  description: string;
  user_id: string;
  template_id?: string;
  parameters: Record<string, any>;
  status: 'draft' | 'active' | 'archived';
  created_at: string;
  updated_at: string;
  execution_count: number;
  last_execution?: string;
}

export interface SimulationTemplate {
  id: string;
  name: string;
  description: string;
  category: string;
  parameters: Record<string, any>;
  default_config: Record<string, any>;
  is_public: boolean;
  usage_count: number;
  created_at: string;
  updated_at: string;
}

export interface SimulationExecution {
  id: string;
  scenario_id: string;
  user_id: string;
  status: 'pending' | 'running' | 'completed' | 'failed' | 'cancelled';
  start_time: string;
  end_time?: string;
  progress: number;
  parameters: Record<string, any>;
  results?: Record<string, any>;
  error_message?: string;
}

export interface SimulationResult {
  execution_id: string;
  scenario_id: string;
  results: Record<string, any>;
  metrics: Record<string, any>;
  impact_analysis: Record<string, any>;
  timestamp: string;
  duration_ms: number;
}

export interface SimulationAnalytics {
  total_scenarios: number;
  total_executions: number;
  success_rate: number;
  average_duration: number;
  popular_scenarios: Array<{
    scenario_id: string;
    name: string;
    execution_count: number;
  }>;
  execution_trends: Array<{
    date: string;
    count: number;
  }>;
}

// Scenario Management Hooks
export const useScenarios = (params?: {
  page?: number;
  limit?: number;
  status?: string;
  search?: string;
}) => {
  return useQuery({
    queryKey: ['scenarios', params],
    queryFn: async () => {
      const response = await apiClient.get('/simulator/scenarios', { params });
      return response.data;
    },
  });
};

export const useScenario = (id: string) => {
  return useQuery({
    queryKey: ['scenario', id],
    queryFn: async () => {
      const response = await apiClient.get(`/simulator/scenarios/${id}`);
      return response.data;
    },
    enabled: !!id,
  });
};

export const useCreateScenario = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (data: {
      name: string;
      description: string;
      template_id?: string;
      parameters: Record<string, any>;
    }) => {
      const response = await apiClient.post('/simulator/scenarios', data);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['scenarios'] });
    },
  });
};

export const useUpdateScenario = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async ({
      id,
      data,
    }: {
      id: string;
      data: Partial<Scenario>;
    }) => {
      const response = await apiClient.put(`/simulator/scenarios/${id}`, data);
      return response.data;
    },
    onSuccess: (_, { id }) => {
      queryClient.invalidateQueries({ queryKey: ['scenarios'] });
      queryClient.invalidateQueries({ queryKey: ['scenario', id] });
    },
  });
};

export const useDeleteScenario = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (id: string) => {
      await apiClient.delete(`/simulator/scenarios/${id}`);
      return id;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['scenarios'] });
    },
  });
};

// Template Management Hooks
export const useTemplates = (params?: {
  category?: string;
  search?: string;
}) => {
  return useQuery({
    queryKey: ['templates', params],
    queryFn: async () => {
      const response = await apiClient.get('/simulator/templates', { params });
      return response.data;
    },
  });
};

export const useTemplate = (id: string) => {
  return useQuery({
    queryKey: ['template', id],
    queryFn: async () => {
      const response = await apiClient.get(`/simulator/templates/${id}`);
      return response.data;
    },
    enabled: !!id,
  });
};

export const useCreateScenarioFromTemplate = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (templateId: string) => {
      const response = await apiClient.post(`/simulator/templates/${templateId}/create-scenario`);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['scenarios'] });
    },
  });
};

// Simulation Execution Hooks
export const useRunSimulation = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (data: {
      scenario_id: string;
      parameters?: Record<string, any>;
    }) => {
      const response = await apiClient.post('/simulator/run', data);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: ['executions'] });
    },
  });
};

export const useExecution = (id: string) => {
  return useQuery({
    queryKey: ['execution', id],
    queryFn: async () => {
      const response = await apiClient.get(`/simulator/executions/${id}`);
      return response.data;
    },
    enabled: !!id,
    refetchInterval: (data) => {
      // Refetch every 5 seconds if execution is still running
      if (data?.status === 'pending' || data?.status === 'running') {
        return 5000;
      }
      return false;
    },
  });
};

export const useCancelExecution = () => {
  const queryClient = useQueryClient();
  return useMutation({
    mutationFn: async (id: string) => {
      const response = await apiClient.delete(`/simulator/executions/${id}`);
      return response.data;
    },
    onSuccess: (_, id) => {
      queryClient.invalidateQueries({ queryKey: ['execution', id] });
      queryClient.invalidateQueries({ queryKey: ['executions'] });
    },
  });
};

// Results and Analytics Hooks
export const useSimulationResult = (executionId: string) => {
  return useQuery({
    queryKey: ['result', executionId],
    queryFn: async () => {
      const response = await apiClient.get(`/simulator/results/${executionId}`);
      return response.data;
    },
    enabled: !!executionId,
  });
};

export const useSimulationHistory = (params?: {
  page?: number;
  limit?: number;
  scenario_id?: string;
  status?: string;
  start_date?: string;
  end_date?: string;
}) => {
  return useQuery({
    queryKey: ['history', params],
    queryFn: async () => {
      const response = await apiClient.get('/simulator/history', { params });
      return response.data;
    },
  });
};

export const useSimulationAnalytics = (params?: {
  scenario_id?: string;
  start_date?: string;
  end_date?: string;
}) => {
  return useQuery({
    queryKey: ['analytics', params],
    queryFn: async () => {
      const response = await apiClient.get('/simulator/analytics', { params });
      return response.data;
    },
  });
};

export const useScenarioMetrics = (scenarioId: string) => {
  return useQuery({
    queryKey: ['metrics', scenarioId],
    queryFn: async () => {
      const response = await apiClient.get(`/simulator/scenarios/${scenarioId}/metrics`);
      return response.data;
    },
    enabled: !!scenarioId,
  });
};

export const usePopularScenarios = (params?: {
  limit?: number;
  category?: string;
}) => {
  return useQuery({
    queryKey: ['popular-scenarios', params],
    queryFn: async () => {
      const response = await apiClient.get('/simulator/popular-scenarios', { params });
      return response.data;
    },
  });
};
