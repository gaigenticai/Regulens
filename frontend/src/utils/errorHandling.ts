/**
 * Frontend Error Handling Utilities
 * Production-grade error handling for standardized backend error responses
 */

import { AxiosError } from 'axios';

// Standardized error types from backend
export interface StandardizedError {
  error: {
    code: string;
    message: string;
    details?: string;
    field?: string;
    timestamp: string;
    request_id: string;
    path: string;
    method: string;
  };
  meta?: {
    version: string;
    compatibility_mode?: boolean;
    deprecation_warning?: string;
  };
}

export interface ErrorContext {
  endpoint: string;
  method: string;
  userId?: string;
  timestamp: string;
  requestId?: string;
}

// Error categories for better handling
export enum ErrorCategory {
  VALIDATION = 'validation',
  AUTHENTICATION = 'authentication',
  AUTHORIZATION = 'authorization',
  NOT_FOUND = 'not_found',
  CONFLICT = 'conflict',
  RATE_LIMIT = 'rate_limit',
  SERVER_ERROR = 'server_error',
  NETWORK_ERROR = 'network_error',
  CLIENT_ERROR = 'client_error'
}

// Error severity levels
export enum ErrorSeverity {
  LOW = 'low',
  MEDIUM = 'medium',
  HIGH = 'high',
  CRITICAL = 'critical'
}

// User-friendly error messages
const ERROR_MESSAGES: Record<string, string> = {
  // Validation errors
  VALIDATION_ERROR: 'Please check your input and try again.',
  AUTHENTICATION_ERROR: 'Please log in to continue.',
  AUTHORIZATION_ERROR: 'You don\'t have permission to perform this action.',
  NOT_FOUND: 'The requested item could not be found.',
  CONFLICT: 'This action conflicts with existing data.',
  RATE_LIMIT_EXCEEDED: 'Too many requests. Please wait a moment before trying again.',
  INTERNAL_ERROR: 'Something went wrong. Please try again later.',
  SERVICE_UNAVAILABLE: 'Service is temporarily unavailable. Please try again later.',
  MAINTENANCE_MODE: 'Service is under maintenance. Please check back later.',
  NETWORK_ERROR: 'Connection problem. Please check your internet and try again.',
  DATABASE_ERROR: 'Data service is temporarily unavailable. Please try again.',
  EXTERNAL_SERVICE_ERROR: 'External service is unavailable. Please try again later.'
};

// Error recovery actions
export interface ErrorRecoveryAction {
  label: string;
  action: () => void;
  primary?: boolean;
}

export class APIError extends Error {
  public readonly code: string;
  public readonly category: ErrorCategory;
  public readonly severity: ErrorSeverity;
  public readonly field?: string;
  public readonly details?: string;
  public readonly requestId?: string;
  public readonly timestamp: string;
  public readonly endpoint: string;
  public readonly httpStatus: number;
  public readonly isRetryable: boolean;
  public readonly retryAfter?: number;
  public readonly userMessage: string;
  public readonly recoveryActions: ErrorRecoveryAction[];

  constructor(
    code: string,
    message: string,
    httpStatus: number,
    context: ErrorContext,
    options: {
      field?: string;
      details?: string;
      isRetryable?: boolean;
      retryAfter?: number;
      category?: ErrorCategory;
      severity?: ErrorSeverity;
    } = {}
  ) {
    super(message);

    this.code = code;
    this.httpStatus = httpStatus;
    this.endpoint = context.endpoint;
    this.timestamp = context.timestamp;
    this.requestId = context.requestId;

    this.field = options.field;
    this.details = options.details;
    this.isRetryable = options.isRetryable ?? false;
    this.retryAfter = options.retryAfter;

    // Determine category and severity
    this.category = options.category ?? this.determineCategory(code, httpStatus);
    this.severity = options.severity ?? this.determineSeverity(code, httpStatus);

    // User-friendly message
    this.userMessage = this.getUserFriendlyMessage(code, message);

    // Recovery actions
    this.recoveryActions = this.generateRecoveryActions();

    this.name = 'APIError';
  }

