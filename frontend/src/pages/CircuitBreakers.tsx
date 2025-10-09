import React from 'react';
import { Shield, ArrowLeft } from 'lucide-react';
import { Link } from 'react-router-dom';

const CircuitBreakers: React.FC = () => {
  return (
    <div className="min-h-screen bg-gray-50 flex items-center justify-center">
      <div className="max-w-md w-full bg-white rounded-lg shadow-sm p-8 text-center">
        <Shield className="w-16 h-16 text-blue-500 mx-auto mb-4" />
        <h1 className="text-2xl font-bold text-gray-900 mb-2">Circuit Breakers</h1>
        <p className="text-gray-600 mb-6">Production-grade circuit breaker management</p>
        <Link to="/" className="inline-flex items-center px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700">
          <ArrowLeft className="w-4 h-4 mr-2" />
          Dashboard
        </Link>
      </div>
    </div>
  );
};

export default CircuitBreakers;
