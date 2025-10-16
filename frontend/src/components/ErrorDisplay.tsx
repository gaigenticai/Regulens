/**
 * Error Display Component
 * User-friendly error display with recovery actions
 */

import React, { useState, useEffect } from 'react';
import { AlertTriangle, X, RefreshCw, AlertCircle, Info, CheckCircle } from 'lucide-react';
import { APIError, ErrorSeverity, ErrorCategory } from '@/utils/errorHandling';
import { Button } from './ui/button';
import { Alert, AlertDescription } from './ui/alert';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from './ui/card';
import { Badge } from './ui/badge';

interface ErrorDisplayProps {
  error: APIError | null;
  onClose?: () => void;
  onRetry?: () => void;
  showDetails?: boolean;
  autoHide?: boolean;
  autoHideDelay?: number;
  className?: string;
}

const ErrorDisplay: React.FC<ErrorDisplayProps> = ({
  error,
  onClose,
  onRetry,
  showDetails = false,
  autoHide = false,
  autoHideDelay = 5000,
  className = ''
}) => {
  const [isVisible, setIsVisible] = useState(true);
  const [showFullDetails, setShowFullDetails] = useState(showDetails);

  useEffect(() => {
    if (autoHide && error && autoHideDelay > 0) {
      const timer = setTimeout(() => {
        if (error.severity !== ErrorSeverity.CRITICAL) {
          handleClose();
        }
      }, autoHideDelay);

      return () => clearTimeout(timer);
    }
  }, [autoHide, autoHideDelay, error]);

  if (!error || !isVisible) {
    return null;
  }

  const handleClose = () => {
    setIsVisible(false);
    onClose?.();
  };

  const handleRetry = () => {
    onRetry?.();
  };

  const getSeverityColor = (severity: ErrorSeverity) => {
    switch (severity) {
      case ErrorSeverity.CRITICAL: return 'border-red-500 bg-red-50';
      case ErrorSeverity.HIGH: return 'border-orange-500 bg-orange-50';
      case ErrorSeverity.MEDIUM: return 'border-yellow-500 bg-yellow-50';
      case ErrorSeverity.LOW: return 'border-blue-500 bg-blue-50';
      default: return 'border-gray-500 bg-gray-50';
    }
  };

  const getSeverityIcon = (severity: ErrorSeverity) => {
    switch (severity) {
      case ErrorSeverity.CRITICAL: return <AlertTriangle className="h-5 w-5 text-red-600" />;
      case ErrorSeverity.HIGH: return <AlertCircle className="h-5 w-5 text-orange-600" />;
      case ErrorSeverity.MEDIUM: return <Info className="h-5 w-5 text-yellow-600" />;
      case ErrorSeverity.LOW: return <Info className="h-5 w-5 text-blue-600" />;
      default: return <AlertCircle className="h-5 w-5 text-gray-600" />;
    }
  };

  const getCategoryBadgeColor = (category: ErrorCategory) => {
    switch (category) {
      case ErrorCategory.AUTHENTICATION: return 'bg-red-100 text-red-800';
      case ErrorCategory.AUTHORIZATION: return 'bg-orange-100 text-orange-800';
      case ErrorCategory.VALIDATION: return 'bg-blue-100 text-blue-800';
      case ErrorCategory.NOT_FOUND: return 'bg-purple-100 text-purple-800';
      case ErrorCategory.SERVER_ERROR: return 'bg-red-100 text-red-800';
      case ErrorCategory.NETWORK_ERROR: return 'bg-gray-100 text-gray-800';
      case ErrorCategory.RATE_LIMIT: return 'bg-yellow-100 text-yellow-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const displayFormat = error.toDisplayFormat();

  return (
    <div className={`fixed inset-0 z-50 flex items-center justify-center p-4 bg-black bg-opacity-50 ${className}`}>
      <Card className={`w-full max-w-2xl ${getSeverityColor(error.severity)}`}>
        <CardHeader className="flex flex-row items-center justify-between">
          <div className="flex items-center gap-3">
            {getSeverityIcon(error.severity)}
            <div>
              <CardTitle className="text-lg">{displayFormat.title}</CardTitle>
              <CardDescription>
                <Badge className={getCategoryBadgeColor(error.category)}>
                  {error.category.replace('_', ' ').toUpperCase()}
                </Badge>
                {error.code && (
                  <Badge variant="outline" className="ml-2">
                    {error.code}
                  </Badge>
                )}
              </CardDescription>
            </div>
          </div>
          <Button variant="ghost" size="sm" onClick={handleClose}>
            <X className="h-4 w-4" />
          </Button>
        </CardHeader>

        <CardContent className="space-y-4">
          {/* Main error message */}
          <Alert>
            <AlertTriangle className="h-4 w-4" />
            <AlertDescription className="text-sm">
              {displayFormat.message}
            </AlertDescription>
          </Alert>

          {/* Field-specific error */}
          {displayFormat.field && (
            <div className="text-sm">
              <span className="font-medium text-red-700">Field: </span>
              <code className="bg-red-100 px-2 py-1 rounded text-red-800">
                {displayFormat.field}
              </code>
            </div>
          )}

          {/* Additional details */}
          {displayFormat.details && (
            <details className="text-sm">
              <summary className="cursor-pointer font-medium text-gray-700">
                Technical Details
              </summary>
              <div className="mt-2 p-3 bg-gray-100 rounded text-gray-800 font-mono text-xs">
                {displayFormat.details}
              </div>
            </details>
          )}

          {/* Full technical details (toggleable) */}
          {showFullDetails && (
            <details className="text-sm">
              <summary className="cursor-pointer font-medium text-gray-700">
                Full Error Information
              </summary>
              <div className="mt-2 space-y-2">
                <div className="grid grid-cols-2 gap-4 text-xs">
                  <div>
                    <span className="font-medium">Request ID:</span>
                    <div className="font-mono bg-gray-100 p-1 rounded mt-1">
                      {displayFormat.requestId || 'N/A'}
                    </div>
                  </div>
                  <div>
                    <span className="font-medium">Timestamp:</span>
                    <div className="font-mono bg-gray-100 p-1 rounded mt-1">
                      {new Date(displayFormat.timestamp).toLocaleString()}
                    </div>
                  </div>
                  <div>
                    <span className="font-medium">HTTP Status:</span>
                    <div className="font-mono bg-gray-100 p-1 rounded mt-1">
                      {error.httpStatus}
                    </div>
                  </div>
                  <div>
                    <span className="font-medium">Endpoint:</span>
                    <div className="font-mono bg-gray-100 p-1 rounded mt-1 break-all">
                      {error.endpoint}
                    </div>
                  </div>
                </div>

                {error.stack && (
                  <div>
                    <span className="font-medium">Stack Trace:</span>
                    <pre className="bg-gray-100 p-2 rounded mt-1 text-xs overflow-auto max-h-32">
                      {error.stack}
                    </pre>
                  </div>
                )}
              </div>
            </details>
          )}

          {/* Recovery actions */}
          <div className="flex flex-wrap gap-2 pt-4">
            {displayFormat.recoveryActions.map((action, index) => (
              <Button
                key={index}
                onClick={action.action}
                variant={action.primary ? "default" : "outline"}
                size="sm"
                className="flex items-center gap-2"
              >
                {action.label === 'Try Again' && <RefreshCw className="h-4 w-4" />}
                {action.label === 'Refresh Page' && <RefreshCw className="h-4 w-4" />}
                {action.label === 'Go Back' && <CheckCircle className="h-4 w-4" />}
                {action.label}
              </Button>
            ))}

            {error.isRetryable && onRetry && (
              <Button onClick={handleRetry} variant="outline" size="sm">
                <RefreshCw className="h-4 w-4 mr-2" />
                Retry
              </Button>
            )}

            <Button
              variant="ghost"
              size="sm"
              onClick={() => setShowFullDetails(!showFullDetails)}
            >
              {showFullDetails ? 'Hide' : 'Show'} Details
            </Button>
          </div>

          {/* Auto-hide notice */}
          {autoHide && error.severity !== ErrorSeverity.CRITICAL && (
            <div className="text-xs text-gray-500 text-center pt-2 border-t">
              This message will auto-hide in {Math.ceil(autoHideDelay / 1000)} seconds
            </div>
          )}
        </CardContent>
      </Card>
    </div>
  );
};

// Inline error display for forms and smaller contexts
interface InlineErrorDisplayProps {
  error: APIError | null;
  onRetry?: () => void;
  showFieldErrors?: boolean;
  className?: string;
}

export const InlineErrorDisplay: React.FC<InlineErrorDisplayProps> = ({
  error,
  onRetry,
  showFieldErrors = true,
  className = ''
}) => {
  if (!error) return null;

  return (
    <Alert variant="destructive" className={className}>
      <AlertTriangle className="h-4 w-4" />
      <AlertDescription className="flex items-center justify-between">
        <div className="flex-1">
          <div className="font-medium">{error.userMessage}</div>
          {showFieldErrors && error.field && (
            <div className="text-sm mt-1">
              Field: <code className="bg-red-100 px-1 py-0.5 rounded text-xs">{error.field}</code>
            </div>
          )}
        </div>
        {error.isRetryable && onRetry && (
          <Button onClick={onRetry} variant="outline" size="sm" className="ml-4">
            <RefreshCw className="h-4 w-4 mr-2" />
            Retry
          </Button>
        )}
      </AlertDescription>
    </Alert>
  );
};

// Toast-style error notification
interface ToastErrorDisplayProps {
  error: APIError | null;
  onClose: () => void;
  duration?: number;
  position?: 'top-right' | 'top-left' | 'bottom-right' | 'bottom-left';
}

export const ToastErrorDisplay: React.FC<ToastErrorDisplayProps> = ({
  error,
  onClose,
  duration = 5000,
  position = 'top-right'
}) => {
  const [isVisible, setIsVisible] = useState(false);

  useEffect(() => {
    if (error) {
      setIsVisible(true);

      if (duration > 0 && error.severity !== ErrorSeverity.CRITICAL) {
        const timer = setTimeout(() => {
          setIsVisible(false);
          setTimeout(onClose, 300); // Allow animation to complete
        }, duration);

        return () => clearTimeout(timer);
      }
    } else {
      setIsVisible(false);
    }
  }, [error, duration, onClose]);

  if (!error || !isVisible) return null;

  const positionClasses = {
    'top-right': 'top-4 right-4',
    'top-left': 'top-4 left-4',
    'bottom-right': 'bottom-4 right-4',
    'bottom-left': 'bottom-4 left-4'
  };

  return (
    <div className={`fixed z-50 ${positionClasses[position]} max-w-sm w-full`}>
      <Alert variant="destructive" className="shadow-lg animate-in slide-in-from-right-2">
        <AlertTriangle className="h-4 w-4" />
        <AlertDescription className="flex items-start justify-between">
          <div className="flex-1 pr-4">
            <div className="font-medium text-sm">{error.userMessage}</div>
            {error.field && (
              <div className="text-xs mt-1 opacity-80">
                Field: {error.field}
              </div>
            )}
          </div>
          <Button variant="ghost" size="sm" onClick={() => { setIsVisible(false); onClose(); }}>
            <X className="h-4 w-4" />
          </Button>
        </AlertDescription>
      </Alert>
    </div>
  );
};

export default ErrorDisplay;
