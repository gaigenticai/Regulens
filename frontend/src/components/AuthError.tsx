/**
 * Authentication Error Component
 * Displays API error messages from login/auth failures
 */

import React from 'react';
import { AlertCircle, X } from 'lucide-react';
import { clsx } from 'clsx';

interface AuthErrorProps {
  message: string;
  onDismiss?: () => void;
  className?: string;
}

export const AuthError: React.FC<AuthErrorProps> = ({
  message,
  onDismiss,
  className,
}) => {
  return (
    <div
      className={clsx(
        'flex items-start gap-3 p-4 bg-red-50 border border-red-200 rounded-lg animate-fade-in',
        className
      )}
      role="alert"
    >
      <AlertCircle className="w-5 h-5 text-red-600 flex-shrink-0 mt-0.5" />

      <div className="flex-1 min-w-0">
        <p className="text-sm font-medium text-red-800">Authentication Error</p>
        <p className="text-sm text-red-700 mt-1">{message}</p>
      </div>

      {onDismiss && (
        <button
          onClick={onDismiss}
          className="flex-shrink-0 text-red-600 hover:text-red-800 transition-colors"
          aria-label="Dismiss error"
        >
          <X className="w-5 h-5" />
        </button>
      )}
    </div>
  );
};

export default AuthError;