  private determineCategory(code: string, httpStatus: number): ErrorCategory {
    if (httpStatus === 401) return ErrorCategory.AUTHENTICATION;
    if (httpStatus === 403) return ErrorCategory.AUTHORIZATION;
    if (httpStatus === 404) return ErrorCategory.NOT_FOUND;
    if (httpStatus === 409) return ErrorCategory.CONFLICT;
    if (httpStatus === 429) return ErrorCategory.RATE_LIMIT;
    if (httpStatus >= 500) return ErrorCategory.SERVER_ERROR;
    if (httpStatus === 400 || code === 'VALIDATION_ERROR') return ErrorCategory.VALIDATION;

    return ErrorCategory.CLIENT_ERROR;
  }

  private determineSeverity(code: string, httpStatus: number): ErrorSeverity {
    if (httpStatus >= 500) return ErrorSeverity.HIGH;
    if (httpStatus === 429) return ErrorSeverity.MEDIUM;
    if (httpStatus === 401 || httpStatus === 403) return ErrorSeverity.HIGH;
    if (code === 'VALIDATION_ERROR') return ErrorSeverity.LOW;

    return ErrorSeverity.MEDIUM;
  }

  private getUserFriendlyMessage(code: string, fallbackMessage: string): string {
    return ERROR_MESSAGES[code] || fallbackMessage;
  }

  private generateRecoveryActions(): ErrorRecoveryAction[] {
    const actions: ErrorRecoveryAction[] = [];

    switch (this.category) {
      case ErrorCategory.AUTHENTICATION:
        actions.push({
          label: 'Log In',
          action: () => window.location.href = '/login',
          primary: true
        });
        break;

      case ErrorCategory.RATE_LIMIT:
        if (this.retryAfter) {
          actions.push({
            label: `Retry in ${this.retryAfter}s`,
            action: () => setTimeout(() => window.location.reload(), this.retryAfter! * 1000),
            primary: true
          });
        }
        break;

      case ErrorCategory.SERVER_ERROR:
      case ErrorCategory.NETWORK_ERROR:
        actions.push({
          label: 'Try Again',
          action: () => window.location.reload(),
          primary: true
        });
        break;

      case ErrorCategory.VALIDATION:
        actions.push({
          label: 'Go Back',
          action: () => window.history.back(),
          primary: true
        });
        break;

      default:
        actions.push({
          label: 'Refresh Page',
          action: () => window.location.reload()
        });
    }

    return actions;
  }

  // Convert to display format
  toDisplayFormat() {
    return {
      title: this.getErrorTitle(),
      message: this.userMessage,
      details: this.details,
      code: this.code,
      severity: this.severity,
      category: this.category,
      field: this.field,
      recoveryActions: this.recoveryActions,
      requestId: this.requestId,
      timestamp: this.timestamp
    };
  }

  private getErrorTitle(): string {
    switch (this.category) {
      case ErrorCategory.AUTHENTICATION: return 'Authentication Required';
      case ErrorCategory.AUTHORIZATION: return 'Access Denied';
      case ErrorCategory.VALIDATION: return 'Invalid Input';
      case ErrorCategory.NOT_FOUND: return 'Not Found';
      case ErrorCategory.CONFLICT: return 'Conflict';
      case ErrorCategory.RATE_LIMIT: return 'Rate Limit Exceeded';
      case ErrorCategory.SERVER_ERROR: return 'Server Error';
      case ErrorCategory.NETWORK_ERROR: return 'Connection Error';
      default: return 'Error';
    }
  }
}

// Error parsing and creation utilities
export class ErrorHandler {
  static parseAPIError(error: AxiosError, context: ErrorContext): APIError {
    // Handle network errors
    if (!error.response) {
      return new APIError(
        'NETWORK_ERROR',
        'Network connection failed',
        0,
        context,
        {
          category: ErrorCategory.NETWORK_ERROR,
          isRetryable: true
        }
      );
    }

    const response = error.response;
    const data = response.data as StandardizedError;

    // Check if it's a standardized error response
    if (data && data.error) {
      const errorInfo = data.error;

      return new APIError(
        errorInfo.code,
        errorInfo.message,
        response.status,
        {
          ...context,
          requestId: errorInfo.request_id
        },
        {
          field: errorInfo.field,
          details: errorInfo.details,
          isRetryable: this.isRetryableError(errorInfo.code, response.status),
          retryAfter: this.getRetryAfter(response.headers)
        }
      );
    }

    // Fallback for non-standardized errors
    return new APIError(
      `HTTP_${response.status}`,
      error.message || 'An error occurred',
      response.status,
      context,
      {
        isRetryable: this.isRetryableStatus(response.status),
        retryAfter: this.getRetryAfter(response.headers)
      }
    );
  }

