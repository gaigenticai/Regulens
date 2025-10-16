/**
 * Training System API Hooks
 * Production-grade hooks for training system operations
 * NO MOCKS - Real API integration following RESTful conventions
 */

import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { apiClient } from '@/services/api';

// Types matching backend training system
export interface TrainingCourse {
  course_id: string;
  title: string;
  description: string;
  course_type: 'compliance' | 'technical' | 'regulatory' | 'security' | 'ethics' | 'general';
  difficulty_level: 'beginner' | 'intermediate' | 'advanced' | 'expert';
  duration_minutes: number;
  pass_threshold: number;
  tags: string[];
  is_active: boolean;
  created_at: string;
  updated_at: string;
  created_by: string;
  enrollment_count?: number;
  completion_rate?: number;
  average_score?: number;
  modules?: TrainingModule[];
  prerequisites?: string[];
}

export interface TrainingModule {
  module_id: string;
  course_id: string;
  title: string;
  description: string;
  content_type: 'video' | 'text' | 'interactive' | 'quiz' | 'document';
  content_url?: string;
  content_text?: string;
  duration_minutes: number;
  order_index: number;
  is_required: boolean;
  created_at: string;
}

export interface UserEnrollment {
  enrollment_id: string;
  user_id: string;
  course_id: string;
  enrollment_date: string;
  completion_date?: string;
  status: 'enrolled' | 'in_progress' | 'completed' | 'failed' | 'expired';
  progress_percentage: number;
  last_accessed_at?: string;
  certificate_issued?: boolean;
  certificate_url?: string;
  quiz_attempts: QuizAttempt[];
  current_module_id?: string;
}

export interface QuizAttempt {
  attempt_id: string;
  enrollment_id: string;
  quiz_id: string;
  score: number;
  max_score: number;
  percentage: number;
  passed: boolean;
  submitted_at: string;
  time_taken_minutes: number;
  answers: Record<string, any>;
}

export interface QuizQuestion {
  question_id: string;
  question_text: string;
  question_type: 'multiple_choice' | 'true_false' | 'short_answer' | 'essay';
  options?: string[];
  correct_answer: any;
  points: number;
  explanation?: string;
}

export interface TrainingQuiz {
  quiz_id: string;
  course_id: string;
  module_id?: string;
  title: string;
  description: string;
  questions: QuizQuestion[];
  time_limit_minutes?: number;
  passing_score: number;
  max_attempts: number;
  is_active: boolean;
  created_at: string;
  updated_at: string;
}

export interface TrainingCertificate {
  certificate_id: string;
  enrollment_id: string;
  user_id: string;
  course_id: string;
  certificate_number: string;
  issued_date: string;
  expiry_date?: string;
  verification_code: string;
  certificate_url: string;
  status: 'active' | 'expired' | 'revoked';
  issued_by: string;
  metadata?: Record<string, any>;
}

export interface TrainingStats {
  total_courses: number;
  active_enrollments: number;
  completed_trainings: number;
  average_completion_rate: number;
  certifications_issued: number;
  popular_courses: Array<{
    course_id: string;
    title: string;
    enrollment_count: number;
    completion_rate: number;
  }>;
  department_completion_rates: Record<string, number>;
  monthly_training_hours: Array<{
    month: string;
    hours: number;
  }>;
  quiz_performance: Array<{
    quiz_id: string;
    title: string;
    average_score: number;
    pass_rate: number;
    attempts_count: number;
  }>;
}

export interface CourseFilters {
  course_type?: string;
  difficulty_level?: string;
  is_active?: boolean;
  tags?: string[];
  search?: string;
  created_by?: string;
  date_range?: {
    start: string;
    end: string;
  };
}

export interface EnrollmentFilters {
  user_id?: string;
  course_id?: string;
  status?: string;
  date_range?: {
    start: string;
    end: string;
  };
}

