/**
 * Course Creation Component
 * Comprehensive form for creating new training courses
 * Production-grade course creation with validation and module management
 */

import React, { useState, useEffect } from 'react';
import {
  Plus,
  X,
  Save,
  BookOpen,
  Clock,
  Target,
  Tag,
  AlertTriangle,
  CheckCircle,
  FileText,
  Play,
  Users,
  Award,
  ChevronDown,
  ChevronUp,
  Trash2,
  Edit3,
  Copy
} from 'lucide-react';
import { clsx } from 'clsx';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { useCreateCourse, useCourses, TrainingModule, CreateCourseRequest } from '@/hooks/useTraining';

interface CourseCreationProps {
  onCourseCreated?: (course: any) => void;
  onCancel?: () => void;
  className?: string;
  initialData?: Partial<CreateCourseRequest>;
}

interface ModuleFormData {
  id: string;
  title: string;
  description: string;
  content_type: 'video' | 'text' | 'interactive' | 'quiz' | 'document';
  content_url?: string;
  content_text?: string;
  duration_minutes: number;
  order_index: number;
  is_required: boolean;
}

const COURSE_TYPES = [
  { value: 'compliance', label: 'Compliance Training', description: 'Regulatory compliance and AML training' },
  { value: 'technical', label: 'Technical Skills', description: 'Technical skills and system training' },
  { value: 'regulatory', label: 'Regulatory Knowledge', description: 'Regulatory frameworks and requirements' },
  { value: 'security', label: 'Security Awareness', description: 'Information security and cybersecurity' },
  { value: 'ethics', label: 'Ethics & Conduct', description: 'Ethical standards and professional conduct' },
  { value: 'general', label: 'General Training', description: 'General business and professional development' }
];

const DIFFICULTY_LEVELS = [
  { value: 'beginner', label: 'Beginner', description: 'No prior knowledge required', color: 'bg-green-100 text-green-800' },
  { value: 'intermediate', label: 'Intermediate', description: 'Some prior knowledge recommended', color: 'bg-yellow-100 text-yellow-800' },
  { value: 'advanced', label: 'Advanced', description: 'Strong background knowledge required', color: 'bg-orange-100 text-orange-800' },
  { value: 'expert', label: 'Expert', description: 'Deep expertise and experience required', color: 'bg-red-100 text-red-800' }
];

const CONTENT_TYPES = [
  { value: 'video', label: 'Video', description: 'Video lecture or presentation', icon: Play },
  { value: 'text', label: 'Text Content', description: 'Reading material and articles', icon: FileText },
  { value: 'interactive', label: 'Interactive', description: 'Interactive exercises and simulations', icon: Users },
  { value: 'quiz', label: 'Quiz', description: 'Knowledge assessment', icon: Award },
  { value: 'document', label: 'Document', description: 'PDFs and downloadable resources', icon: BookOpen }
];

