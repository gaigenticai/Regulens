/**
 * Login Form Component
 * Real authentication with form validation
 * Calls /api/auth/login endpoint
 */

import React, { useState, FormEvent } from 'react';
import { Eye, EyeOff, LogIn } from 'lucide-react';
import { useLogin } from '@/hooks/useLogin';
import { AuthError } from '@/components/AuthError';
import type { LoginRequest } from '@/types/api';

export const LoginForm: React.FC = () => {
  const [credentials, setCredentials] = useState<LoginRequest>({
    username: '',
    password: '',
  });
  const [showPassword, setShowPassword] = useState(false);
  const [validationErrors, setValidationErrors] = useState<Partial<LoginRequest>>({});

  const { login, isLoading, error, clearError } = useLogin();

  const validateForm = (): boolean => {
    const errors: Partial<LoginRequest> = {};

    if (!credentials.username.trim()) {
      errors.username = 'Username is required';
    }

    if (!credentials.password) {
      errors.password = 'Password is required';
    } else if (credentials.password.length < 6) {
      errors.password = 'Password must be at least 6 characters';
    }

    setValidationErrors(errors);
    return Object.keys(errors).length === 0;
  };

  const handleSubmit = async (e: FormEvent<HTMLFormElement>): Promise<void> => {
    e.preventDefault();
    clearError();

    if (!validateForm()) {
      return;
    }

    await login(credentials);
  };

  const handleInputChange = (field: keyof LoginRequest, value: string): void => {
    setCredentials((prev) => ({ ...prev, [field]: value }));
    // Clear validation error for this field
    if (validationErrors[field]) {
      setValidationErrors((prev) => ({ ...prev, [field]: undefined }));
    }
    // Clear API error when user starts typing
    if (error) {
      clearError();
    }
  };

  return (
    <form onSubmit={handleSubmit} className="space-y-6">
      {error && <AuthError message={error} onDismiss={clearError} />}

      <div>
        <label htmlFor="username" className="block text-sm font-medium text-gray-700 mb-2">
          Username
        </label>
        <input
          id="username"
          type="text"
          autoComplete="username"
          required
          value={credentials.username}
          onChange={(e) => handleInputChange('username', e.target.value)}
          className={`
            w-full px-4 py-3 rounded-lg border transition-colors
            ${validationErrors.username
              ? 'border-red-300 focus:border-red-500 focus:ring-red-500'
              : 'border-gray-300 focus:border-blue-500 focus:ring-blue-500'
            }
            focus:outline-none focus:ring-2
            disabled:bg-gray-100 disabled:cursor-not-allowed
          `}
          placeholder="Enter your username"
          disabled={isLoading}
        />
        {validationErrors.username && (
          <p className="mt-1 text-sm text-red-600">{validationErrors.username}</p>
        )}
      </div>

      <div>
        <label htmlFor="password" className="block text-sm font-medium text-gray-700 mb-2">
          Password
        </label>
        <div className="relative">
          <input
            id="password"
            type={showPassword ? 'text' : 'password'}
            autoComplete="current-password"
            required
            value={credentials.password}
            onChange={(e) => handleInputChange('password', e.target.value)}
            className={`
              w-full px-4 py-3 pr-12 rounded-lg border transition-colors
              ${validationErrors.password
                ? 'border-red-300 focus:border-red-500 focus:ring-red-500'
                : 'border-gray-300 focus:border-blue-500 focus:ring-blue-500'
              }
              focus:outline-none focus:ring-2
              disabled:bg-gray-100 disabled:cursor-not-allowed
            `}
            placeholder="Enter your password"
            disabled={isLoading}
          />
          <button
            type="button"
            onClick={() => setShowPassword(!showPassword)}
            className="absolute right-3 top-1/2 -translate-y-1/2 text-gray-500 hover:text-gray-700 transition-colors"
            tabIndex={-1}
            disabled={isLoading}
          >
            {showPassword ? <EyeOff className="w-5 h-5" /> : <Eye className="w-5 h-5" />}
          </button>
        </div>
        {validationErrors.password && (
          <p className="mt-1 text-sm text-red-600">{validationErrors.password}</p>
        )}
      </div>

      <button
        type="submit"
        disabled={isLoading}
        className="
          w-full flex items-center justify-center gap-2
          px-4 py-3 bg-blue-600 text-white font-medium rounded-lg
          hover:bg-blue-700 focus:outline-none focus:ring-2 focus:ring-blue-500 focus:ring-offset-2
          disabled:bg-blue-400 disabled:cursor-not-allowed
          transition-colors
        "
      >
        {isLoading ? (
          <>
            <div className="w-5 h-5 border-2 border-white border-t-transparent rounded-full animate-spin" />
            Signing in...
          </>
        ) : (
          <>
            <LogIn className="w-5 h-5" />
            Sign In
          </>
        )}
      </button>
    </form>
  );
};

export default LoginForm;
