/**
 * Training Management System
 * Comprehensive compliance training platform with courses, enrollments, quizzes, and certifications
 * Production-grade training management interface
 */

import React, { useState, useEffect } from 'react';
import {
  BookOpen,
  Users,
  Award,
  TrendingUp,
  Play,
  Pause,
  CheckCircle,
  Clock,
  AlertCircle,
  Search,
  Filter,
  Plus,
  Edit,
  Trash2,
  Download,
  Upload,
  BarChart3,
  Calendar,
  UserCheck,
  GraduationCap,
  Target,
  Star,
  FileText,
  Settings,
  Eye
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { formatDistanceToNow, format } from 'date-fns';
import CourseCreation from '@/components/Training/CourseCreation';
import TrainingProgress from '@/components/Training/TrainingProgress';
import CertificateManagement from '@/components/Training/CertificateManagement';
import TrainingAnalytics from '@/components/Training/TrainingAnalytics';

// Training Types (matching backend)
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
}

interface TrainingManagementProps {
  className?: string;
}

const COURSE_TYPES = [
  { value: 'compliance', label: 'Compliance', color: 'bg-blue-100 text-blue-800' },
  { value: 'technical', label: 'Technical', color: 'bg-green-100 text-green-800' },
  { value: 'regulatory', label: 'Regulatory', color: 'bg-purple-100 text-purple-800' },
  { value: 'security', label: 'Security', color: 'bg-red-100 text-red-800' },
  { value: 'ethics', label: 'Ethics', color: 'bg-yellow-100 text-yellow-800' },
  { value: 'general', label: 'General', color: 'bg-gray-100 text-gray-800' }
];

const DIFFICULTY_LEVELS = [
  { value: 'beginner', label: 'Beginner', color: 'bg-green-100 text-green-800' },
  { value: 'intermediate', label: 'Intermediate', color: 'bg-yellow-100 text-yellow-800' },
  { value: 'advanced', label: 'Advanced', color: 'bg-orange-100 text-orange-800' },
  { value: 'expert', label: 'Expert', color: 'bg-red-100 text-red-800' }
];

