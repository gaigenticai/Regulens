/**
 * Login Hook
 * Handles login logic with real API calls
 */

import { useState } from 'react';
import { useNavigate, useLocation } from 'react-router-dom';
import { useAuth } from '@/contexts/AuthContext';
import type { LoginRequest } from '@/types/api';
import { AxiosError } from 'axios';

interface UseLoginReturn {
  login: (credentials: LoginRequest) => Promise<void>;
  isLoading: boolean;
  error: string | null;
  clearError: () => void;
}

export const useLogin = (): UseLoginReturn => {
  const [isLoading, setIsLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const { login: authLogin } = useAuth();
  const navigate = useNavigate();
  const location = useLocation();

  const login = async (credentials: LoginRequest): Promise<void> => {
    setIsLoading(true);
    setError(null);

    try {
      await authLogin(credentials);

      // Redirect to the page user was trying to access, or dashboard
      const from = (location.state as any)?.from?.pathname || '/';
      navigate(from, { replace: true });
    } catch (err) {
      const axiosError = err as AxiosError<{ error?: string; message?: string }>;

      if (axiosError.response) {
        // Server responded with error
        const errorMessage =
          axiosError.response.data?.message ||
          axiosError.response.data?.error ||
          'Login failed. Please check your credentials.';
        setError(errorMessage);
      } else if (axiosError.request) {
        // Request made but no response
        setError('Cannot connect to server. Please check your network connection.');
      } else {
        // Something else happened
        setError('An unexpected error occurred. Please try again.');
      }

      console.error('Login error:', err);
    } finally {
      setIsLoading(false);
    }
  };

  const clearError = (): void => {
    setError(null);
  };

  return {
    login,
    isLoading,
    error,
    clearError,
  };
};

export default useLogin;