  static createValidationError(
    field: string,
    message: string,
    context: ErrorContext
  ): APIError {
    return new APIError(
      'VALIDATION_ERROR',
      message,
      400,
      context,
      {
        field,
        category: ErrorCategory.VALIDATION,
        severity: ErrorSeverity.LOW
      }
    );
  }

  static createNetworkError(context: ErrorContext): APIError {
    return new APIError(
      'NETWORK_ERROR',
      'Unable to connect to the server',
      0,
      context,
      {
        category: ErrorCategory.NETWORK_ERROR,
        isRetryable: true
      }
    );
  }

  private static isRetryableError(code: string, status: number): boolean {
    const retryableCodes = [
      'NETWORK_ERROR',
      'INTERNAL_ERROR',
      'SERVICE_UNAVAILABLE',
      'DATABASE_ERROR',
      'EXTERNAL_SERVICE_ERROR'
    ];

    return retryableCodes.includes(code) || status === 429 || status >= 500;
  }

  private static isRetryableStatus(status: number): boolean {
    return status === 429 || status >= 500;
  }

  private static getRetryAfter(headers: any): number | undefined {
    const retryAfter = headers?.['retry-after'] || headers?.['Retry-After'];
    if (retryAfter) {
      const parsed = parseInt(retryAfter, 10);
      return isNaN(parsed) ? undefined : parsed;
    }
    return undefined;
  }
}

// Error boundary component props
export interface ErrorBoundaryState {
  hasError: boolean;
  error?: APIError;
  errorInfo?: any;
}

// Error reporting utilities
export class ErrorReporter {
  static report(error: APIError): void {
    // In production, this would send to error tracking service
    console.error('API Error Reported:', {
      code: error.code,
      message: error.message,
      endpoint: error.endpoint,
      requestId: error.requestId,
      timestamp: error.timestamp,
      userMessage: error.userMessage
    });

    // Send to analytics/error tracking service
    this.sendToAnalytics(error);
  }

  static reportNetworkError(context: ErrorContext): void {
    const error = ErrorHandler.createNetworkError(context);
    this.report(error);
  }

  private static sendToAnalytics(error: APIError): void {
    // Integration point for error tracking services like Sentry, Rollbar, etc.
    if (typeof window !== 'undefined' && (window as any).gtag) {
      (window as any).gtag('event', 'exception', {
        description: `${error.code}: ${error.message}`,
        fatal: error.severity === ErrorSeverity.CRITICAL
      });
    }
  }
}

// Utility functions for common error handling patterns
export const handleAPIError = (error: AxiosError, context: ErrorContext): APIError => {
  const parsedError = ErrorHandler.parseAPIError(error, context);

  // Auto-report certain types of errors
  if (parsedError.severity === ErrorSeverity.HIGH || parsedError.severity === ErrorSeverity.CRITICAL) {
    ErrorReporter.report(parsedError);
  }

  return parsedError;
};

export const createErrorContext = (
  endpoint: string,
  method: string = 'GET',
  userId?: string
): ErrorContext => ({
  endpoint,
  method,
  userId,
  timestamp: new Date().toISOString()
});

export const isAuthenticationError = (error: APIError): boolean => {
  return error.category === ErrorCategory.AUTHENTICATION;
};

export const isAuthorizationError = (error: APIError): boolean => {
  return error.category === ErrorCategory.AUTHORIZATION;
};

export const isRetryableError = (error: APIError): boolean => {
  return error.isRetryable;
};

export const shouldShowErrorToUser = (error: APIError): boolean => {
  // Don't show internal errors to users in production
  return error.category !== ErrorCategory.SERVER_ERROR || process.env.NODE_ENV === 'development';
};

// Hook for error handling in React components
export const useErrorHandler = () => {
  const handleError = (error: AxiosError, context: ErrorContext) => {
    return handleAPIError(error, context);
  };

  const reportError = (error: APIError) => {
    ErrorReporter.report(error);
  };

  return { handleError, reportError };
};