export interface CreateCourseRequest {
  title: string;
  description: string;
  course_type: 'compliance' | 'technical' | 'regulatory' | 'security' | 'ethics' | 'general';
  difficulty_level: 'beginner' | 'intermediate' | 'advanced' | 'expert';
  duration_minutes: number;
  pass_threshold: number;
  tags?: string[];
  modules?: Omit<TrainingModule, 'module_id' | 'course_id' | 'created_at'>[];
  prerequisites?: string[];
}

export interface UpdateCourseRequest extends Partial<CreateCourseRequest> {
  course_id: string;
}

export interface CreateQuizRequest {
  course_id: string;
  module_id?: string;
  title: string;
  description: string;
  questions: Omit<QuizQuestion, 'question_id'>[];
  time_limit_minutes?: number;
  passing_score: number;
  max_attempts: number;
}

export interface SubmitQuizRequest {
  quiz_id: string;
  answers: Record<string, any>;
  time_taken_minutes: number;
}

export interface EnrollUserRequest {
  user_id: string;
  course_id: string;
  enrollment_date?: string;
  notes?: string;
}

export interface UpdateProgressRequest {
  enrollment_id: string;
  progress_percentage: number;
  current_module_id?: string;
  completed_modules?: string[];
  notes?: string;
}

// Query Keys
export const trainingKeys = {
  all: ['training'] as const,
  courses: () => [...trainingKeys.all, 'courses'] as const,
  course: (id: string) => [...trainingKeys.courses(), id] as const,
  courseList: (filters: CourseFilters) => [...trainingKeys.courses(), 'list', filters] as const,
  enrollments: () => [...trainingKeys.all, 'enrollments'] as const,
  enrollment: (id: string) => [...trainingKeys.enrollments(), id] as const,
  enrollmentList: (filters: EnrollmentFilters) => [...trainingKeys.enrollments(), 'list', filters] as const,
  userEnrollments: (userId: string) => [...trainingKeys.enrollments(), 'user', userId] as const,
  quizzes: () => [...trainingKeys.all, 'quizzes'] as const,
  quiz: (id: string) => [...trainingKeys.quizzes(), id] as const,
  certificates: () => [...trainingKeys.all, 'certificates'] as const,
  certificate: (id: string) => [...trainingKeys.certificates(), id] as const,
  userCertificates: (userId: string) => [...trainingKeys.certificates(), 'user', userId] as const,
  stats: () => [...trainingKeys.all, 'stats'] as const,
  leaderboard: () => [...trainingKeys.all, 'leaderboard'] as const,
  analytics: () => [...trainingKeys.all, 'analytics'] as const,
};

// Get all courses with filtering
export const useCourses = (filters?: CourseFilters) => {
  return useQuery({
    queryKey: trainingKeys.courseList(filters || {}),
    queryFn: async () => {
      const params = new URLSearchParams();

      if (filters?.course_type) params.append('type', filters.course_type);
      if (filters?.difficulty_level) params.append('difficulty', filters.difficulty_level);
      if (filters?.is_active !== undefined) params.append('active', filters.is_active.toString());
      if (filters?.tags?.length) params.append('tags', filters.tags.join(','));
      if (filters?.search) params.append('search', filters.search);
      if (filters?.created_by) params.append('createdBy', filters.created_by);
      if (filters?.date_range) {
        params.append('startDate', filters.date_range.start);
        params.append('endDate', filters.date_range.end);
      }

      const response = await apiClient.get<TrainingCourse[]>(`/training/courses?${params.toString()}`);
      return response.data;
    },
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Alias for backward compatibility with existing code
export const useTrainingCourses = useCourses;

// Get single course by ID
export const useCourse = (courseId: string) => {
  return useQuery({
    queryKey: trainingKeys.course(courseId),
    queryFn: async () => {
      const response = await apiClient.get<TrainingCourse>(`/training/courses/${courseId}`);
      return response.data;
    },
    enabled: !!courseId,
    staleTime: 2 * 60 * 1000, // 2 minutes
  });
};

// Create new course
export const useCreateCourse = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (course: CreateCourseRequest) => {
      const response = await apiClient.post<TrainingCourse>('/training/courses', course);
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.courses() });
    },
  });
};

