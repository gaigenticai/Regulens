/**
 * Login Page
 * Real authentication interface - NO MOCKS
 */

import React from 'react';
import { Navigate } from 'react-router-dom';
import { useAuth } from '@/contexts/AuthContext';
import { LoginForm } from '@/components/LoginForm';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { Shield, Layers } from 'lucide-react';

const Login: React.FC = () => {
  const { isAuthenticated, isLoading } = useAuth();

  // Redirect if already authenticated
  if (isLoading) {
    return <LoadingSpinner fullScreen message="Checking authentication..." />;
  }

  if (isAuthenticated) {
    return <Navigate to="/" replace />;
  }

  return (
    <div className="min-h-screen bg-gradient-to-br from-blue-50 via-indigo-50 to-purple-50 flex items-center justify-center px-4 py-12">
      <div className="max-w-md w-full">
        {/* Logo and branding */}
        <div className="text-center mb-8 animate-fade-in">
          <div className="inline-flex items-center justify-center w-16 h-16 bg-blue-600 rounded-2xl mb-4 shadow-lg">
            <Layers className="w-8 h-8 text-white" />
          </div>
          <h1 className="text-3xl font-bold text-gray-900 mb-2">Regulens</h1>
          <p className="text-gray-600">AI-Powered Regulatory Compliance Platform</p>
        </div>

        {/* Login card */}
        <div className="bg-white rounded-2xl shadow-xl p-8 animate-fade-in">
          <div className="flex items-center gap-2 mb-6">
            <Shield className="w-6 h-6 text-blue-600" />
            <h2 className="text-2xl font-bold text-gray-900">Sign In</h2>
          </div>

          <LoginForm />

          {/* Demo credentials notice for development */}
          {import.meta.env.DEV && (
            <div className="mt-6 p-4 bg-blue-50 border border-blue-200 rounded-lg">
              <p className="text-xs font-medium text-blue-900 mb-1">Development Mode</p>
              <p className="text-xs text-blue-700">
                Use your backend credentials to sign in. The app connects to the real API at{' '}
                <code className="bg-blue-100 px-1 py-0.5 rounded">localhost:8080</code>
              </p>
            </div>
          )}
        </div>

        {/* Footer */}
        <p className="text-center text-sm text-gray-500 mt-8">
          Powered by Multi-Criteria Decision Analysis & AI Agents
        </p>
      </div>
    </div>
  );
};

export default Login;
