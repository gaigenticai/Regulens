import { useQuery } from '@tanstack/react-query';
import { apiClient } from '@/services/api';

export function useTrainingCourses() {
  return useQuery({
    queryKey: ['training-courses'],
    queryFn: () => apiClient.getTrainingCourses(),
  });
}

export function useTrainingLeaderboard() {
  return useQuery({
    queryKey: ['training-leaderboard'],
    queryFn: () => apiClient.getTrainingLeaderboard(),
    refetchInterval: 60000,
  });
}