// Update existing course
export const useUpdateCourse = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ courseId, course }: { courseId: string; course: UpdateCourseRequest }) => {
      const response = await apiClient.put<TrainingCourse>(`/training/courses/${courseId}`, course);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.courses() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.course(data.course_id) });
    },
  });
};

// Delete course
export const useDeleteCourse = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (courseId: string) => {
      await apiClient.delete(`/training/courses/${courseId}`);
      return courseId;
    },
    onSuccess: (courseId) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.courses() });
      queryClient.removeQueries({ queryKey: trainingKeys.course(courseId) });
    },
  });
};

// Get user enrollments
export const useUserEnrollments = (userId: string, filters?: EnrollmentFilters) => {
  return useQuery({
    queryKey: [...trainingKeys.userEnrollments(userId), filters],
    queryFn: async () => {
      const params = new URLSearchParams();

      if (filters?.course_id) params.append('courseId', filters.course_id);
      if (filters?.status) params.append('status', filters.status);
      if (filters?.date_range) {
        params.append('startDate', filters.date_range.start);
        params.append('endDate', filters.date_range.end);
      }

      const response = await apiClient.get<UserEnrollment[]>(`/training/progress?userId=${userId}&${params.toString()}`);
      return response.data;
    },
    enabled: !!userId,
    staleTime: 60 * 1000, // 1 minute
  });
};

// Enroll user in course
export const useEnrollUser = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (enrollment: EnrollUserRequest) => {
      const response = await apiClient.post<UserEnrollment>(`/training/courses/${enrollment.course_id}/enroll`, {
        user_id: enrollment.user_id,
        enrollment_date: enrollment.enrollment_date,
        notes: enrollment.notes,
      });
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollments() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.userEnrollments(data.user_id) });
      queryClient.invalidateQueries({ queryKey: trainingKeys.course(data.course_id) });
    },
  });
};

// Update enrollment progress
export const useUpdateProgress = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (progress: UpdateProgressRequest) => {
      const response = await apiClient.put<UserEnrollment>(`/training/enrollments/${progress.enrollment_id}/progress`, {
        progress_percentage: progress.progress_percentage,
        current_module_id: progress.current_module_id,
        completed_modules: progress.completed_modules,
        notes: progress.notes,
      });
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollments() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollment(data.enrollment_id) });
      queryClient.invalidateQueries({ queryKey: trainingKeys.userEnrollments(data.user_id) });
    },
  });
};

// Mark course as completed
export const useCompleteCourse = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ courseId, userId }: { courseId: string; userId: string }) => {
      const response = await apiClient.post<UserEnrollment>(`/training/courses/${courseId}/complete`, { user_id: userId });
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollments() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollment(data.enrollment_id) });
      queryClient.invalidateQueries({ queryKey: trainingKeys.userEnrollments(data.user_id) });
      queryClient.invalidateQueries({ queryKey: trainingKeys.certificates() });
    },
  });
};

// Get quiz by ID
export const useQuiz = (quizId: string) => {
  return useQuery({
    queryKey: trainingKeys.quiz(quizId),
    queryFn: async () => {
      const response = await apiClient.get<TrainingQuiz>(`/training/quizzes/${quizId}`);
      return response.data;
    },
    enabled: !!quizId,
    staleTime: 10 * 60 * 1000, // 10 minutes
  });
};

// Submit quiz answers
export const useSubmitQuiz = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (submission: SubmitQuizRequest) => {
      const response = await apiClient.post<QuizAttempt>(`/training/quizzes/${submission.quiz_id}/submit`, {
        answers: submission.answers,
        time_taken_minutes: submission.time_taken_minutes,
      });
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollments() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollment(data.enrollment_id) });
      queryClient.invalidateQueries({ queryKey: trainingKeys.quiz(data.quiz_id) });
    },
  });
};

