/**
 * Training Progress Component
 * Comprehensive progress tracking and learning path visualization
 * Production-grade progress monitoring with detailed analytics
 */

import React, { useState, useEffect } from 'react';
import {
  BookOpen,
  Clock,
  CheckCircle,
  Circle,
  Play,
  Award,
  TrendingUp,
  Calendar,
  Target,
  BarChart3,
  ChevronRight,
  ChevronDown,
  Lock,
  Unlock,
  AlertCircle,
  Star,
  Zap,
  Users,
  FileText,
  Video,
  MessageSquare,
  HelpCircle
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { formatDistanceToNow, format } from 'date-fns';
import {
  useCourse,
  useUserEnrollments,
  useUpdateProgress,
  useQuizResults,
  TrainingCourse,
  UserEnrollment,
  TrainingModule,
  QuizAttempt
} from '@/hooks/useTraining';

interface TrainingProgressProps {
  enrollmentId: string;
  courseId: string;
  userId: string;
  onModuleClick?: (moduleId: string) => void;
  onQuizStart?: (quizId: string) => void;
  onCompleteCourse?: () => void;
  className?: string;
}

interface ModuleProgress {
  module_id: string;
  status: 'not_started' | 'in_progress' | 'completed';
  progress_percentage: number;
  time_spent_minutes: number;
  last_accessed_at?: string;
  completed_at?: string;
  score?: number;
  attempts_count: number;
}

interface CourseProgress {
  enrollment_id: string;
  course_id: string;
  overall_progress: number;
  modules_completed: number;
  total_modules: number;
  time_spent_total: number;
  estimated_completion_time: number;
  certificate_eligible: boolean;
  next_deadline?: string;
  streak_days: number;
  learning_velocity: number; // modules per day
  engagement_score: number;
  modules_progress: ModuleProgress[];
}

const PROGRESS_COLORS = {
  not_started: 'bg-gray-200 text-gray-600',
  in_progress: 'bg-blue-200 text-blue-600',
  completed: 'bg-green-200 text-green-600'
};

const STATUS_ICONS = {
  not_started: Circle,
  in_progress: Play,
  completed: CheckCircle
};

const CONTENT_TYPE_ICONS = {
  video: Video,
  text: FileText,
  interactive: Users,
  quiz: HelpCircle,
  document: FileText
};

const TrainingProgress: React.FC<TrainingProgressProps> = ({
  enrollmentId,
  courseId,
  userId,
  onModuleClick,
  onQuizStart,
  onCompleteCourse,
  className = ''
}) => {
  const [expandedModules, setExpandedModules] = useState<Set<string>>(new Set());
  const [selectedModule, setSelectedModule] = useState<string | null>(null);

  // Mock progress data (replace with real API calls)
  const [progress, setProgress] = useState<CourseProgress>({
    enrollment_id: enrollmentId,
    course_id: courseId,
    overall_progress: 65,
    modules_completed: 4,
    total_modules: 8,
    time_spent_total: 180,
    estimated_completion_time: 240,
    certificate_eligible: false,
    next_deadline: '2024-02-15T23:59:59Z',
    streak_days: 5,
    learning_velocity: 0.8,
    engagement_score: 85,
    modules_progress: [
      {
        module_id: 'module-1',
        status: 'completed',
        progress_percentage: 100,
        time_spent_minutes: 45,
        last_accessed_at: '2024-01-20T14:30:00Z',
        completed_at: '2024-01-20T14:30:00Z',
        score: 92,
        attempts_count: 1
      },
      {
        module_id: 'module-2',
        status: 'completed',
        progress_percentage: 100,
        time_spent_minutes: 38,
        last_accessed_at: '2024-01-21T10:15:00Z',
        completed_at: '2024-01-21T10:15:00Z',
        score: 88,
        attempts_count: 1
      },
      {
        module_id: 'module-3',
        status: 'in_progress',
        progress_percentage: 75,
        time_spent_minutes: 52,
        last_accessed_at: '2024-01-22T16:45:00Z',
        attempts_count: 2
      },
      {
        module_id: 'module-4',
        status: 'not_started',
        progress_percentage: 0,
        time_spent_minutes: 0,
        attempts_count: 0
      },
      {
        module_id: 'module-5',
        status: 'not_started',
        progress_percentage: 0,
        time_spent_minutes: 0,
        attempts_count: 0
      },
      {
        module_id: 'module-6',
        status: 'not_started',
        progress_percentage: 0,
        time_spent_minutes: 0,
        attempts_count: 0
      },
      {
        module_id: 'module-7',
        status: 'not_started',
        progress_percentage: 0,
        time_spent_minutes: 0,
        attempts_count: 0
      },
      {
        module_id: 'module-8',
        status: 'not_started',
        progress_percentage: 0,
        time_spent_minutes: 0,
        attempts_count: 0
      }
    ]
  });

  const { data: course, isLoading: courseLoading } = useCourse(courseId);
  const { data: enrollments = [] } = useUserEnrollments(userId);
  const { data: quizResults = [] } = useQuizResults(enrollmentId);
  const updateProgressMutation = useUpdateProgress();

  const enrollment = enrollments.find(e => e.enrollment_id === enrollmentId);

  const toggleModuleExpansion = (moduleId: string) => {
    const newExpanded = new Set(expandedModules);
    if (newExpanded.has(moduleId)) {
      newExpanded.delete(moduleId);
    } else {
      newExpanded.add(moduleId);
    }
    setExpandedModules(newExpanded);
  };

  const handleModuleClick = (moduleId: string) => {
    setSelectedModule(moduleId);
    if (onModuleClick) {
      onModuleClick(moduleId);
    }
  };

  const getModuleProgress = (moduleId: string): ModuleProgress | undefined => {
    return progress.modules_progress.find(m => m.module_id === moduleId);
  };

  const getNextModule = (): string | null => {
    const inProgressModule = progress.modules_progress.find(m => m.status === 'in_progress');
    if (inProgressModule) return inProgressModule.module_id;

    const firstNotStarted = progress.modules_progress.find(m => m.status === 'not_started');
    return firstNotStarted?.module_id || null;
  };

  const formatTime = (minutes: number): string => {
    const hours = Math.floor(minutes / 60);
    const mins = minutes % 60;
    if (hours > 0) {
      return `${hours}h ${mins}m`;
    }
    return `${mins}m`;
  };

  const getEngagementColor = (score: number): string => {
    if (score >= 80) return 'text-green-600';
    if (score >= 60) return 'text-yellow-600';
    return 'text-red-600';
  };

  const getEngagementLabel = (score: number): string => {
    if (score >= 80) return 'High';
    if (score >= 60) return 'Medium';
    return 'Low';
  };

  if (courseLoading || !course) {
    return (
      <div className="flex items-center justify-center min-h-96">
        <LoadingSpinner />
      </div>
    );
  }

  const nextModule = getNextModule();

  return (
    <div className={clsx('max-w-6xl mx-auto space-y-6', className)}>
      {/* Header */}
      <div className="bg-white rounded-lg border shadow-sm p-6">
        <div className="flex items-center justify-between mb-6">
          <div>
            <h1 className="text-2xl font-bold text-gray-900">{course.title}</h1>
            <p className="text-gray-600 mt-1">{course.description}</p>
            <div className="flex items-center gap-4 mt-3 text-sm text-gray-500">
              <span className="flex items-center gap-1">
                <Clock className="w-4 h-4" />
                Enrolled {enrollment ? formatDistanceToNow(new Date(enrollment.enrollment_date), { addSuffix: true }) : 'Recently'}
              </span>
              {enrollment?.last_accessed_at && (
                <span className="flex items-center gap-1">
                  <Calendar className="w-4 h-4" />
                  Last accessed {formatDistanceToNow(new Date(enrollment.last_accessed_at), { addSuffix: true })}
                </span>
              )}
            </div>
          </div>

          {enrollment?.certificate_issued && (
            <div className="flex items-center gap-2 px-4 py-2 bg-green-100 text-green-800 rounded-lg">
              <Award className="w-5 h-5" />
              <span className="font-medium">Certificate Earned</span>
            </div>
          )}
        </div>

        {/* Progress Overview */}
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4 mb-6">
          <div className="bg-blue-50 rounded-lg p-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-sm font-medium text-blue-900">Overall Progress</span>
              <Target className="w-5 h-5 text-blue-600" />
            </div>
            <div className="text-2xl font-bold text-blue-900">{progress.overall_progress}%</div>
            <div className="w-full bg-blue-200 rounded-full h-2 mt-2">
              <div
                className="bg-blue-600 h-2 rounded-full transition-all duration-300"
                style={{ width: `${progress.overall_progress}%` }}
              />
            </div>
          </div>

          <div className="bg-green-50 rounded-lg p-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-sm font-medium text-green-900">Modules Completed</span>
              <CheckCircle className="w-5 h-5 text-green-600" />
            </div>
            <div className="text-2xl font-bold text-green-900">
              {progress.modules_completed}/{progress.total_modules}
            </div>
          </div>

          <div className="bg-purple-50 rounded-lg p-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-sm font-medium text-purple-900">Time Spent</span>
              <Clock className="w-5 h-5 text-purple-600" />
            </div>
            <div className="text-2xl font-bold text-purple-900">
              {formatTime(progress.time_spent_total)}
            </div>
          </div>

          <div className="bg-orange-50 rounded-lg p-4">
            <div className="flex items-center justify-between mb-2">
              <span className="text-sm font-medium text-orange-900">Engagement</span>
              <TrendingUp className="w-5 h-5 text-orange-600" />
            </div>
            <div className={clsx('text-2xl font-bold', getEngagementColor(progress.engagement_score))}>
              {getEngagementLabel(progress.engagement_score)}
            </div>
            <div className="text-sm text-gray-600">{progress.engagement_score}/100</div>
          </div>
        </div>

        {/* Learning Stats */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
          <div className="text-center">
            <div className="text-lg font-semibold text-gray-900">{progress.streak_days}</div>
            <div className="text-sm text-gray-600">Day Streak</div>
          </div>
          <div className="text-center">
            <div className="text-lg font-semibold text-gray-900">{progress.learning_velocity.toFixed(1)}</div>
            <div className="text-sm text-gray-600">Modules/Day</div>
          </div>
          <div className="text-center">
            <div className="text-lg font-semibold text-gray-900">
              {formatTime(progress.estimated_completion_time - progress.time_spent_total)}
            </div>
            <div className="text-sm text-gray-600">Time Remaining</div>
          </div>
        </div>
      </div>

      {/* Next Action Banner */}
      {nextModule && (
        <div className="bg-blue-50 border border-blue-200 rounded-lg p-4">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-3">
              <div className="w-10 h-10 bg-blue-100 rounded-full flex items-center justify-center">
                <Play className="w-5 h-5 text-blue-600" />
              </div>
              <div>
                <h3 className="font-medium text-blue-900">Continue Learning</h3>
                <p className="text-sm text-blue-700">
                  {progress.modules_completed === 0
                    ? "Start your first module to begin the course"
                    : `Continue with the next module to keep your progress going`
                  }
                </p>
              </div>
            </div>
            <button
              onClick={() => handleModuleClick(nextModule)}
              className="bg-blue-600 hover:bg-blue-700 text-white px-4 py-2 rounded-lg transition-colors flex items-center gap-2"
            >
              <span>Continue</span>
              <ChevronRight className="w-4 h-4" />
            </button>
          </div>
        </div>
      )}

      {/* Course Modules */}
      <div className="bg-white rounded-lg border shadow-sm">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900">Course Modules</h2>
          <p className="text-gray-600 mt-1">Track your progress through each module</p>
        </div>

        <div className="divide-y divide-gray-200">
          {course.modules?.map((module, index) => {
            const moduleProgress = getModuleProgress(module.module_id);
            const StatusIcon = STATUS_ICONS[moduleProgress?.status || 'not_started'];
            const ContentIcon = CONTENT_TYPE_ICONS[module.content_type];
            const isExpanded = expandedModules.has(module.module_id);
            const isLocked = index > 0 && getModuleProgress(course.modules![index - 1].module_id)?.status !== 'completed';

            return (
              <div key={module.module_id} className="p-6">
                <div className="flex items-center justify-between mb-4">
                  <div className="flex items-center gap-4 flex-1">
                    <div className="flex items-center gap-2">
                      <div className={clsx(
                        'w-8 h-8 rounded-full flex items-center justify-center',
                        PROGRESS_COLORS[moduleProgress?.status || 'not_started']
                      )}>
                        <StatusIcon className="w-4 h-4" />
                      </div>
                      <span className="text-sm font-medium text-gray-500">Module {index + 1}</span>
                    </div>

                    <div className="flex items-center gap-2 flex-1">
                      <ContentIcon className="w-4 h-4 text-gray-400" />
                      <div>
                        <h3 className="font-medium text-gray-900">{module.title}</h3>
                        <p className="text-sm text-gray-600">{module.description}</p>
                      </div>
                    </div>
                  </div>

                  <div className="flex items-center gap-4">
                    <div className="text-right text-sm text-gray-600">
                      <div>{formatTime(module.duration_minutes)}</div>
                      {moduleProgress?.time_spent_minutes && moduleProgress.time_spent_minutes > 0 && (
                        <div className="text-xs">
                          Spent: {formatTime(moduleProgress.time_spent_minutes)}
                        </div>
                      )}
                    </div>

                    <div className="flex items-center gap-2">
                      {module.is_required && (
                        <span className="px-2 py-1 text-xs font-medium bg-red-100 text-red-800 rounded">
                          Required
                        </span>
                      )}

                      <button
                        onClick={() => toggleModuleExpansion(module.module_id)}
                        className="p-1 text-gray-400 hover:text-gray-600 transition-colors"
                      >
                        {isExpanded ? <ChevronDown className="w-4 h-4" /> : <ChevronRight className="w-4 h-4" />}
                      </button>
                    </div>
                  </div>
                </div>

                {/* Progress Bar */}
                {moduleProgress && (
                  <div className="mb-4">
                    <div className="flex items-center justify-between text-sm text-gray-600 mb-1">
                      <span>Progress</span>
                      <span>{moduleProgress.progress_percentage}%</span>
                    </div>
                    <div className="w-full bg-gray-200 rounded-full h-2">
                      <div
                        className={clsx(
                          'h-2 rounded-full transition-all duration-300',
                          moduleProgress.status === 'completed' ? 'bg-green-600' :
                          moduleProgress.status === 'in_progress' ? 'bg-blue-600' :
                          'bg-gray-400'
                        )}
                        style={{ width: `${moduleProgress.progress_percentage}%` }}
                      />
                    </div>
                  </div>
                )}

                {/* Module Actions */}
                <div className="flex items-center justify-between">
                  <div className="flex items-center gap-2 text-sm text-gray-600">
                    {moduleProgress?.last_accessed_at && (
                      <span>
                        Last accessed {formatDistanceToNow(new Date(moduleProgress.last_accessed_at), { addSuffix: true })}
                      </span>
                    )}
                    {moduleProgress?.score && (
                      <span className="flex items-center gap-1">
                        <Star className="w-4 h-4 text-yellow-500" />
                        Score: {moduleProgress.score}%
                      </span>
                    )}
                  </div>

                  <div className="flex items-center gap-2">
                    {isLocked ? (
                      <div className="flex items-center gap-2 px-3 py-1 bg-gray-100 text-gray-500 rounded-lg text-sm">
                        <Lock className="w-4 h-4" />
                        Complete previous module first
                      </div>
                    ) : (
                      <button
                        onClick={() => handleModuleClick(module.module_id)}
                        className={clsx(
                          'px-4 py-2 rounded-lg text-sm font-medium transition-colors flex items-center gap-2',
                          moduleProgress?.status === 'completed'
                            ? 'bg-green-600 hover:bg-green-700 text-white'
                            : moduleProgress?.status === 'in_progress'
                            ? 'bg-blue-600 hover:bg-blue-700 text-white'
                            : 'bg-gray-600 hover:bg-gray-700 text-white'
                        )}
                      >
                        {moduleProgress?.status === 'completed' ? (
                          <>
                            <CheckCircle className="w-4 h-4" />
                            Review
                          </>
                        ) : moduleProgress?.status === 'in_progress' ? (
                          <>
                            <Play className="w-4 h-4" />
                            Continue
                          </>
                        ) : (
                          <>
                            <Play className="w-4 h-4" />
                            Start
                          </>
                        )}
                      </button>
                    )}
                  </div>
                </div>

                {/* Expanded Details */}
                {isExpanded && (
                  <div className="mt-4 pt-4 border-t border-gray-200">
                    <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                      <div>
                        <h4 className="font-medium text-gray-900 mb-2">Module Details</h4>
                        <div className="space-y-2 text-sm text-gray-600">
                          <div className="flex justify-between">
                            <span>Type:</span>
                            <span className="capitalize">{module.content_type}</span>
                          </div>
                          <div className="flex justify-between">
                            <span>Duration:</span>
                            <span>{formatTime(module.duration_minutes)}</span>
                          </div>
                          <div className="flex justify-between">
                            <span>Required:</span>
                            <span>{module.is_required ? 'Yes' : 'No'}</span>
                          </div>
                          {moduleProgress?.attempts_count && (
                            <div className="flex justify-between">
                              <span>Attempts:</span>
                              <span>{moduleProgress.attempts_count}</span>
                            </div>
                          )}
                        </div>
                      </div>

                      <div>
                        <h4 className="font-medium text-gray-900 mb-2">Learning Objectives</h4>
                        <p className="text-sm text-gray-600">
                          {module.description}
                        </p>
                        {module.content_url && (
                          <div className="mt-2">
                            <a
                              href={module.content_url}
                              target="_blank"
                              rel="noopener noreferrer"
                              className="text-blue-600 hover:text-blue-800 text-sm underline"
                            >
                              View Content →
                            </a>
                          </div>
                        )}
                      </div>
                    </div>
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </div>

      {/* Quiz Results */}
      {quizResults.length > 0 && (
        <div className="bg-white rounded-lg border shadow-sm">
          <div className="p-6 border-b border-gray-200">
            <h2 className="text-xl font-semibold text-gray-900">Quiz Performance</h2>
            <p className="text-gray-600 mt-1">Your quiz results and scores</p>
          </div>

          <div className="p-6">
            <div className="space-y-4">
              {quizResults.map((result) => (
                <div key={result.attempt_id} className="border border-gray-200 rounded-lg p-4">
                  <div className="flex items-center justify-between mb-2">
                    <h3 className="font-medium text-gray-900">Quiz Attempt</h3>
                    <div className="flex items-center gap-2">
                      <span className={clsx(
                        'px-2 py-1 text-xs font-medium rounded',
                        result.passed ? 'bg-green-100 text-green-800' : 'bg-red-100 text-red-800'
                      )}>
                        {result.passed ? 'Passed' : 'Failed'}
                      </span>
                      <span className="text-sm text-gray-600">
                        {result.percentage}% ({result.score}/{result.max_score})
                      </span>
                    </div>
                  </div>

                  <div className="grid grid-cols-1 md:grid-cols-3 gap-4 text-sm text-gray-600">
                    <div>
                      <span className="font-medium">Submitted:</span>
                      <span className="ml-1">{format(new Date(result.submitted_at), 'MMM dd, yyyy HH:mm')}</span>
                    </div>
                    <div>
                      <span className="font-medium">Time Taken:</span>
                      <span className="ml-1">{formatTime(result.time_taken_minutes)}</span>
                    </div>
                    <div>
                      <span className="font-medium">Attempts:</span>
                      <span className="ml-1">1</span>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        </div>
      )}

      {/* Certificate Eligibility */}
      {progress.certificate_eligible && !enrollment?.certificate_issued && (
        <div className="bg-gradient-to-r from-yellow-400 to-orange-500 rounded-lg p-6 text-white">
          <div className="flex items-center justify-between">
            <div className="flex items-center gap-4">
              <Award className="w-12 h-12" />
              <div>
                <h3 className="text-xl font-bold">Congratulations!</h3>
                <p className="opacity-90">You have successfully completed this course and are eligible for a certificate.</p>
                <div className="mt-2 text-sm opacity-75">
                  Completion Rate: {progress.overall_progress}% • Time Spent: {formatTime(progress.time_spent_total)}
                </div>
              </div>
            </div>
            <button
              onClick={onCompleteCourse}
              className="bg-white text-orange-600 px-6 py-3 rounded-lg font-medium hover:bg-gray-50 transition-colors"
            >
              Claim Certificate
            </button>
          </div>
        </div>
      )}
    </div>
  );
};

export default TrainingProgress;
