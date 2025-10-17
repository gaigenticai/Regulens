/**
 * Simple Production App - Working system
 * Basic authentication and dashboard
 */

import React, { useState } from 'react';
import LoginForm from './components/LoginForm';
import SimpleDashboard from './pages/SimpleDashboard';

const App: React.FC = () => {
  const [isAuthenticated, setIsAuthenticated] = useState(() => {
    return !!localStorage.getItem('accessToken');
  });

  const handleLogin = () => {
    setIsAuthenticated(true);
  };

  const handleLogout = () => {
    localStorage.removeItem('accessToken');
    localStorage.removeItem('user');
    setIsAuthenticated(false);
  };

  if (!isAuthenticated) {
    return <LoginForm onLogin={handleLogin} />;
  }

  return (
    <div>
      <nav className="bg-blue-600 text-white p-4">
        <div className="max-w-7xl mx-auto flex justify-between items-center">
          <h1 className="text-xl font-bold">Regulens Compliance System</h1>
          <button
            onClick={handleLogout}
            className="px-4 py-2 bg-blue-700 rounded hover:bg-blue-800"
          >
            Logout
          </button>
        </div>
      </nav>
      <SimpleDashboard />
    </div>
  );
};

export default App;