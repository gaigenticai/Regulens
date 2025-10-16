/**
 * Training Analytics Component
 * Comprehensive training performance analytics and insights dashboard
 * Production-grade analytics with interactive visualizations and reporting
 */

import React, { useState, useEffect, useMemo } from 'react';
import {
  BarChart3,
  TrendingUp,
  TrendingDown,
  Users,
  Award,
  Clock,
  Target,
  Calendar,
  Download,
  Filter,
  RefreshCw,
  Eye,
  BookOpen,
  GraduationCap,
  Star,
  Zap,
  AlertTriangle,
  CheckCircle,
  XCircle,
  Activity,
  PieChart,
  LineChart,
  MoreVertical,
  ChevronDown,
  ChevronUp,
  Search
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { format, subDays, subMonths, startOfMonth, endOfMonth } from 'date-fns';
import {
  useTrainingAnalytics,
  useCourseAnalytics,
  useUserAnalytics,
  useDepartmentAnalytics,
  TrainingAnalyticsData,
  CourseAnalytics,
  UserAnalytics,
  DepartmentAnalytics
} from '@/hooks/useTraining';

interface TrainingAnalyticsProps {
  className?: string;
  dateRange?: {
    start: Date;
    end: Date;
  };
  onExport?: (format: 'pdf' | 'csv' | 'xlsx', data: any) => void;
  onDrillDown?: (type: string, id: string) => void;
}

interface ChartData {
  labels: string[];
  datasets: {
    label: string;
    data: number[];
    backgroundColor?: string;
    borderColor?: string;
    borderWidth?: number;
  }[];
}

interface MetricCard {
  title: string;
  value: string | number;
  change?: number;
  changeLabel?: string;
  icon: React.ComponentType<{ className?: string }>;
  color: string;
  trend: 'up' | 'down' | 'neutral';
}

const TrainingAnalytics: React.FC<TrainingAnalyticsProps> = ({
  className = '',
  dateRange,
  onExport,
  onDrillDown
}) => {
  const [selectedPeriod, setSelectedPeriod] = useState<'7d' | '30d' | '90d' | '1y'>('30d');
  const [selectedDepartment, setSelectedDepartment] = useState<string>('all');
  const [selectedCourse, setSelectedCourse] = useState<string>('all');
  const [viewType, setViewType] = useState<'overview' | 'courses' | 'users' | 'departments'>('overview');
  const [showFilters, setShowFilters] = useState(false);

  // Mock analytics data (replace with real API calls)
  const [analyticsData] = useState<TrainingAnalyticsData>({
    overview: {
      total_users: 1250,
      active_users: 892,
      total_courses: 45,
      total_enrollments: 3456,
      completion_rate: 78.5,
      average_score: 84.2,
      certificates_issued: 1876,
      training_hours: 15420,
      engagement_score: 82.3,
      roi_percentage: 245.8
    },
    trends: {
      enrollments_over_time: [
        { date: '2024-01-01', count: 45 },
        { date: '2024-01-08', count: 52 },
        { date: '2024-01-15', count: 48 },
        { date: '2024-01-22', count: 61 },
        { date: '2024-01-29', count: 55 },
        { date: '2024-02-05', count: 67 },
        { date: '2024-02-12', count: 72 },
        { date: '2024-02-19', count: 68 },
        { date: '2024-02-26', count: 75 }
      ],
      completions_over_time: [
        { date: '2024-01-01', count: 32 },
        { date: '2024-01-08', count: 38 },
        { date: '2024-01-15', count: 35 },
        { date: '2024-01-22', count: 42 },
        { date: '2024-01-29', count: 39 },
        { date: '2024-02-05', count: 48 },
        { date: '2024-02-12', count: 52 },
        { date: '2024-02-19', count: 49 },
        { date: '2024-02-26', count: 58 }
      ],
      engagement_over_time: [
        { date: '2024-01-01', score: 78.5 },
        { date: '2024-01-08', score: 79.2 },
        { date: '2024-01-15', score: 81.1 },
        { date: '2024-01-22', score: 82.8 },
        { date: '2024-01-29', score: 81.9 },
        { date: '2024-02-05', score: 83.4 },
        { date: '2024-02-12', score: 84.1 },
        { date: '2024-02-19', score: 82.7 },
        { date: '2024-02-26', score: 82.3 }
      ]
    },
    courses: [
      {
        course_id: 'course-001',
        title: 'Anti-Money Laundering Fundamentals',
        enrollments: 245,
        completions: 198,
        completion_rate: 80.8,
        average_score: 86.4,
        average_time_spent: 145,
        drop_off_rate: 12.2,
        satisfaction_score: 4.2
      },
      {
        course_id: 'course-002',
        title: 'Data Protection and GDPR Compliance',
        enrollments: 189,
        completions: 167,
        completion_rate: 88.4,
        average_score: 89.1,
        average_time_spent: 180,
        drop_off_rate: 8.5,
        satisfaction_score: 4.4
      },
      {
        course_id: 'course-003',
        title: 'Cybersecurity Awareness',
        enrollments: 312,
        completions: 245,
        completion_rate: 78.5,
        average_score: 82.3,
        average_time_spent: 95,
        drop_off_rate: 15.7,
        satisfaction_score: 3.9
      }
    ],
    departments: [
      {
        department: 'Compliance',
        user_count: 145,
        enrollments: 523,
        completions: 445,
        completion_rate: 85.1,
        average_score: 87.2,
        training_hours: 2840,
        certificates_issued: 389
      },
      {
        department: 'IT Security',
        user_count: 98,
        enrollments: 387,
        completions: 312,
        completion_rate: 80.6,
        average_score: 84.8,
        training_hours: 2150,
        certificates_issued: 267
      },
      {
        department: 'Operations',
        user_count: 167,
        enrollments: 456,
        completions: 378,
        completion_rate: 82.9,
        average_score: 83.1,
        training_hours: 3120,
        certificates_issued: 334
      },
      {
        department: 'Finance',
        user_count: 89,
        enrollments: 298,
        completions: 267,
        completion_rate: 89.6,
        average_score: 88.9,
        training_hours: 1890,
        certificates_issued: 245
      }
    ],
    users: [
      {
        user_id: 'user-001',
        name: 'John Doe',
        department: 'Compliance',
        courses_completed: 12,
        total_score: 94.2,
        time_spent: 2340,
        certificates_earned: 8,
        last_activity: '2024-01-28T14:30:00Z',
        engagement_score: 92.1
      },
      {
        user_id: 'user-002',
        name: 'Jane Smith',
        department: 'IT Security',
        courses_completed: 15,
        total_score: 96.8,
        time_spent: 3120,
        certificates_earned: 12,
        last_activity: '2024-01-29T09:15:00Z',
        engagement_score: 94.3
      }
    ]
  });

  // API hooks (commented out until backend is fully implemented)
  // const { data: analytics, isLoading } = useTrainingAnalytics(selectedPeriod, selectedDepartment);
  // const { data: courseAnalytics } = useCourseAnalytics(selectedPeriod);
  // const { data: userAnalytics } = useUserAnalytics(selectedPeriod);
  // const { data: departmentAnalytics } = useDepartmentAnalytics(selectedPeriod);

  const isLoading = false; // Mock loading state

  const metricCards: MetricCard[] = useMemo(() => [
    {
      title: 'Total Users',
      value: analyticsData.overview.total_users.toLocaleString(),
      change: 12.5,
      changeLabel: 'vs last month',
      icon: Users,
      color: 'bg-blue-500',
      trend: 'up'
    },
    {
      title: 'Completion Rate',
      value: `${analyticsData.overview.completion_rate}%`,
      change: 5.2,
      changeLabel: 'vs last month',
      icon: Target,
      color: 'bg-green-500',
      trend: 'up'
    },
    {
      title: 'Training Hours',
      value: analyticsData.overview.training_hours.toLocaleString(),
      change: 8.1,
      changeLabel: 'vs last month',
      icon: Clock,
      color: 'bg-purple-500',
      trend: 'up'
    },
    {
      title: 'Certificates Issued',
      value: analyticsData.overview.certificates_issued.toLocaleString(),
      change: -2.3,
      changeLabel: 'vs last month',
      icon: Award,
      color: 'bg-yellow-500',
      trend: 'down'
    },
    {
      title: 'Average Score',
      value: `${analyticsData.overview.average_score}%`,
      change: 1.8,
      changeLabel: 'vs last month',
      icon: Star,
      color: 'bg-red-500',
      trend: 'up'
    },
    {
      title: 'ROI',
      value: `${analyticsData.overview.roi_percentage}%`,
      change: 15.4,
      changeLabel: 'vs last quarter',
      icon: TrendingUp,
      color: 'bg-indigo-500',
      trend: 'up'
    }
  ], [analyticsData]);

  const getPeriodDateRange = (period: string) => {
    const now = new Date();
    switch (period) {
      case '7d':
        return { start: subDays(now, 7), end: now };
      case '30d':
        return { start: subDays(now, 30), end: now };
      case '90d':
        return { start: subDays(now, 90), end: now };
      case '1y':
        return { start: subMonths(now, 12), end: now };
      default:
        return { start: subDays(now, 30), end: now };
    }
  };

  const handleExport = (format: 'pdf' | 'csv' | 'xlsx') => {
    if (onExport) {
      onExport(format, analyticsData);
    }
  };

  const formatTime = (minutes: number): string => {
    const hours = Math.floor(minutes / 60);
    const mins = minutes % 60;
    return hours > 0 ? `${hours}h ${mins}m` : `${mins}m`;
  };

  const getTrendIcon = (trend: string) => {
    switch (trend) {
      case 'up':
        return <TrendingUp className="w-4 h-4 text-green-500" />;
      case 'down':
        return <TrendingDown className="w-4 h-4 text-red-500" />;
      default:
        return <Activity className="w-4 h-4 text-gray-500" />;
    }
  };

  if (isLoading) {
    return (
      <div className="flex items-center justify-center min-h-96">
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
            <BarChart3 className="w-8 h-8 text-blue-600" />
            Training Analytics
          </h1>
          <p className="text-gray-600 mt-1">
            Comprehensive insights into training performance and user engagement
          </p>
        </div>

        <div className="flex items-center gap-3">
          <button
            onClick={() => setShowFilters(!showFilters)}
            className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
          >
            <Filter className="w-4 h-4" />
            Filters
            {showFilters ? <ChevronUp className="w-4 h-4" /> : <ChevronDown className="w-4 h-4" />}
          </button>

          <div className="flex items-center gap-2">
            <button
              onClick={() => handleExport('pdf')}
              className="flex items-center gap-2 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
            >
              <Download className="w-4 h-4" />
              Export Report
            </button>
            <div className="relative">
              <button className="p-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors">
                <MoreVertical className="w-4 h-4" />
              </button>
            </div>
          </div>
        </div>
      </div>

      {/* Filters Panel */}
      {showFilters && (
        <div className="bg-white rounded-lg border shadow-sm p-6">
          <div className="grid grid-cols-1 md:grid-cols-4 gap-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2">
                Time Period
              </label>
              <select
                value={selectedPeriod}
                onChange={(e) => setSelectedPeriod(e.target.value as any)}
                className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
              >
                <option value="7d">Last 7 days</option>
                <option value="30d">Last 30 days</option>
                <option value="90d">Last 90 days</option>
                <option value="1y">Last year</option>
              </select>
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2">
                Department
              </label>
              <select
                value={selectedDepartment}
                onChange={(e) => setSelectedDepartment(e.target.value)}
                className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
              >
                <option value="all">All Departments</option>
                <option value="compliance">Compliance</option>
                <option value="it-security">IT Security</option>
                <option value="operations">Operations</option>
                <option value="finance">Finance</option>
              </select>
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2">
                Course
              </label>
              <select
                value={selectedCourse}
                onChange={(e) => setSelectedCourse(e.target.value)}
                className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
              >
                <option value="all">All Courses</option>
                {analyticsData.courses.map(course => (
                  <option key={course.course_id} value={course.course_id}>
                    {course.title}
                  </option>
                ))}
              </select>
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 mb-2">
                View Type
              </label>
              <select
                value={viewType}
                onChange={(e) => setViewType(e.target.value as any)}
                className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
              >
                <option value="overview">Overview</option>
                <option value="courses">Courses</option>
                <option value="users">Users</option>
                <option value="departments">Departments</option>
              </select>
            </div>
          </div>
        </div>
      )}

      {/* Metric Cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-6">
        {metricCards.map((metric, index) => (
          <div key={index} className="bg-white rounded-lg border shadow-sm p-6">
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className={clsx('p-2 rounded-lg', metric.color)}>
                  <metric.icon className="w-5 h-5 text-white" />
                </div>
                <div>
                  <p className="text-sm font-medium text-gray-600">{metric.title}</p>
                  <p className="text-2xl font-bold text-gray-900">{metric.value}</p>
                </div>
              </div>
              {metric.change && (
                <div className="flex items-center gap-1">
                  {getTrendIcon(metric.trend)}
                  <span className={clsx(
                    'text-sm font-medium',
                    metric.trend === 'up' ? 'text-green-600' :
                    metric.trend === 'down' ? 'text-red-600' : 'text-gray-600'
                  )}>
                    {metric.change > 0 ? '+' : ''}{metric.change}%
                  </span>
                </div>
              )}
            </div>
            {metric.changeLabel && (
              <p className="text-xs text-gray-500 mt-2">{metric.changeLabel}</p>
            )}
          </div>
        ))}
      </div>

      {/* Main Content */}
      <div className="space-y-6">
        {viewType === 'overview' && (
          <div className="space-y-6">
            {/* Trends Chart */}
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <div className="flex items-center justify-between mb-6">
                <h2 className="text-xl font-semibold text-gray-900">Training Trends</h2>
                <div className="flex items-center gap-2 text-sm text-gray-600">
                  <LineChart className="w-4 h-4" />
                  <span>Last {selectedPeriod.replace('d', ' days').replace('1y', ' year')}</span>
                </div>
              </div>

              <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
                {/* Enrollment Trend */}
                <div>
                  <h3 className="text-lg font-medium text-gray-900 mb-4">Enrollments</h3>
                  <div className="space-y-3">
                    {analyticsData.trends.enrollments_over_time.slice(-5).map((point, index) => (
                      <div key={index} className="flex items-center justify-between">
                        <span className="text-sm text-gray-600">
                          {format(new Date(point.date), 'MMM dd')}
                        </span>
                        <div className="flex items-center gap-2">
                          <div className="w-20 bg-gray-200 rounded-full h-2">
                            <div
                              className="bg-blue-600 h-2 rounded-full"
                              style={{
                                width: `${(point.count / Math.max(...analyticsData.trends.enrollments_over_time.map(p => p.count))) * 100}%`
                              }}
                            />
                          </div>
                          <span className="text-sm font-medium text-gray-900 w-8">{point.count}</span>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>

                {/* Completion Trend */}
                <div>
                  <h3 className="text-lg font-medium text-gray-900 mb-4">Completions</h3>
                  <div className="space-y-3">
                    {analyticsData.trends.completions_over_time.slice(-5).map((point, index) => (
                      <div key={index} className="flex items-center justify-between">
                        <span className="text-sm text-gray-600">
                          {format(new Date(point.date), 'MMM dd')}
                        </span>
                        <div className="flex items-center gap-2">
                          <div className="w-20 bg-gray-200 rounded-full h-2">
                            <div
                              className="bg-green-600 h-2 rounded-full"
                              style={{
                                width: `${(point.count / Math.max(...analyticsData.trends.completions_over_time.map(p => p.count))) * 100}%`
                              }}
                            />
                          </div>
                          <span className="text-sm font-medium text-gray-900 w-8">{point.count}</span>
                        </div>
                      </div>
                    ))}
                  </div>
                </div>
              </div>
            </div>

            {/* Department Performance */}
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-6">Department Performance</h2>
              <div className="space-y-4">
                {analyticsData.departments.map((dept, index) => (
                  <div key={index} className="flex items-center justify-between p-4 border border-gray-200 rounded-lg">
                    <div className="flex-1">
                      <h3 className="font-medium text-gray-900">{dept.department}</h3>
                      <div className="grid grid-cols-4 gap-4 mt-2 text-sm text-gray-600">
                        <div>
                          <span className="font-medium">{dept.completion_rate}%</span>
                          <span className="ml-1">completion</span>
                        </div>
                        <div>
                          <span className="font-medium">{dept.average_score}%</span>
                          <span className="ml-1">avg score</span>
                        </div>
                        <div>
                          <span className="font-medium">{dept.certificates_issued}</span>
                          <span className="ml-1">certificates</span>
                        </div>
                        <div>
                          <span className="font-medium">{formatTime(dept.training_hours)}</span>
                          <span className="ml-1">training time</span>
                        </div>
                      </div>
                    </div>
                    <button
                      onClick={() => onDrillDown?.('department', dept.department)}
                      className="ml-4 px-3 py-1 text-blue-600 hover:bg-blue-50 rounded transition-colors"
                    >
                      View Details
                    </button>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {viewType === 'courses' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm p-6">
              <h2 className="text-xl font-semibold text-gray-900 mb-6">Course Performance</h2>
              <div className="space-y-4">
                {analyticsData.courses.map((course, index) => (
                  <div key={index} className="border border-gray-200 rounded-lg p-6">
                    <div className="flex items-center justify-between mb-4">
                      <h3 className="text-lg font-medium text-gray-900">{course.title}</h3>
                      <div className="flex items-center gap-2">
                        <span className={clsx(
                          'px-2 py-1 text-xs font-medium rounded',
                          course.completion_rate >= 85 ? 'bg-green-100 text-green-800' :
                          course.completion_rate >= 70 ? 'bg-yellow-100 text-yellow-800' :
                          'bg-red-100 text-red-800'
                        )}>
                          {course.completion_rate}% completion
                        </span>
                      </div>
                    </div>

                    <div className="grid grid-cols-2 md:grid-cols-4 gap-4 mb-4">
                      <div className="text-center">
                        <div className="text-lg font-semibold text-gray-900">{course.enrollments}</div>
                        <div className="text-sm text-gray-600">Enrollments</div>
                      </div>
                      <div className="text-center">
                        <div className="text-lg font-semibold text-gray-900">{course.completions}</div>
                        <div className="text-sm text-gray-600">Completions</div>
                      </div>
                      <div className="text-center">
                        <div className="text-lg font-semibold text-gray-900">{course.average_score}%</div>
                        <div className="text-sm text-gray-600">Avg Score</div>
                      </div>
                      <div className="text-center">
                        <div className="text-lg font-semibold text-gray-900">{formatTime(course.average_time_spent)}</div>
                        <div className="text-sm text-gray-600">Avg Time</div>
                      </div>
                    </div>

                    <div className="flex items-center justify-between">
                      <div className="flex items-center gap-4 text-sm text-gray-600">
                        <span>Drop-off rate: {course.drop_off_rate}%</span>
                        <span>Satisfaction: {course.satisfaction_score}/5</span>
                      </div>
                      <button
                        onClick={() => onDrillDown?.('course', course.course_id)}
                        className="px-3 py-1 text-blue-600 hover:bg-blue-50 rounded transition-colors"
                      >
                        View Details
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {viewType === 'users' && (
          <div className="space-y-6">
            <div className="bg-white rounded-lg border shadow-sm overflow-hidden">
              <div className="p-6 border-b border-gray-200">
                <h2 className="text-xl font-semibold text-gray-900">User Performance</h2>
                <p className="text-gray-600 mt-1">Individual user training analytics</p>
              </div>

              <div className="divide-y divide-gray-200">
                {analyticsData.users.map((user, index) => (
                  <div key={index} className="p-6 hover:bg-gray-50">
                    <div className="flex items-center justify-between">
                      <div className="flex-1">
                        <div className="flex items-center gap-3 mb-2">
                          <h3 className="font-medium text-gray-900">{user.name}</h3>
                          <span className="px-2 py-1 text-xs font-medium bg-blue-100 text-blue-800 rounded">
                            {user.department}
                          </span>
                        </div>

                        <div className="grid grid-cols-2 md:grid-cols-4 gap-4 text-sm text-gray-600">
                          <div>
                            <span className="font-medium">{user.courses_completed}</span>
                            <span className="ml-1">courses completed</span>
                          </div>
                          <div>
                            <span className="font-medium">{user.total_score}%</span>
                            <span className="ml-1">average score</span>
                          </div>
                          <div>
                            <span className="font-medium">{formatTime(user.time_spent)}</span>
                            <span className="ml-1">time spent</span>
                          </div>
                          <div>
                            <span className="font-medium">{user.certificates_earned}</span>
                            <span className="ml-1">certificates</span>
                          </div>
                        </div>

                        <div className="flex items-center gap-4 mt-2 text-xs text-gray-500">
                          <span>Last activity: {format(new Date(user.last_activity), 'MMM dd, yyyy')}</span>
                          <span>Engagement: {user.engagement_score}%</span>
                        </div>
                      </div>

                      <button
                        onClick={() => onDrillDown?.('user', user.user_id)}
                        className="ml-4 px-3 py-1 text-blue-600 hover:bg-blue-50 rounded transition-colors"
                      >
                        View Profile
                      </button>
                    </div>
                  </div>
                ))}
              </div>
            </div>
          </div>
        )}

        {viewType === 'departments' && (
          <div className="space-y-6">
            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
              {analyticsData.departments.map((dept, index) => (
                <div key={index} className="bg-white rounded-lg border shadow-sm p-6">
                  <h3 className="text-lg font-semibold text-gray-900 mb-4">{dept.department}</h3>

                  <div className="space-y-4">
                    <div className="flex justify-between items-center">
                      <span className="text-sm text-gray-600">Users</span>
                      <span className="font-medium">{dept.user_count}</span>
                    </div>

                    <div className="flex justify-between items-center">
                      <span className="text-sm text-gray-600">Completion Rate</span>
                      <span className="font-medium">{dept.completion_rate}%</span>
                    </div>

                    <div className="flex justify-between items-center">
                      <span className="text-sm text-gray-600">Average Score</span>
                      <span className="font-medium">{dept.average_score}%</span>
                    </div>

                    <div className="flex justify-between items-center">
                      <span className="text-sm text-gray-600">Training Hours</span>
                      <span className="font-medium">{formatTime(dept.training_hours)}</span>
                    </div>

                    <div className="flex justify-between items-center">
                      <span className="text-sm text-gray-600">Certificates Issued</span>
                      <span className="font-medium">{dept.certificates_issued}</span>
                    </div>
                  </div>

                  <button
                    onClick={() => onDrillDown?.('department', dept.department)}
                    className="w-full mt-4 px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
                  >
                    View Department Details
                  </button>
                </div>
              ))}
            </div>
          </div>
        )}
      </div>

      {/* Export Options */}
      <div className="bg-white rounded-lg border shadow-sm p-6">
        <h2 className="text-xl font-semibold text-gray-900 mb-4">Export Analytics</h2>
        <div className="flex items-center gap-4">
          <button
            onClick={() => handleExport('pdf')}
            className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
          >
            <FileText className="w-4 h-4" />
            Export as PDF
          </button>
          <button
            onClick={() => handleExport('csv')}
            className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
          >
            <Download className="w-4 h-4" />
            Export as CSV
          </button>
          <button
            onClick={() => handleExport('xlsx')}
            className="flex items-center gap-2 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50 transition-colors"
          >
            <Download className="w-4 h-4" />
            Export as Excel
          </button>
        </div>
        <p className="text-sm text-gray-600 mt-2">
          Export comprehensive training analytics reports for further analysis or compliance reporting.
        </p>
      </div>
    </div>
  );
};

export default TrainingAnalytics;