const CourseCreation: React.FC<CourseCreationProps> = ({
  onCourseCreated,
  onCancel,
  className = '',
  initialData
}) => {
  const [formData, setFormData] = useState<CreateCourseRequest>({
    title: initialData?.title || '',
    description: initialData?.description || '',
    course_type: initialData?.course_type || 'compliance',
    difficulty_level: initialData?.difficulty_level || 'beginner',
    duration_minutes: initialData?.duration_minutes || 60,
    pass_threshold: initialData?.pass_threshold || 80,
    tags: initialData?.tags || [],
    modules: initialData?.modules || [],
    prerequisites: initialData?.prerequisites || []
  });

  const [modules, setModules] = useState<ModuleFormData[]>(
    initialData?.modules?.map((module, index) => ({
      id: `module-${Date.now()}-${index}`,
      title: module.title || '',
      description: module.description || '',
      content_type: module.content_type || 'text',
      content_url: module.content_url || '',
      content_text: module.content_text || '',
      duration_minutes: module.duration_minutes || 15,
      order_index: module.order_index || index,
      is_required: module.is_required ?? true
    })) || []
  );

  const [newTag, setNewTag] = useState('');
  const [newPrerequisite, setNewPrerequisite] = useState('');
  const [showAdvanced, setShowAdvanced] = useState(false);
  const [currentStep, setCurrentStep] = useState<'basic' | 'modules' | 'review'>('basic');

  const createCourseMutation = useCreateCourse();
  const { data: existingCourses = [] } = useCourses();

  // Calculate total duration from modules
  const calculatedDuration = modules.reduce((total, module) => total + module.duration_minutes, 0);

  // Validation
  const [errors, setErrors] = useState<Record<string, string>>({});

  const validateForm = () => {
    const newErrors: Record<string, string> = {};

    if (!formData.title.trim()) {
      newErrors.title = 'Course title is required';
    } else if (formData.title.length < 5) {
      newErrors.title = 'Course title must be at least 5 characters';
    }

    if (!formData.description.trim()) {
      newErrors.description = 'Course description is required';
    } else if (formData.description.length < 20) {
      newErrors.description = 'Course description must be at least 20 characters';
    }

    if (formData.duration_minutes < 15) {
      newErrors.duration_minutes = 'Course duration must be at least 15 minutes';
    }

    if (formData.pass_threshold < 0 || formData.pass_threshold > 100) {
      newErrors.pass_threshold = 'Pass threshold must be between 0 and 100';
    }

    // Check for duplicate module titles
    const moduleTitles = modules.map(m => m.title.toLowerCase());
    const duplicateTitles = moduleTitles.filter((title, index) => moduleTitles.indexOf(title) !== index);
    if (duplicateTitles.length > 0) {
      newErrors.modules = 'Module titles must be unique';
    }

    // Validate modules
    modules.forEach((module, index) => {
      if (!module.title.trim()) {
        newErrors[`module_${index}_title`] = 'Module title is required';
      }
      if (!module.description.trim()) {
        newErrors[`module_${index}_description`] = 'Module description is required';
      }
      if (module.duration_minutes < 1) {
        newErrors[`module_${index}_duration`] = 'Module duration must be at least 1 minute';
      }
    });

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSubmit = async () => {
    if (!validateForm()) {
      setCurrentStep('basic');
      return;
    }

    try {
      const courseData: CreateCourseRequest = {
        ...formData,
        modules: modules.map((module, index) => ({
          title: module.title,
          description: module.description,
          content_type: module.content_type,
          content_url: module.content_url,
          content_text: module.content_text,
          duration_minutes: module.duration_minutes,
          order_index: index,
          is_required: module.is_required
        })),
        duration_minutes: Math.max(formData.duration_minutes, calculatedDuration)
      };

      const result = await createCourseMutation.mutateAsync(courseData);

      if (onCourseCreated) {
        onCourseCreated(result);
      }
    } catch (error) {
      console.error('Failed to create course:', error);
    }
  };

  const addModule = () => {
    const newModule: ModuleFormData = {
      id: `module-${Date.now()}`,
      title: '',
      description: '',
      content_type: 'text',
      duration_minutes: 15,
      order_index: modules.length,
      is_required: true
    };
    setModules([...modules, newModule]);
  };

  const updateModule = (id: string, updates: Partial<ModuleFormData>) => {
    setModules(modules.map(module =>
      module.id === id ? { ...module, ...updates } : module
    ));
  };

  const removeModule = (id: string) => {
    setModules(modules.filter(module => module.id !== id).map((module, index) => ({
      ...module,
      order_index: index
    })));
  };

  const moveModule = (index: number, direction: 'up' | 'down') => {
    if (
      (direction === 'up' && index === 0) ||
      (direction === 'down' && index === modules.length - 1)
    ) {
      return;
    }

    const newIndex = direction === 'up' ? index - 1 : index + 1;
    const newModules = [...modules];
    [newModules[index], newModules[newIndex]] = [newModules[newIndex], newModules[index]];

    // Update order indices
    newModules.forEach((module, idx) => {
      module.order_index = idx;
    });

    setModules(newModules);
  };

  const addTag = () => {
    if (newTag.trim() && !formData.tags.includes(newTag.trim())) {
      setFormData({
        ...formData,
        tags: [...formData.tags, newTag.trim()]
      });
      setNewTag('');
    }
  };

  const removeTag = (tagToRemove: string) => {
    setFormData({
      ...formData,
      tags: formData.tags.filter(tag => tag !== tagToRemove)
    });
  };

  const addPrerequisite = () => {
    if (newPrerequisite.trim() && !formData.prerequisites.includes(newPrerequisite.trim())) {
      setFormData({
        ...formData,
        prerequisites: [...formData.prerequisites, newPrerequisite.trim()]
      });
      setNewPrerequisite('');
    }
  };

  const removePrerequisite = (prereqToRemove: string) => {
    setFormData({
      ...formData,
      prerequisites: formData.prerequisites.filter(prereq => prereq !== prereqToRemove)
    });
  };

  const getStepProgress = () => {
    const steps = ['basic', 'modules', 'review'];
    const currentIndex = steps.indexOf(currentStep);
    return ((currentIndex + 1) / steps.length) * 100;
  };

  if (createCourseMutation.isSuccess) {
    return (
      <div className="max-w-2xl mx-auto p-6 bg-white rounded-lg border shadow-sm">
        <div className="text-center">
          <CheckCircle className="w-16 h-16 text-green-500 mx-auto mb-4" />
          <h2 className="text-2xl font-bold text-gray-900 mb-2">Course Created Successfully!</h2>
          <p className="text-gray-600 mb-6">
            Your training course "{formData.title}" has been created and is ready for enrollment.
          </p>
          <div className="flex gap-3 justify-center">
            <button
              onClick={() => window.location.reload()}
              className="px-4 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
            >
              Create Another Course
            </button>
            {onCancel && (
              <button
                onClick={onCancel}
                className="px-4 py-2 bg-gray-600 hover:bg-gray-700 text-white rounded-lg transition-colors"
              >
                Back to Courses
              </button>
            )}
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className={clsx('max-w-4xl mx-auto', className)}>
      {/* Header */}
      <div className="bg-white rounded-lg border shadow-sm p-6 mb-6">
        <div className="flex items-center justify-between mb-4">
          <div>
            <h1 className="text-2xl font-bold text-gray-900">Create Training Course</h1>
            <p className="text-gray-600 mt-1">Build comprehensive training content for your team</p>
          </div>
          {onCancel && (
            <button
              onClick={onCancel}
              className="p-2 text-gray-400 hover:text-gray-600 transition-colors"
            >
              <X className="w-5 h-5" />
            </button>
          )}
        </div>

        {/* Progress Indicator */}
        <div className="mb-6">
          <div className="flex items-center justify-between mb-2">
            <span className="text-sm font-medium text-gray-700">Course Creation Progress</span>
            <span className="text-sm text-gray-500">{Math.round(getStepProgress())}% complete</span>
          </div>
          <div className="w-full bg-gray-200 rounded-full h-2">
            <div
              className="bg-blue-600 h-2 rounded-full transition-all duration-300"
              style={{ width: `${getStepProgress()}%` }}
            />
          </div>
          <div className="flex justify-between mt-2">
            {['Basic Info', 'Modules', 'Review'].map((step, index) => (
              <button
                key={step}
                onClick={() => setCurrentStep(['basic', 'modules', 'review'][index] as any)}
                className={clsx(
                  'text-sm font-medium px-2 py-1 rounded',
                  currentStep === ['basic', 'modules', 'review'][index]
                    ? 'bg-blue-100 text-blue-800'
                    : 'text-gray-500 hover:text-gray-700'
                )}
              >
                {step}
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Step Content */}
      <div className="bg-white rounded-lg border shadow-sm">
        {currentStep === 'basic' && (
          <div className="p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-6">Basic Course Information</h2>

            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              {/* Title */}
              <div className="md:col-span-2">
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Course Title *
                </label>
                <input
                  type="text"
                  value={formData.title}
                  onChange={(e) => setFormData({ ...formData, title: e.target.value })}
                  className={clsx(
                    'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                    errors.title ? 'border-red-300' : 'border-gray-300'
                  )}
                  placeholder="Enter course title"
                />
                {errors.title && (
                  <p className="mt-1 text-sm text-red-600 flex items-center gap-1">
                    <AlertTriangle className="w-4 h-4" />
                    {errors.title}
                  </p>
                )}
              </div>

              {/* Description */}
              <div className="md:col-span-2">
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Description *
                </label>
                <textarea
                  value={formData.description}
                  onChange={(e) => setFormData({ ...formData, description: e.target.value })}
                  rows={4}
                  className={clsx(
                    'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                    errors.description ? 'border-red-300' : 'border-gray-300'
                  )}
                  placeholder="Describe the course content and learning objectives"
                />
                {errors.description && (
                  <p className="mt-1 text-sm text-red-600 flex items-center gap-1">
                    <AlertTriangle className="w-4 h-4" />
                    {errors.description}
                  </p>
                )}
              </div>

              {/* Course Type */}
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Course Type
                </label>
                <select
                  value={formData.course_type}
                  onChange={(e) => setFormData({ ...formData, course_type: e.target.value as any })}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                >
                  {COURSE_TYPES.map(type => (
                    <option key={type.value} value={type.value}>
                      {type.label}
                    </option>
                  ))}
                </select>
                <p className="mt-1 text-xs text-gray-500">
                  {COURSE_TYPES.find(t => t.value === formData.course_type)?.description}
                </p>
              </div>

              {/* Difficulty Level */}
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Difficulty Level
                </label>
                <select
                  value={formData.difficulty_level}
                  onChange={(e) => setFormData({ ...formData, difficulty_level: e.target.value as any })}
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                >
                  {DIFFICULTY_LEVELS.map(level => (
                    <option key={level.value} value={level.value}>
                      {level.label}
                    </option>
                  ))}
                </select>
                <p className="mt-1 text-xs text-gray-500">
                  {DIFFICULTY_LEVELS.find(l => l.value === formData.difficulty_level)?.description}
                </p>
              </div>

              {/* Duration */}
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Estimated Duration (minutes) *
                </label>
                <input
                  type="number"
                  value={formData.duration_minutes}
                  onChange={(e) => setFormData({ ...formData, duration_minutes: parseInt(e.target.value) || 0 })}
                  min="15"
                  className={clsx(
                    'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                    errors.duration_minutes ? 'border-red-300' : 'border-gray-300'
                  )}
                />
                {errors.duration_minutes && (
                  <p className="mt-1 text-sm text-red-600 flex items-center gap-1">
                    <AlertTriangle className="w-4 h-4" />
                    {errors.duration_minutes}
                  </p>
                )}
                {calculatedDuration > 0 && calculatedDuration !== formData.duration_minutes && (
                  <p className="mt-1 text-xs text-blue-600">
                    Modules total: {calculatedDuration} minutes
                  </p>
                )}
              </div>

              {/* Pass Threshold */}
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Pass Threshold (%) *
                </label>
                <input
                  type="number"
                  value={formData.pass_threshold}
                  onChange={(e) => setFormData({ ...formData, pass_threshold: parseInt(e.target.value) || 0 })}
                  min="0"
                  max="100"
                  className={clsx(
                    'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                    errors.pass_threshold ? 'border-red-300' : 'border-gray-300'
                  )}
                />
                {errors.pass_threshold && (
                  <p className="mt-1 text-sm text-red-600 flex items-center gap-1">
                    <AlertTriangle className="w-4 h-4" />
                    {errors.pass_threshold}
                  </p>
                )}
              </div>

              {/* Tags */}
              <div className="md:col-span-2">
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Tags
                </label>
                <div className="flex gap-2 mb-2">
                  <input
                    type="text"
                    value={newTag}
                    onChange={(e) => setNewTag(e.target.value)}
                    onKeyPress={(e) => e.key === 'Enter' && (e.preventDefault(), addTag())}
                    className="flex-1 px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    placeholder="Add a tag"
                  />
                  <button
                    onClick={addTag}
                    disabled={!newTag.trim()}
                    className="px-3 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white rounded-lg transition-colors"
                  >
                    <Plus className="w-4 h-4" />
                  </button>
                </div>
                <div className="flex flex-wrap gap-2">
                  {formData.tags.map(tag => (
                    <span
                      key={tag}
                      className="inline-flex items-center gap-1 px-2 py-1 bg-blue-100 text-blue-800 text-sm rounded-full"
                    >
                      <Tag className="w-3 h-3" />
                      {tag}
                      <button
                        onClick={() => removeTag(tag)}
                        className="ml-1 text-blue-600 hover:text-blue-800"
                      >
                        <X className="w-3 h-3" />
                      </button>
                    </span>
                  ))}
                </div>
              </div>

              {/* Prerequisites */}
              <div className="md:col-span-2">
                <label className="block text-sm font-medium text-gray-700 mb-2">
                  Prerequisites
                </label>
                <div className="flex gap-2 mb-2">
                  <input
                    type="text"
                    value={newPrerequisite}
                    onChange={(e) => setNewPrerequisite(e.target.value)}
                    onKeyPress={(e) => e.key === 'Enter' && (e.preventDefault(), addPrerequisite())}
                    className="flex-1 px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                    placeholder="Add a prerequisite course"
                  />
                  <button
                    onClick={addPrerequisite}
                    disabled={!newPrerequisite.trim()}
                    className="px-3 py-2 bg-blue-600 hover:bg-blue-700 disabled:bg-gray-400 text-white rounded-lg transition-colors"
                  >
                    <Plus className="w-4 h-4" />
                  </button>
                </div>
                <div className="space-y-2">
                  {formData.prerequisites.map(prereq => (
                    <div
                      key={prereq}
                      className="flex items-center justify-between p-2 bg-gray-50 rounded-lg"
                    >
                      <span className="text-sm text-gray-700">{prereq}</span>
                      <button
                        onClick={() => removePrerequisite(prereq)}
                        className="text-gray-400 hover:text-gray-600"
                      >
                        <X className="w-4 h-4" />
                      </button>
                    </div>
                  ))}
                </div>
              </div>
            </div>

            <div className="mt-6 flex justify-end">
              <button
                onClick={() => setCurrentStep('modules')}
                className="px-6 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
              >
                Next: Add Modules
              </button>
            </div>
          </div>
        )}

        {currentStep === 'modules' && (
          <div className="p-6">
            <div className="flex items-center justify-between mb-6">
              <h2 className="text-xl font-semibold text-gray-900">Course Modules</h2>
              <button
                onClick={addModule}
                className="flex items-center gap-2 px-4 py-2 bg-green-600 hover:bg-green-700 text-white rounded-lg transition-colors"
              >
                <Plus className="w-4 h-4" />
                Add Module
              </button>
            </div>

            {errors.modules && (
              <div className="mb-4 p-3 bg-red-50 border border-red-200 rounded-lg">
                <p className="text-sm text-red-600 flex items-center gap-1">
                  <AlertTriangle className="w-4 h-4" />
                  {errors.modules}
                </p>
              </div>
            )}

            <div className="space-y-4">
              {modules.map((module, index) => (
                <div key={module.id} className="border border-gray-200 rounded-lg p-4">
                  <div className="flex items-center justify-between mb-4">
                    <h3 className="font-medium text-gray-900">Module {index + 1}</h3>
                    <div className="flex items-center gap-2">
                      <button
                        onClick={() => moveModule(index, 'up')}
                        disabled={index === 0}
                        className="p-1 text-gray-400 hover:text-gray-600 disabled:opacity-50"
                      >
                        <ChevronUp className="w-4 h-4" />
                      </button>
                      <button
                        onClick={() => moveModule(index, 'down')}
                        disabled={index === modules.length - 1}
                        className="p-1 text-gray-400 hover:text-gray-600 disabled:opacity-50"
                      >
                        <ChevronDown className="w-4 h-4" />
                      </button>
                      <button
                        onClick={() => removeModule(module.id)}
                        className="p-1 text-red-500 hover:text-red-700"
                      >
                        <Trash2 className="w-4 h-4" />
                      </button>
                    </div>
                  </div>

                  <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
                    <div>
                      <label className="block text-sm font-medium text-gray-700 mb-1">
                        Module Title *
                      </label>
                      <input
                        type="text"
                        value={module.title}
                        onChange={(e) => updateModule(module.id, { title: e.target.value })}
                        className={clsx(
                          'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                          errors[`module_${index}_title`] ? 'border-red-300' : 'border-gray-300'
                        )}
                        placeholder="Enter module title"
                      />
                      {errors[`module_${index}_title`] && (
                        <p className="mt-1 text-sm text-red-600">{errors[`module_${index}_title`]}</p>
                      )}
                    </div>

                    <div>
                      <label className="block text-sm font-medium text-gray-700 mb-1">
                        Content Type
                      </label>
                      <select
                        value={module.content_type}
                        onChange={(e) => updateModule(module.id, { content_type: e.target.value as any })}
                        className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                      >
                        {CONTENT_TYPES.map(type => (
                          <option key={type.value} value={type.value}>
                            {type.label}
                          </option>
                        ))}
                      </select>
                    </div>

                    <div className="md:col-span-2">
                      <label className="block text-sm font-medium text-gray-700 mb-1">
                        Description *
                      </label>
                      <textarea
                        value={module.description}
                        onChange={(e) => updateModule(module.id, { description: e.target.value })}
                        rows={3}
                        className={clsx(
                          'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                          errors[`module_${index}_description`] ? 'border-red-300' : 'border-gray-300'
                        )}
                        placeholder="Describe the module content and objectives"
                      />
                      {errors[`module_${index}_description`] && (
                        <p className="mt-1 text-sm text-red-600">{errors[`module_${index}_description`]}</p>
                      )}
                    </div>

                    <div>
                      <label className="block text-sm font-medium text-gray-700 mb-1">
                        Duration (minutes) *
                      </label>
                      <input
                        type="number"
                        value={module.duration_minutes}
                        onChange={(e) => updateModule(module.id, { duration_minutes: parseInt(e.target.value) || 0 })}
                        min="1"
                        className={clsx(
                          'w-full px-3 py-2 border rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500',
                          errors[`module_${index}_duration`] ? 'border-red-300' : 'border-gray-300'
                        )}
                      />
                      {errors[`module_${index}_duration`] && (
                        <p className="mt-1 text-sm text-red-600">{errors[`module_${index}_duration`]}</p>
                      )}
                    </div>

                    <div className="flex items-center">
                      <label className="flex items-center">
                        <input
                          type="checkbox"
                          checked={module.is_required}
                          onChange={(e) => updateModule(module.id, { is_required: e.target.checked })}
                          className="rounded border-gray-300 text-blue-600 focus:ring-blue-500"
                        />
                        <span className="ml-2 text-sm text-gray-700">Required module</span>
                      </label>
                    </div>

                    {/* Content-specific fields */}
                    {(module.content_type === 'video' || module.content_type === 'document') && (
                      <div className="md:col-span-2">
                        <label className="block text-sm font-medium text-gray-700 mb-1">
                          Content URL
                        </label>
                        <input
                          type="url"
                          value={module.content_url || ''}
                          onChange={(e) => updateModule(module.id, { content_url: e.target.value })}
                          className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                          placeholder="https://example.com/content"
                        />
                      </div>
                    )}

                    {module.content_type === 'text' && (
                      <div className="md:col-span-2">
                        <label className="block text-sm font-medium text-gray-700 mb-1">
                          Content Text
                        </label>
                        <textarea
                          value={module.content_text || ''}
                          onChange={(e) => updateModule(module.id, { content_text: e.target.value })}
                          rows={4}
                          className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-blue-500"
                          placeholder="Enter the text content for this module"
                        />
                      </div>
                    )}
                  </div>
                </div>
              ))}
            </div>

            <div className="mt-6 flex justify-between">
              <button
                onClick={() => setCurrentStep('basic')}
                className="px-6 py-2 bg-gray-600 hover:bg-gray-700 text-white rounded-lg transition-colors"
              >
                Back
              </button>
              <button
                onClick={() => setCurrentStep('review')}
                className="px-6 py-2 bg-blue-600 hover:bg-blue-700 text-white rounded-lg transition-colors"
              >
                Next: Review Course
              </button>
            </div>
          </div>
        )}

        {currentStep === 'review' && (
          <div className="p-6">
            <h2 className="text-xl font-semibold text-gray-900 mb-6">Review Course</h2>

            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-6">
              {/* Course Summary */}
              <div className="space-y-4">
                <h3 className="font-semibold text-gray-900">Course Details</h3>

                <div className="space-y-3">
                  <div>
                    <span className="text-sm font-medium text-gray-500">Title:</span>
                    <p className="text-sm text-gray-900">{formData.title}</p>
                  </div>

                  <div>
                    <span className="text-sm font-medium text-gray-500">Type:</span>
                    <span className={clsx(
                      'ml-2 px-2 py-1 text-xs font-medium rounded-full',
                      COURSE_TYPES.find(t => t.value === formData.course_type)?.color || 'bg-gray-100 text-gray-800'
                    )}>
                      {COURSE_TYPES.find(t => t.value === formData.course_type)?.label}
                    </span>
                  </div>

                  <div>
                    <span className="text-sm font-medium text-gray-500">Difficulty:</span>
                    <span className={clsx(
                      'ml-2 px-2 py-1 text-xs font-medium rounded-full',
                      DIFFICULTY_LEVELS.find(l => l.value === formData.difficulty_level)?.color
                    )}>
                      {DIFFICULTY_LEVELS.find(l => l.value === formData.difficulty_level)?.label}
                    </span>
                  </div>

                  <div>
                    <span className="text-sm font-medium text-gray-500">Duration:</span>
                    <span className="ml-2 text-sm text-gray-900">
                      {Math.floor(formData.duration_minutes / 60)}h {formData.duration_minutes % 60}m
                    </span>
                  </div>

                  <div>
                    <span className="text-sm font-medium text-gray-500">Pass Threshold:</span>
                    <span className="ml-2 text-sm text-gray-900">{formData.pass_threshold}%</span>
                  </div>

                  {formData.tags.length > 0 && (
                    <div>
                      <span className="text-sm font-medium text-gray-500">Tags:</span>
                      <div className="mt-1 flex flex-wrap gap-1">
                        {formData.tags.map(tag => (
                          <span key={tag} className="px-2 py-1 bg-blue-100 text-blue-800 text-xs rounded-full">
                            {tag}
                          </span>
                        ))}
                      </div>
                    </div>
                  )}
                </div>
              </div>

              {/* Modules Summary */}
              <div className="space-y-4">
                <h3 className="font-semibold text-gray-900">Modules ({modules.length})</h3>

                <div className="space-y-2 max-h-64 overflow-y-auto">
                  {modules.map((module, index) => (
                    <div key={module.id} className="p-3 bg-gray-50 rounded-lg">
                      <div className="flex items-center justify-between mb-1">
                        <span className="font-medium text-sm text-gray-900">
                          {index + 1}. {module.title}
                        </span>
                        <span className="text-xs text-gray-500">{module.duration_minutes}m</span>
                      </div>
                      <div className="flex items-center gap-2">
                        <span className={clsx(
                          'px-1.5 py-0.5 text-xs font-medium rounded',
                          CONTENT_TYPES.find(t => t.value === module.content_type)?.label === 'Video' ? 'bg-red-100 text-red-800' :
                          CONTENT_TYPES.find(t => t.value === module.content_type)?.label === 'Text Content' ? 'bg-blue-100 text-blue-800' :
                          CONTENT_TYPES.find(t => t.value === module.content_type)?.label === 'Interactive' ? 'bg-green-100 text-green-800' :
                          CONTENT_TYPES.find(t => t.value === module.content_type)?.label === 'Quiz' ? 'bg-yellow-100 text-yellow-800' :
                          'bg-purple-100 text-purple-800'
                        )}>
                          {CONTENT_TYPES.find(t => t.value === module.content_type)?.label}
                        </span>
                        {module.is_required && (
                          <span className="px-1.5 py-0.5 text-xs font-medium bg-gray-200 text-gray-800 rounded">
                            Required
                          </span>
                        )}
                      </div>
                    </div>
                  ))}
                </div>
              </div>
            </div>

            <div className="mt-6 flex justify-between">
              <button
                onClick={() => setCurrentStep('modules')}
                className="px-6 py-2 bg-gray-600 hover:bg-gray-700 text-white rounded-lg transition-colors"
              >
                Back
              </button>
              <button
                onClick={handleSubmit}
                disabled={createCourseMutation.isPending}
                className="flex items-center gap-2 px-6 py-2 bg-green-600 hover:bg-green-700 disabled:bg-gray-400 text-white rounded-lg transition-colors"
              >
                {createCourseMutation.isPending ? (
                  <LoadingSpinner size="sm" />
                ) : (
                  <Save className="w-4 h-4" />
                )}
                Create Course
              </button>
            </div>

            {createCourseMutation.isError && (
              <div className="mt-4 p-4 bg-red-50 border border-red-200 rounded-lg">
                <p className="text-sm text-red-600">
                  Failed to create course: {createCourseMutation.error?.message || 'Unknown error'}
                </p>
              </div>
            )}
          </div>
        )}
      </div>
    </div>
  );
};

export default CourseCreation;
