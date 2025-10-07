/**
 * Main App Component
 * React Router setup with protected and public routes
 * Real authentication flow - NO MOCKS
 */

import React, { Suspense } from 'react';
import { BrowserRouter, Routes, Route } from 'react-router-dom';
import { ErrorBoundary } from '@/components/ErrorBoundary';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { ProtectedRoute } from '@/components/ProtectedRoute';
import { MainLayout } from '@/components/Layout/MainLayout';
import { routes, publicRoutes } from '@/routes';

const App: React.FC = () => {
  return (
    <ErrorBoundary>
      <BrowserRouter>
        <Suspense fallback={<LoadingSpinner fullScreen message="Loading..." />}>
          <Routes>
            {/* Public routes */}
            {publicRoutes.map((route) => (
              <Route
                key={route.path}
                path={route.path}
                element={
                  <Suspense fallback={<LoadingSpinner fullScreen />}>
                    <route.element />
                  </Suspense>
                }
              />
            ))}

            {/* Protected routes with layout */}
            <Route
              element={
                <ProtectedRoute>
                  <MainLayout />
                </ProtectedRoute>
              }
            >
              {routes.map((route) => (
                <Route
                  key={route.path}
                  path={route.path}
                  element={
                    <Suspense fallback={<LoadingSpinner message="Loading page..." />}>
                      {route.requiredPermissions ? (
                        <ProtectedRoute requiredPermissions={route.requiredPermissions}>
                          <route.element />
                        </ProtectedRoute>
                      ) : (
                        <route.element />
                      )}
                    </Suspense>
                  }
                />
              ))}
            </Route>
          </Routes>
        </Suspense>
      </BrowserRouter>
    </ErrorBoundary>
  );
};

export default App;
