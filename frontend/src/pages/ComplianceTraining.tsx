import React from 'react';
import { GraduationCap, Trophy, Clock, Award } from 'lucide-react';
import { useTrainingCourses, useTrainingLeaderboard } from '@/hooks/useTraining';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const ComplianceTraining: React.FC = () => {
  const { data: courses = [], isLoading: coursesLoading } = useTrainingCourses();
  const { data: leaderboard = [] } = useTrainingLeaderboard();

  if (coursesLoading) {
    return <LoadingSpinner fullScreen message="Loading training..." />;
  }

  const getDifficultyColor = (level: string) => {
    switch (level) {
      case 'beginner': return 'bg-green-100 text-green-800';
      case 'intermediate': return 'bg-yellow-100 text-yellow-800';
      case 'advanced': return 'bg-red-100 text-red-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">Compliance Training</h1>
        <p className="text-gray-600 mt-1">Gamified learning and staff development</p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        {/* Courses */}
        <div className="lg:col-span-2 space-y-6">
          <div className="bg-white rounded-lg shadow-sm border border-gray-200">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-lg font-semibold text-gray-900">Available Courses</h2>
            </div>
            <div className="divide-y divide-gray-200">
              {courses.length === 0 ? (
                <div className="p-12 text-center">
                  <GraduationCap className="w-12 h-12 text-gray-400 mx-auto mb-4" />
                  <h3 className="text-lg font-medium text-gray-900 mb-2">No courses available</h3>
                  <p className="text-gray-600">Training courses will appear here</p>
                </div>
              ) : (
                courses.map((course) => (
                  <div key={course.course_id} className="p-6 hover:bg-gray-50">
                    <div className="flex items-start justify-between">
                      <div className="flex-1">
                        <div className="flex items-center gap-3 mb-2">
                          <h3 className="text-lg font-semibold text-gray-900">{course.course_name}</h3>
                          {course.is_required && (
                            <span className="px-2 py-1 text-xs font-medium rounded-full bg-red-100 text-red-800">
                              Required
                            </span>
                          )}
                          <span className={`px-2 py-1 text-xs font-medium rounded-full ${getDifficultyColor(course.difficulty_level)}`}>
                            {course.difficulty_level}
                          </span>
                        </div>
                        <p className="text-sm text-gray-600 mb-3">{course.course_category}</p>
                        <div className="flex items-center gap-4 text-xs text-gray-500">
                          {course.estimated_duration_minutes && (
                            <div className="flex items-center gap-1">
                              <Clock className="w-4 h-4" />
                              <span>{course.estimated_duration_minutes} min</span>
                            </div>
                          )}
                          <div className="flex items-center gap-1">
                            <Award className="w-4 h-4" />
                            <span>{course.points_reward} points</span>
                          </div>
                          <span>Pass: {course.passing_score}%</span>
                        </div>
                      </div>
                      <button className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 text-sm font-medium">
                        Start Course
                      </button>
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>

        {/* Leaderboard */}
        <div className="lg:col-span-1">
          <div className="bg-white rounded-lg shadow-sm border border-gray-200">
            <div className="p-6 border-b border-gray-200">
              <div className="flex items-center gap-2">
                <Trophy className="w-5 h-5 text-yellow-600" />
                <h2 className="text-lg font-semibold text-gray-900">Leaderboard</h2>
              </div>
            </div>
            <div className="divide-y divide-gray-200 max-h-[600px] overflow-y-auto">
              {leaderboard.length === 0 ? (
                <div className="p-8 text-center">
                  <Trophy className="w-12 h-12 text-gray-400 mx-auto mb-4" />
                  <p className="text-gray-600 text-sm">No rankings yet</p>
                </div>
              ) : (
                leaderboard.map((entry) => (
                  <div key={entry.user_id} className="p-4 hover:bg-gray-50">
                    <div className="flex items-center gap-3">
                      <div className={`w-8 h-8 rounded-full flex items-center justify-center font-bold text-sm ${
                        entry.rank === 1 ? 'bg-yellow-100 text-yellow-800' :
                        entry.rank === 2 ? 'bg-gray-100 text-gray-800' :
                        entry.rank === 3 ? 'bg-orange-100 text-orange-800' :
                        'bg-blue-50 text-blue-800'
                      }`}>
                        #{entry.rank}
                      </div>
                      <div className="flex-1 min-w-0">
                        <p className="text-sm font-medium text-gray-900 truncate">{entry.user_id}</p>
                        <div className="flex items-center gap-3 text-xs text-gray-500 mt-1">
                          <span>{entry.total_points} pts</span>
                          <span>{entry.courses_completed} courses</span>
                        </div>
                      </div>
                      {entry.rank && entry.rank <= 3 && (
                        <Trophy className={`w-5 h-5 ${
                          entry.rank === 1 ? 'text-yellow-600' :
                          entry.rank === 2 ? 'text-gray-600' :
                          'text-orange-600'
                        }`} />
                      )}
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default ComplianceTraining;