// Get quiz results for enrollment
export const useQuizResults = (enrollmentId: string) => {
  return useQuery({
    queryKey: [...trainingKeys.enrollment(enrollmentId), 'quiz-results'],
    queryFn: async () => {
      const response = await apiClient.get<QuizAttempt[]>(`/training/enrollments/${enrollmentId}/quiz-results`);
      return response.data;
    },
    enabled: !!enrollmentId,
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Get user certificates
export const useUserCertificates = (userId: string) => {
  return useQuery({
    queryKey: trainingKeys.userCertificates(userId),
    queryFn: async () => {
      const response = await apiClient.get<TrainingCertificate[]>(`/training/certifications?userId=${userId}`);
      return response.data;
    },
    enabled: !!userId,
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Issue certificate
export const useIssueCertificate = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (enrollmentId: string) => {
      const response = await apiClient.post<TrainingCertificate>(`/training/enrollments/${enrollmentId}/certificate`);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.certificates() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.userCertificates(data.user_id) });
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollment(data.enrollment_id) });
    },
  });
};

// Verify certificate
export const useVerifyCertificate = () => {
  return useMutation({
    mutationFn: async (verificationCode: string) => {
      const response = await apiClient.get<TrainingCertificate>(`/training/certificates/${verificationCode}/verify`);
      return response.data;
    },
  });
};

// Get training statistics
export const useTrainingStats = () => {
  return useQuery({
    queryKey: trainingKeys.stats(),
    queryFn: async () => {
      const response = await apiClient.get<TrainingStats>('/training/analytics');
      return response.data;
    },
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Get training leaderboard
export const useTrainingLeaderboard = (limit = 50) => {
  return useQuery({
    queryKey: [...trainingKeys.leaderboard(), limit],
    queryFn: async () => {
      const response = await apiClient.get<Array<{
        user_id: string;
        username: string;
        full_name: string;
        total_courses_completed: number;
        total_certifications: number;
        average_score: number;
        total_training_hours: number;
        rank: number;
      }>>(`/training/leaderboard?limit=${limit}`);
      return response.data;
    },
    staleTime: 10 * 60 * 1000, // 10 minutes
  });
};

// Get detailed training analytics
export const useTrainingAnalytics = (timeRange?: string) => {
  return useQuery({
    queryKey: [...trainingKeys.analytics(), timeRange],
    queryFn: async () => {
      const params = timeRange ? `?timeRange=${timeRange}` : '';
      const response = await apiClient.get<{
        course_completion_trends: Array<{ date: string; completed: number; enrolled: number }>;
        quiz_performance_trends: Array<{ date: string; average_score: number; pass_rate: number }>;
        department_analytics: Record<string, {
          total_users: number;
          completed_courses: number;
          average_completion_time: number;
          certification_rate: number;
        }>;
        compliance_metrics: {
          mandatory_training_completion_rate: number;
          overdue_trainings: number;
          expiring_certifications: number;
        };
        engagement_metrics: {
          average_session_duration: number;
          course_completion_velocity: number;
          repeat_training_rate: number;
        };
      }>(`/training/analytics/detailed${params}`);
      return response.data;
    },
    staleTime: 15 * 60 * 1000, // 15 minutes
  });
};

// Create quiz
export const useCreateQuiz = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (quiz: CreateQuizRequest) => {
      const response = await apiClient.post<TrainingQuiz>('/training/quizzes', quiz);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.quizzes() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.course(data.course_id) });
    },
  });
};

// Update quiz
export const useUpdateQuiz = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({ quizId, quiz }: { quizId: string; quiz: Partial<TrainingQuiz> }) => {
      const response = await apiClient.put<TrainingQuiz>(`/training/quizzes/${quizId}`, quiz);
      return response.data;
    },
    onSuccess: (data) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.quizzes() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.quiz(data.quiz_id) });
      queryClient.invalidateQueries({ queryKey: trainingKeys.course(data.course_id) });
    },
  });
};

// Delete quiz
export const useDeleteQuiz = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async (quizId: string) => {
      await apiClient.delete(`/training/quizzes/${quizId}`);
      return quizId;
    },
    onSuccess: (quizId, quizId2) => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.quizzes() });
      queryClient.removeQueries({ queryKey: trainingKeys.quiz(quizId2) });
    },
  });
};

