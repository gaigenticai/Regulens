/**
 * React Hook for Error Handling
 * Production-grade error handling hook for React components
 */

import { useState, useCallback, useEffect } from 'react';
import { AxiosError } from 'axios';
import {
  APIError,
  ErrorHandler,
  ErrorReporter,
  ErrorContext,
  ErrorSeverity,
  createErrorContext,
  handleAPIError
} from '@/utils/errorHandling';

interface ErrorState {
  error: APIError | null;
  isVisible: boolean;
  retryCount: number;
}

interface ErrorHandlingOptions {
  maxRetries?: number;
  retryDelay?: number;
  showToasts?: boolean;
  autoHideDelay?: number;
  onError?: (error: APIError) => void;
  onRetry?: (error: APIError, attempt: number) => void;
}

const defaultOptions: Required<ErrorHandlingOptions> = {
  maxRetries: 3,
  retryDelay: 1000,
  showToasts: true,
  autoHideDelay: 5000,
  onError: () => {},
  onRetry: () => {}
};

export const useErrorHandling = (options: ErrorHandlingOptions = {}) => {
  const opts = { ...defaultOptions, ...options };

  const [errorState, setErrorState] = useState<ErrorState>({
    error: null,
    isVisible: false,
    retryCount: 0
  });

  const [isRetrying, setIsRetrying] = useState(false);

  // Clear error state
  const clearError = useCallback(() => {
    setErrorState({
      error: null,
      isVisible: false,
      retryCount: 0
    });
  }, []);

  // Handle API error
  const handleError = useCallback((error: AxiosError, context: ErrorContext) => {
    const parsedError = handleAPIError(error, context);

    setErrorState(prev => ({
      error: parsedError,
      isVisible: true,
      retryCount: 0
    }));

    // Call error callback
    opts.onError(parsedError);

    return parsedError;
  }, [opts.onError]);

  // Handle error with automatic retry
  const handleErrorWithRetry = useCallback(async (
    error: AxiosError,
    context: ErrorContext,
    retryFunction?: () => Promise<any>
  ): Promise<any> => {
    const parsedError = handleAPIError(error, context);

    if (parsedError.isRetryable && retryFunction && errorState.retryCount < opts.maxRetries) {
      setIsRetrying(true);

      // Call retry callback
      opts.onRetry(parsedError, errorState.retryCount + 1);

      // Wait for retry delay
      await new Promise(resolve => setTimeout(resolve, opts.retryDelay));

      try {
        setIsRetrying(false);
        const result = await retryFunction();

        // Success - clear error state
        clearError();
        return result;

      } catch (retryError) {
        setIsRetrying(false);

        // Retry failed - increment count and try again if possible
        setErrorState(prev => ({
          ...prev,
          retryCount: prev.retryCount + 1
        }));

        if (errorState.retryCount + 1 < opts.maxRetries) {
          // Try again recursively
          return handleErrorWithRetry(error, context, retryFunction);
        } else {
          // Max retries reached - show error
          setErrorState({
            error: parsedError,
            isVisible: true,
            retryCount: opts.maxRetries
          });
          throw retryError;
        }
      }
    } else {
      // Not retryable or no retry function - show error
      setErrorState({
        error: parsedError,
        isVisible: true,
        retryCount: 0
      });
      throw error;
    }
  }, [errorState.retryCount, opts.maxRetries, opts.retryDelay, opts.onRetry, clearError]);

  // Manual retry
  const retry = useCallback(async (retryFunction?: () => Promise<any>) => {
    if (!errorState.error || !retryFunction) return;

    setErrorState(prev => ({
      ...prev,
      retryCount: prev.retryCount + 1
    }));

    try {
      const result = await retryFunction();
      clearError();
      return result;
    } catch (error) {
      // Retry failed - error will be handled by the calling code
      throw error;
    }
  }, [errorState.error, clearError]);

  // Show error manually
  const showError = useCallback((error: APIError) => {
    setErrorState({
      error,
      isVisible: true,
      retryCount: 0
    });
  }, []);

  // Hide error
  const hideError = useCallback(() => {
    setErrorState(prev => ({
      ...prev,
      isVisible: false
    }));
  }, []);

  // Auto-hide error after delay
  useEffect(() => {
    if (errorState.error && errorState.isVisible && opts.autoHideDelay > 0) {
      const timer = setTimeout(() => {
        if (errorState.error?.severity !== ErrorSeverity.CRITICAL) {
          hideError();
        }
      }, opts.autoHideDelay);

      return () => clearTimeout(timer);
    }
  }, [errorState.error, errorState.isVisible, opts.autoHideDelay, hideError]);

  // Report error to external service
  const reportError = useCallback(() => {
    if (errorState.error) {
      ErrorReporter.report(errorState.error);
    }
  }, [errorState.error]);

  return {
    // State
    error: errorState.error,
    isErrorVisible: errorState.isVisible,
    isRetrying,
    retryCount: errorState.retryCount,
    canRetry: errorState.error?.isRetryable && errorState.retryCount < opts.maxRetries,

    // Actions
    handleError,
    handleErrorWithRetry,
    clearError,
    showError,
    hideError,
    retry,
    reportError,

    // Utilities
    createContext: createErrorContext
  };
};

// Specialized hooks for common error scenarios

export const useAuthErrorHandling = (options: ErrorHandlingOptions = {}) => {
  const errorHandling = useErrorHandling({
    ...options,
    onError: (error) => {
      // Handle authentication errors specially
      if (error.code === 'AUTHENTICATION_ERROR') {
        // Redirect to login or refresh token
        window.location.href = '/login';
      }
      options.onError?.(error);
    }
  });

  return errorHandling;
};

export const useNetworkErrorHandling = (options: ErrorHandlingOptions = {}) => {
  const [isOnline, setIsOnline] = useState(navigator.onLine);

  useEffect(() => {
    const handleOnline = () => setIsOnline(true);
    const handleOffline = () => setIsOnline(false);

    window.addEventListener('online', handleOnline);
    window.addEventListener('offline', handleOffline);

    return () => {
      window.removeEventListener('online', handleOnline);
      window.removeEventListener('offline', handleOffline);
    };
  }, []);

  const errorHandling = useErrorHandling({
    ...options,
    maxRetries: isOnline ? options.maxRetries || 3 : 0, // Don't retry if offline
    onError: (error) => {
      if (error.code === 'NETWORK_ERROR' && !isOnline) {
        // Handle offline scenario
        console.warn('Network error while offline');
      }
      options.onError?.(error);
    }
  });

  return {
    ...errorHandling,
    isOnline
  };
};

// Hook for form validation errors
export const useFormErrorHandling = (options: ErrorHandlingOptions = {}) => {
  const [fieldErrors, setFieldErrors] = useState<Record<string, string>>({});

  const errorHandling = useErrorHandling({
    ...options,
    onError: (error) => {
      if (error.field) {
        setFieldErrors(prev => ({
          ...prev,
          [error.field!]: error.userMessage
        }));
      }
      options.onError?.(error);
    }
  });

  const clearFieldError = useCallback((field: string) => {
    setFieldErrors(prev => {
      const newErrors = { ...prev };
      delete newErrors[field];
      return newErrors;
    });
  }, []);

  const clearAllFieldErrors = useCallback(() => {
    setFieldErrors({});
  }, []);

  return {
    ...errorHandling,
    fieldErrors,
    clearFieldError,
    clearAllFieldErrors
  };
};
