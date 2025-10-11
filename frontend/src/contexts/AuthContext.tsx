/**
 * Authentication Context - DATABASE-BACKED SESSION Implementation
 * 
 * Security features:
 * - HttpOnly cookies (session token stored in database)
 * - No client-side token storage (immune to XSS)
 * - Server-side session validation
 * - Production-grade security
 */

import React, { createContext, useContext, useState, useEffect, useCallback } from 'react';
import { apiClient } from '@/services/api';
import type { User, LoginRequest } from '@/types/api';

interface AuthContextType {
  user: User | null;
  isAuthenticated: boolean;
  isLoading: boolean;
  login: (credentials: LoginRequest) => Promise<void>;
  logout: () => Promise<void>;
  refreshUser: () => Promise<void>;
}

const AuthContext = createContext<AuthContextType | undefined>(undefined);

export function AuthProvider({ children }: { children: React.ReactNode }) {
  const [user, setUser] = useState<User | null>(null);
  const [isLoading, setIsLoading] = useState(true);

  // DATABASE-BACKED SESSIONS: No token management needed!
  // Session is validated on each request via HttpOnly cookie

  const login = async (credentials: LoginRequest) => {
    setIsLoading(true);
    
    try {
      // Backend sets HttpOnly cookie automatically
      const response = await apiClient.login(credentials);
      
      // Set user from response
      setUser(response.user);
      
    } catch (error) {
      setIsLoading(false);
      throw error;
    }
    
    setIsLoading(false);
  };

  const logout = async () => {
    setIsLoading(true);
    
    try {
      // Backend invalidates session in DB and clears cookie
      await apiClient.logout();
    } catch (error) {
      console.error('Logout error:', error);
    }
    
    // Clear user state
    setUser(null);
    setIsLoading(false);
    
    // Redirect to login
    window.location.href = '/login';
  };

  const refreshUser = useCallback(async () => {
    try {
      // Session is validated via HttpOnly cookie
      const userData = await apiClient.getCurrentUser();
      setUser(userData);
    } catch (error) {
      // Session invalid/expired
      setUser(null);
    } finally {
      setIsLoading(false);
    }
  }, []);

  // Initialize auth state on mount
  useEffect(() => {
    refreshUser();
  }, [refreshUser]);

  // Listen for auth events
  useEffect(() => {
    const handleLogout = () => {
      setUser(null);
      window.location.href = '/login';
    };

    window.addEventListener('auth:logout', handleLogout);
    return () => window.removeEventListener('auth:logout', handleLogout);
  }, []);

  const value = {
    user,
    isAuthenticated: !!user,
    isLoading,
    login,
    logout,
    refreshUser,
  };

  return <AuthContext.Provider value={value}>{children}</AuthContext.Provider>;
}

export function useAuth() {
  const context = useContext(AuthContext);
  if (context === undefined) {
    throw new Error('useAuth must be used within an AuthProvider');
  }
  return context;
}