// Bulk enroll users
export const useBulkEnrollUsers = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      courseId,
      userIds,
      enrollmentDate
    }: {
      courseId: string;
      userIds: string[];
      enrollmentDate?: string;
    }) => {
      const response = await apiClient.post<UserEnrollment[]>('/training/bulk-enroll', {
        course_id: courseId,
        user_ids: userIds,
        enrollment_date: enrollmentDate,
      });
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.enrollments() });
      queryClient.invalidateQueries({ queryKey: trainingKeys.courses() });
    },
  });
};

// Export training data
export const useExportTrainingData = () => {
  return useMutation({
    mutationFn: async ({
      dataType,
      format = 'csv',
      filters
    }: {
      dataType: 'courses' | 'enrollments' | 'certificates' | 'analytics';
      format?: 'csv' | 'excel' | 'pdf';
      filters?: Record<string, any>;
    }) => {
      const params = new URLSearchParams();
      params.append('format', format);
      if (filters) {
        Object.entries(filters).forEach(([key, value]) => {
          params.append(key, String(value));
        });
      }

      const response = await apiClient.get(`/training/export/${dataType}?${params.toString()}`, {
        responseType: 'blob',
      });
      return {
        data: response.data,
        filename: `training_${dataType}_${new Date().toISOString().split('T')[0]}.${format}`
      };
    },
  });
};

// Import training data
export const useImportTrainingData = () => {
  const queryClient = useQueryClient();

  return useMutation({
    mutationFn: async ({
      dataType,
      file
    }: {
      dataType: 'courses' | 'enrollments' | 'quizzes';
      file: File;
    }) => {
      const formData = new FormData();
      formData.append('file', file);
      formData.append('dataType', dataType);

      const response = await apiClient.post<{
        imported: number;
        skipped: number;
        errors: string[];
      }>(`/training/import/${dataType}`, formData, {
        headers: {
          'Content-Type': 'multipart/form-data',
        },
      });
      return response.data;
    },
    onSuccess: () => {
      queryClient.invalidateQueries({ queryKey: trainingKeys.all });
    },
  });
};

// Get course prerequisites
export const useCoursePrerequisites = (courseId: string) => {
  return useQuery({
    queryKey: [...trainingKeys.course(courseId), 'prerequisites'],
    queryFn: async () => {
      const response = await apiClient.get<Array<{
        prerequisite_id: string;
        required_course_id: string;
        course_title: string;
        is_completed: boolean;
        completed_date?: string;
      }>>(`/training/courses/${courseId}/prerequisites`);
      return response.data;
    },
    enabled: !!courseId,
    staleTime: 5 * 60 * 1000, // 5 minutes
  });
};

// Validate course configuration
export const useValidateCourse = () => {
  return useMutation({
    mutationFn: async (course: CreateCourseRequest | UpdateCourseRequest) => {
      const response = await apiClient.post<{
        valid: boolean;
        errors: string[];
        warnings: string[];
        suggestions: string[];
      }>('/training/courses/validate', course);
      return response.data;
    },
  });
};

// Get course recommendations for user
export const useRecommendedCourses = (userId: string) => {
  return useQuery({
    queryKey: [...trainingKeys.all, 'recommendations', userId],
    queryFn: async () => {
      const response = await apiClient.get<Array<{
        course: TrainingCourse;
        relevance_score: number;
        reason: string;
        priority: 'high' | 'medium' | 'low';
      }>>(`/training/recommendations/${userId}`);
      return response.data;
    },
    enabled: !!userId,
    staleTime: 30 * 60 * 1000, // 30 minutes
  });
};

// Send training reminders
export const useSendTrainingReminders = () => {
  return useMutation({
    mutationFn: async ({
      userIds,
      courseId,
      reminderType
    }: {
      userIds: string[];
      courseId?: string;
      reminderType: 'deadline' | 'overdue' | 'completion' | 'enrollment';
    }) => {
      const response = await apiClient.post<{
        sent: number;
        failed: number;
        errors: string[];
      }>('/training/reminders', {
        user_ids: userIds,
        course_id: courseId,
        reminder_type: reminderType,
      });
      return response.data;
    },
  });
};