const TrainingManagement: React.FC<TrainingManagementProps> = ({ className = '' }) => {
  const [activeTab, setActiveTab] = useState<'courses' | 'enrollments' | 'progress' | 'certifications' | 'analytics' | 'settings'>('courses');
  const [courses, setCourses] = useState<TrainingCourse[]>([]);
  const [enrollments, setEnrollments] = useState<UserEnrollment[]>([]);
  const [stats, setStats] = useState<TrainingStats | null>(null);
  const [isLoading, setIsLoading] = useState(true);
  const [searchTerm, setSearchTerm] = useState('');
  const [selectedCourseType, setSelectedCourseType] = useState<string>('all');
  const [selectedDifficulty, setSelectedDifficulty] = useState<string>('all');
  const [showCreateCourse, setShowCreateCourse] = useState(false);
  const [selectedCourse, setSelectedCourse] = useState<TrainingCourse | null>(null);

  // Mock data for development (replace with real API calls)
  useEffect(() => {
    const loadTrainingData = async () => {
      setIsLoading(true);
      try {
        // Simulate API calls
        await new Promise(resolve => setTimeout(resolve, 1000));

        // Mock courses data
        const mockCourses: TrainingCourse[] = [
          {
            course_id: 'course-001',
            title: 'Anti-Money Laundering Fundamentals',
            description: 'Comprehensive training on AML regulations and compliance requirements',
            course_type: 'compliance',
            difficulty_level: 'intermediate',
            duration_minutes: 120,
            pass_threshold: 80,
            tags: ['AML', 'Compliance', 'Financial Crime'],
            is_active: true,
            created_at: '2024-01-15T10:00:00Z',
            updated_at: '2024-01-15T10:00:00Z',
            created_by: 'admin',
            enrollment_count: 245,
            completion_rate: 87.5,
            average_score: 82.3
          },
          {
            course_id: 'course-002',
            title: 'Data Protection and GDPR Compliance',
            description: 'Understanding data protection regulations and GDPR requirements',
            course_type: 'regulatory',
            difficulty_level: 'advanced',
            duration_minutes: 180,
            pass_threshold: 85,
            tags: ['GDPR', 'Data Protection', 'Privacy'],
            is_active: true,
            created_at: '2024-01-10T14:30:00Z',
            updated_at: '2024-01-10T14:30:00Z',
            created_by: 'admin',
            enrollment_count: 189,
            completion_rate: 92.1,
            average_score: 88.7
          },
          {
            course_id: 'course-003',
            title: 'Cybersecurity Awareness',
            description: 'Essential cybersecurity practices and threat awareness',
            course_type: 'security',
            difficulty_level: 'beginner',
            duration_minutes: 90,
            pass_threshold: 75,
            tags: ['Security', 'Cybersecurity', 'Awareness'],
            is_active: true,
            created_at: '2024-01-05T09:15:00Z',
            updated_at: '2024-01-05T09:15:00Z',
            created_by: 'admin',
            enrollment_count: 312,
            completion_rate: 78.9,
            average_score: 76.4
          }
        ];

        // Mock enrollments data
        const mockEnrollments: UserEnrollment[] = [
          {
            enrollment_id: 'enrollment-001',
            user_id: 'user-001',
            course_id: 'course-001',
            enrollment_date: '2024-01-20T08:00:00Z',
            status: 'in_progress',
            progress_percentage: 65,
            last_accessed_at: '2024-01-22T14:30:00Z',
            current_module_id: 'module-003',
            quiz_attempts: []
          },
          {
            enrollment_id: 'enrollment-002',
            user_id: 'user-002',
            course_id: 'course-002',
            enrollment_date: '2024-01-18T10:15:00Z',
            completion_date: '2024-01-21T16:45:00Z',
            status: 'completed',
            progress_percentage: 100,
            certificate_issued: true,
            certificate_url: '/certificates/cert-001.pdf',
            quiz_attempts: [{
              attempt_id: 'attempt-001',
              enrollment_id: 'enrollment-002',
              quiz_id: 'quiz-001',
              score: 42,
              max_score: 50,
              percentage: 84,
              passed: true,
              submitted_at: '2024-01-21T16:45:00Z',
              time_taken_minutes: 45
            }]
          }
        ];

        // Mock stats
        const mockStats: TrainingStats = {
          total_courses: 15,
          active_enrollments: 1247,
          completed_trainings: 892,
          average_completion_rate: 71.5,
          certifications_issued: 756,
          popular_courses: [
            { course_id: 'course-001', title: 'Anti-Money Laundering Fundamentals', enrollment_count: 245, completion_rate: 87.5 },
            { course_id: 'course-002', title: 'Data Protection and GDPR Compliance', enrollment_count: 189, completion_rate: 92.1 },
            { course_id: 'course-003', title: 'Cybersecurity Awareness', enrollment_count: 312, completion_rate: 78.9 }
          ],
          department_completion_rates: {
            'Compliance': 89.2,
            'IT Security': 76.8,
            'Operations': 82.4,
            'Finance': 94.1
          },
          monthly_training_hours: [
            { month: 'Jan 2024', hours: 2450 },
            { month: 'Dec 2023', hours: 2230 },
            { month: 'Nov 2023', hours: 1980 }
          ]
        };

        setCourses(mockCourses);
        setEnrollments(mockEnrollments);
        setStats(mockStats);
      } catch (error) {
        console.error('Failed to load training data:', error);
      } finally {
        setIsLoading(false);
      }
    };

    loadTrainingData();
  }, []);

  const filteredCourses = courses.filter(course => {
    const matchesSearch = course.title.toLowerCase().includes(searchTerm.toLowerCase()) ||
                         course.description.toLowerCase().includes(searchTerm.toLowerCase()) ||
                         course.tags.some(tag => tag.toLowerCase().includes(searchTerm.toLowerCase()));

    const matchesType = selectedCourseType === 'all' || course.course_type === selectedCourseType;
    const matchesDifficulty = selectedDifficulty === 'all' || course.difficulty_level === selectedDifficulty;

    return matchesSearch && matchesType && matchesDifficulty;
  });

  const getCourseTypeInfo = (type: string) => {
    return COURSE_TYPES.find(t => t.value === type) || COURSE_TYPES[5];
  };

  const getDifficultyInfo = (level: string) => {
    return DIFFICULTY_LEVELS.find(d => d.value === level) || DIFFICULTY_LEVELS[0];
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'completed': return 'bg-green-100 text-green-800';
      case 'in_progress': return 'bg-blue-100 text-blue-800';
      case 'enrolled': return 'bg-yellow-100 text-yellow-800';
      case 'failed': return 'bg-red-100 text-red-800';
      case 'expired': return 'bg-gray-100 text-gray-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  if (isLoading) {
    return (
      <div className="flex items-center justify-center min-h-screen">
        <LoadingSpinner />
      </div>
    );
  }

  return (
    <div className={clsx('space-y-6', className)}>
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold text-gray-900 flex items-center gap-3">
            <GraduationCap className="w-8 h-8 text-blue-600" />
            Training Management
          </h1>
          <p className="text-gray-600 mt-1">
            Comprehensive compliance training platform for regulatory education
          </p>
        </div>
        <div className="flex items-center gap-3">
          <button
            onClick={() => setShowCreateCourse(true)}
            className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
          >
            <Plus className="w-4 h-4" />
            Create Course
          </button>
          <button className="flex items-center gap-2 px-4 py-2 bg-gray-600 hover:bg-gray-700 text-white rounded-lg transition-colors">
            <Download className="w-4 h-4" />
            Export Report
          </button>
        </div>
      </div>

      {/* Stats Overview */}
      {stats && (
        <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
          <div className="bg-white rounded-lg border shadow-sm p-6">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-sm font-medium text-gray-600">Total Courses</p>
                <p className="text-2xl font-bold text-gray-900">{stats.total_courses}</p>
              </div>
              <BookOpen className="w-8 h-8 text-blue-600" />
            </div>
          </div>

          <div className="bg-white rounded-lg border shadow-sm p-6">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-sm font-medium text-gray-600">Active Enrollments</p>
                <p className="text-2xl font-bold text-gray-900">{stats.active_enrollments}</p>
              </div>
              <Users className="w-8 h-8 text-green-600" />
            </div>
          </div>

          <div className="bg-white rounded-lg border shadow-sm p-6">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-sm font-medium text-gray-600">Completion Rate</p>
                <p className="text-2xl font-bold text-gray-900">{stats.average_completion_rate}%</p>
              </div>
              <Target className="w-8 h-8 text-yellow-600" />
            </div>
          </div>

          <div className="bg-white rounded-lg border shadow-sm p-6">
            <div className="flex items-center justify-between">
              <div>
                <p className="text-sm font-medium text-gray-600">Certifications</p>
                <p className="text-2xl font-bold text-gray-900">{stats.certifications_issued}</p>
              </div>
              <Award className="w-8 h-8 text-purple-600" />
            </div>
          </div>
        </div>
      )}

      {/* Tabs */}
      <div className="border-b border-gray-200">
        <nav className="-mb-px flex space-x-8">
          {[
            { id: 'courses', label: 'Courses', icon: BookOpen },
            { id: 'enrollments', label: 'Enrollments', icon: Users },
            { id: 'progress', label: 'Progress', icon: Target },
            { id: 'certifications', label: 'Certifications', icon: Award },
            { id: 'analytics', label: 'Analytics', icon: BarChart3 },
            { id: 'settings', label: 'Settings', icon: Settings }
          ].map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              onClick={() => setActiveTab(id as any)}
              className={clsx(
                'flex items-center gap-2 py-4 px-1 border-b-2 font-medium text-sm transition-colors',
                activeTab === id
                  ? 'border-blue-500 text-blue-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              )}
            >
              <Icon className="w-4 h-4" />
              {label}
            </button>
          ))}
        </nav>
      </div>

      {/* Tab Content */}
      <div className="space-y-6">
        {showCreateCourse ? (
          <CourseCreation
            onCourseCreated={(course) => {
              // Refresh the courses list and go back to courses tab
              setCourses(prev => [...prev, course]);
              setShowCreateCourse(false);
              setActiveTab('courses');
            }}
            onCancel={() => setShowCreateCourse(false)}
          />
        ) : activeTab === 'courses' && (
          <div className="space-y-6">
            {/* Search and Filters */}
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex flex-col sm:flex-row gap-4">
                <div className="flex-1">
                  <div className="relative">
                    <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400 w-4 h-4" />
                    <input
                      type="text"
                      placeholder="Search courses..."
                      value={searchTerm}
                      onChange={(e) => setSearchTerm(e.target.value)}
                      className="w-full pl-10 pr-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    />
                  </div>
                </div>

                <div className="flex gap-2">
                  <select
                    value={selectedCourseType}
                    onChange={(e) => setSelectedCourseType(e.target.value)}
                    className="px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  >
                    <option value="all">All Types</option>
                    {COURSE_TYPES.map(type => (
                      <option key={type.value} value={type.value}>{type.label}</option>
                    ))}
                  </select>

                  <select
                    value={selectedDifficulty}
                    onChange={(e) => setSelectedDifficulty(e.target.value)}
                    className="px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                  >
                    <option value="all">All Levels</option>
                    {DIFFICULTY_LEVELS.map(level => (
                      <option key={level.value} value={level.value}>{level.label}</option>
                    ))}
                  </select>
                </div>
              </div>
            </div>

            {/* Courses Grid */}
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
              {filteredCourses.map((course) => (
                <div
                  key={course.course_id}
                  className="bg-white rounded-lg border shadow-sm hover:shadow-md transition-shadow cursor-pointer"
                  onClick={() => setSelectedCourse(course)}
                >
                  <div className="p-6">
                    <div className="flex items-start justify-between mb-4">
                      <div className="flex-1">
                        <h3 className="text-lg font-semibold text-gray-900 mb-2">{course.title}</h3>
                        <p className="text-sm text-gray-600 line-clamp-2">{course.description}</p>
                      </div>
                      <div className="flex items-center gap-1 ml-2">
                        {course.is_active ? (
                          <CheckCircle className="w-4 h-4 text-green-500" />
                        ) : (
                          <Pause className="w-4 h-4 text-gray-400" />
                        )}
                      </div>
                    </div>

                    <div className="flex items-center gap-2 mb-4">
                      <span className={clsx(
                        'px-2 py-1 text-xs font-medium rounded-full',
                        getCourseTypeInfo(course.course_type).color
                      )}>
                        {getCourseTypeInfo(course.course_type).label}
                      </span>
                      <span className={clsx(
                        'px-2 py-1 text-xs font-medium rounded-full',
                        getDifficultyInfo(course.difficulty_level).color
                      )}>
                        {getDifficultyInfo(course.difficulty_level).label}
                      </span>
                    </div>

                    <div className="grid grid-cols-2 gap-4 text-sm text-gray-600 mb-4">
                      <div>
                        <span className="font-medium">Duration:</span>
                        <span className="ml-1">{Math.floor(course.duration_minutes / 60)}h {course.duration_minutes % 60}m</span>
                      </div>
                      <div>
                        <span className="font-medium">Pass Rate:</span>
                        <span className="ml-1">{course.pass_threshold}%</span>
                      </div>
                    </div>

                    {course.enrollment_count && (
                      <div className="flex items-center justify-between text-sm text-gray-600">
                        <span>{course.enrollment_count} enrolled</span>
                        {course.completion_rate && (
                          <span>{course.completion_rate}% completion</span>
                        )}
                      </div>
                    )}

                    <div className="flex items-center gap-2 mt-4">
                      <button className="flex-1 bg-blue-600 hover:bg-blue-700 text-white px-3 py-2 rounded-lg text-sm font-medium transition-colors">
                        View Details
                      </button>
                      <button className="p-2 text-gray-400 hover:text-gray-600 transition-colors">
                        <Edit className="w-4 h-4" />
                      </button>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'enrollments' && (
          <div className="bg-white rounded-lg border shadow-sm">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-xl font-semibold text-gray-900">User Enrollments</h2>
              <p className="text-gray-600 mt-1">Track user progress and enrollment status</p>
            </div>

            <div className="divide-y divide-gray-200">
              {enrollments.map((enrollment) => (
                <div key={enrollment.enrollment_id} className="p-6 hover:bg-gray-50">
                  <div className="flex items-center justify-between">
                    <div className="flex-1">
                      <div className="flex items-center gap-3 mb-2">
                        <h3 className="font-medium text-gray-900">
                          {courses.find(c => c.course_id === enrollment.course_id)?.title || 'Unknown Course'}
                        </h3>
                        <span className={clsx(
                          'px-2 py-1 text-xs font-medium rounded-full',
                          getStatusColor(enrollment.status)
                        )}>
                          {enrollment.status.replace('_', ' ')}
                        </span>
                      </div>

                      <div className="flex items-center gap-4 text-sm text-gray-600">
                        <span>Enrolled: {format(new Date(enrollment.enrollment_date), 'MMM dd, yyyy')}</span>
                        {enrollment.completion_date && (
                          <span>Completed: {format(new Date(enrollment.completion_date), 'MMM dd, yyyy')}</span>
                        )}
                        {enrollment.last_accessed_at && (
                          <span>Last accessed: {formatDistanceToNow(new Date(enrollment.last_accessed_at), { addSuffix: true })}</span>
                        )}
                      </div>

                      <div className="mt-3">
                        <div className="flex items-center justify-between text-sm text-gray-600 mb-1">
                          <span>Progress</span>
                          <span>{enrollment.progress_percentage}%</span>
                        </div>
                        <div className="w-full bg-gray-200 rounded-full h-2">
                          <div
                            className="bg-blue-600 h-2 rounded-full transition-all duration-300"
                            style={{ width: `${enrollment.progress_percentage}%` }}
                          />
                        </div>
                      </div>

                      {enrollment.certificate_issued && (
                        <div className="mt-3 flex items-center gap-2">
                          <Award className="w-4 h-4 text-yellow-500" />
                          <span className="text-sm text-gray-600">Certificate issued</span>
                          <button className="text-blue-600 hover:text-blue-800 text-sm font-medium">
                            Download
                          </button>
                        </div>
                      )}
                    </div>

                    <div className="flex items-center gap-2 ml-4">
                      <button
                        onClick={() => {
                          setSelectedCourse(courses.find(c => c.course_id === enrollment.course_id));
                          setActiveTab('progress');
                        }}
                        className="p-2 text-gray-400 hover:text-gray-600 transition-colors"
                        title="View detailed progress"
                      >
                        <Eye className="w-4 h-4" />
                      </button>
                      <button className="p-2 text-gray-400 hover:text-gray-600 transition-colors">
                        <Edit className="w-4 h-4" />
                      </button>
                    </div>
                  </div>
                </div>
              ))}
            </div>
          </div>
        )}

        {activeTab === 'progress' && selectedCourse && (
          <TrainingProgress
            enrollmentId={enrollments.find(e => e.course_id === selectedCourse.course_id)?.enrollment_id || ''}
            courseId={selectedCourse.course_id}
            userId="current-user" // In production, this would come from authentication
            onModuleClick={(moduleId) => {
              console.log('Navigate to module:', moduleId);
              // Handle module navigation
            }}
            onQuizStart={(quizId) => {
              console.log('Start quiz:', quizId);
              // Handle quiz start
            }}
            onCompleteCourse={() => {
              console.log('Complete course and issue certificate');
              // Handle course completion
            }}
          />
        )}

        {activeTab === 'certifications' && (
          <CertificateManagement
            onCertificateView={(certificate) => {
              console.log('View certificate:', certificate);
              // Handle certificate viewing
            }}
            onCertificateDownload={(certificateId) => {
              console.log('Download certificate:', certificateId);
              // Handle certificate download
            }}
            onCertificateShare={(certificateId) => {
              console.log('Share certificate:', certificateId);
              // Handle certificate sharing
            }}
            onBulkOperation={(operation, certificates) => {
              console.log('Bulk operation:', operation, 'on', certificates.length, 'certificates');
              // Handle bulk operations
            }}
          />
        )}

        {activeTab === 'analytics' && (
          <TrainingAnalytics
            onExport={(format, data) => {
              console.log('Exporting analytics as', format, data);
              // Handle export functionality
            }}
            onDrillDown={(type, id) => {
              console.log('Drill down:', type, id);
              // Handle drill-down navigation
            }}
          />
        )}

        {activeTab === 'settings' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-6">Training Settings</h2>

              <div className="space-y-6">
                <div>
                  <h3 className="text-sm font-medium text-gray-900 mb-3">General Settings</h3>
                  <div className="space-y-3">
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Auto-enrollment for new users</span>
                      <input type="checkbox" className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Email notifications for deadlines</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Certificate auto-generation</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                  </div>
                </div>

                <div>
                  <h3 className="text-sm font-medium text-gray-900 mb-3">Compliance Settings</h3>
                  <div className="space-y-3">
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Mandatory annual refresh training</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="text-sm text-gray-700">Regulatory change notifications</span>
                      <input type="checkbox" defaultChecked className="rounded border-gray-300" />
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        )}
      </div>
    </div>
  );
};

export default TrainingManagement;
