/**
 * Customer Profile Page - Production Implementation
 * Comprehensive customer profile management with real KYC, risk, and compliance data
 * Connects to Customer Profile API endpoints
 */

import React, { useState, useEffect } from 'react';
import { useParams, Link } from 'react-router-dom';
import {
  User, Shield, AlertTriangle, CheckCircle, XCircle,
  TrendingUp, DollarSign, Activity, MapPin, Phone,
  Mail, Calendar, Briefcase, Flag, Eye, Edit3,
  RefreshCw, FileText, AlertCircle, Clock
} from 'lucide-react';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

interface CustomerProfilePageState {
  customer: API.CustomerProfile | null;
  riskEvents: API.CustomerRiskEvent[];
  transactions: API.Transaction[];
  loading: boolean;
  error: string | null;
  activeTab: 'overview' | 'risk' | 'transactions' | 'kyc';
}

const CustomerProfile: React.FC = () => {
  const { customerId } = useParams<{ customerId: string }>();
  const [state, setState] = useState<CustomerProfilePageState>({
    customer: null,
    riskEvents: [],
    transactions: [],
    loading: true,
    error: null,
    activeTab: 'overview'
  });

  useEffect(() => {
    if (customerId) {
      loadCustomerData();
    }
  }, [customerId]);

  const loadCustomerData = async () => {
    if (!customerId) return;

    setState(prev => ({ ...prev, loading: true, error: null }));

    try {
      // Load customer profile, risk events, and transactions in parallel
      const [customerProfile, riskEvents, transactions] = await Promise.all([
        apiClient.getCustomerProfile(customerId),
        apiClient.getCustomerRiskProfile(customerId),
        apiClient.getCustomerTransactions(customerId, 20)
      ]);

      setState(prev => ({
        ...prev,
        customer: customerProfile,
        riskEvents,
        transactions,
        loading: false
      }));
    } catch (error) {
      console.error('Failed to load customer data:', error);
      setState(prev => ({
        ...prev,
        loading: false,
        error: 'Failed to load customer profile'
      }));
    }
  };

  const updateKYCStatus = async (newStatus: string, notes?: string) => {
    if (!customerId) return;

    try {
      await apiClient.updateCustomerKYC(customerId, newStatus, notes);
      // Reload customer data to get updated status
      await loadCustomerData();
    } catch (error) {
      console.error('Failed to update KYC status:', error);
      setState(prev => ({
        ...prev,
        error: 'Failed to update KYC status'
      }));
    }
  };

  const getRiskColor = (riskRating: string) => {
    switch (riskRating?.toUpperCase()) {
      case 'CRITICAL': return 'text-red-600 bg-red-50 border-red-200';
      case 'HIGH': return 'text-orange-600 bg-orange-50 border-orange-200';
      case 'MEDIUM': return 'text-yellow-600 bg-yellow-50 border-yellow-200';
      case 'LOW': return 'text-green-600 bg-green-50 border-green-200';
      default: return 'text-gray-600 bg-gray-50 border-gray-200';
    }
  };

  const getKYCColor = (kycStatus: string) => {
    switch (kycStatus?.toUpperCase()) {
      case 'VERIFIED': return 'text-green-600 bg-green-50 border-green-200';
      case 'PENDING': return 'text-yellow-600 bg-yellow-50 border-yellow-200';
      case 'IN_PROGRESS': return 'text-blue-600 bg-blue-50 border-blue-200';
      case 'FAILED': case 'EXPIRED': return 'text-red-600 bg-red-50 border-red-200';
      default: return 'text-gray-600 bg-gray-50 border-gray-200';
    }
  };

  if (state.loading) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="animate-spin rounded-full h-12 w-12 border-b-2 border-blue-600"></div>
      </div>
    );
  }

  if (state.error || !state.customer) {
    return (
      <div className="min-h-screen bg-gray-50 flex items-center justify-center">
        <div className="text-center">
          <AlertCircle className="mx-auto h-12 w-12 text-red-500" />
          <h3 className="mt-2 text-sm font-medium text-gray-900">Error Loading Customer</h3>
          <p className="mt-1 text-sm text-gray-500">{state.error || 'Customer not found'}</p>
          <div className="mt-6">
            <Link
              to="/transactions"
              className="inline-flex items-center px-4 py-2 border border-transparent shadow-sm text-sm font-medium rounded-md text-white bg-blue-600 hover:bg-blue-700"
            >
              Back to Transactions
            </Link>
          </div>
        </div>
      </div>
    );
  }

  const customer = state.customer;

  return (
    <div className="min-h-screen bg-gray-50">
      {/* Header */}
      <div className="bg-white shadow">
        <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
          <div className="py-6">
            <div className="flex items-center justify-between">
              <div className="flex items-center">
                <Link
                  to="/transactions"
                  className="text-gray-400 hover:text-gray-600 mr-4"
                >
                  ← Back to Transactions
                </Link>
                <div>
                  <h1 className="text-2xl font-bold text-gray-900 flex items-center">
                    <User className="mr-2 h-8 w-8" />
                    {customer.fullName}
                  </h1>
                  <p className="text-sm text-gray-500 mt-1">Customer ID: {customer.customerId}</p>
                </div>
              </div>
              <button
                onClick={loadCustomerData}
                className="inline-flex items-center px-4 py-2 border border-gray-300 rounded-md shadow-sm text-sm font-medium text-gray-700 bg-white hover:bg-gray-50"
              >
                <RefreshCw className="mr-2 h-4 w-4" />
                Refresh
              </button>
            </div>
          </div>
        </div>
      </div>

      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {/* Risk and KYC Status Cards */}
        <div className="grid grid-cols-1 md:grid-cols-3 gap-6 mb-8">
          <div className={`p-6 rounded-lg border ${getRiskColor(customer.riskRating)}`}>
            <div className="flex items-center">
              <TrendingUp className="h-8 w-8" />
              <div className="ml-4">
                <p className="text-sm font-medium">Risk Rating</p>
                <p className="text-2xl font-bold">{customer.riskRating}</p>
                <p className="text-sm">Score: {customer.riskScore}/100</p>
              </div>
            </div>
          </div>

          <div className={`p-6 rounded-lg border ${getKYCColor(customer.kycStatus)}`}>
            <div className="flex items-center">
              <Shield className="h-8 w-8" />
              <div className="ml-4">
                <p className="text-sm font-medium">KYC Status</p>
                <p className="text-2xl font-bold">{customer.kycStatus}</p>
                {customer.kycExpiryDate && (
                  <p className="text-sm">Expires: {customer.kycExpiryDate}</p>
                )}
              </div>
            </div>
          </div>

          <div className="bg-white p-6 rounded-lg border border-gray-200">
            <div className="flex items-center">
              <Activity className="h-8 w-8 text-blue-600" />
              <div className="ml-4">
                <p className="text-sm font-medium text-gray-500">Total Transactions</p>
                <p className="text-2xl font-bold text-gray-900">{customer.totalTransactions}</p>
                <p className="text-sm text-gray-500">
                  ${customer.totalVolumeUsd?.toLocaleString()} volume
                </p>
              </div>
            </div>
          </div>
        </div>

        {/* Tab Navigation */}
        <div className="bg-white shadow rounded-lg mb-6">
          <div className="border-b border-gray-200">
            <nav className="-mb-px flex space-x-8 px-6">
              {[
                { id: 'overview', label: 'Overview', icon: User },
                { id: 'risk', label: 'Risk Profile', icon: AlertTriangle },
                { id: 'transactions', label: 'Transactions', icon: DollarSign },
                { id: 'kyc', label: 'KYC Management', icon: Shield }
              ].map(({ id, label, icon: Icon }) => (
                <button
                  key={id}
                  onClick={() => setState(prev => ({ ...prev, activeTab: id as any }))}
                  className={`py-4 px-1 border-b-2 font-medium text-sm flex items-center ${
                    state.activeTab === id
                      ? 'border-blue-500 text-blue-600'
                      : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
                  }`}
                >
                  <Icon className="mr-2 h-4 w-4" />
                  {label}
                </button>
              ))}
            </nav>
          </div>
        </div>

        {/* Tab Content */}
        <div className="bg-white shadow rounded-lg">
          {state.activeTab === 'overview' && (
            <div className="p-6">
              <h3 className="text-lg font-medium text-gray-900 mb-6">Customer Overview</h3>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div>
                  <h4 className="text-sm font-medium text-gray-500 uppercase tracking-wider mb-3">Personal Information</h4>
                  <dl className="space-y-3">
                    <div className="flex items-center">
                      <Mail className="h-4 w-4 text-gray-400 mr-2" />
                      <dt className="text-sm text-gray-500 mr-2">Email:</dt>
                      <dd className="text-sm text-gray-900">{customer.email || 'Not provided'}</dd>
                    </div>
                    <div className="flex items-center">
                      <Phone className="h-4 w-4 text-gray-400 mr-2" />
                      <dt className="text-sm text-gray-500 mr-2">Phone:</dt>
                      <dd className="text-sm text-gray-900">{customer.phone || 'Not provided'}</dd>
                    </div>
                    <div className="flex items-center">
                      <Calendar className="h-4 w-4 text-gray-400 mr-2" />
                      <dt className="text-sm text-gray-500 mr-2">Date of Birth:</dt>
                      <dd className="text-sm text-gray-900">{customer.dateOfBirth || 'Not provided'}</dd>
                    </div>
                    <div className="flex items-center">
                      <Briefcase className="h-4 w-4 text-gray-400 mr-2" />
                      <dt className="text-sm text-gray-500 mr-2">Occupation:</dt>
                      <dd className="text-sm text-gray-900">{customer.occupation || 'Not provided'}</dd>
                    </div>
                  </dl>
                </div>
                <div>
                  <h4 className="text-sm font-medium text-gray-500 uppercase tracking-wider mb-3">Compliance Flags</h4>
                  <dl className="space-y-3">
                    <div className="flex items-center">
                      <Flag className="h-4 w-4 text-gray-400 mr-2" />
                      <dt className="text-sm text-gray-500 mr-2">PEP Status:</dt>
                      <dd className={`text-sm ${customer.pepStatus ? 'text-red-600' : 'text-green-600'}`}>
                        {customer.pepStatus ? 'Yes' : 'No'}
                      </dd>
                    </div>
                    <div className="flex items-center">
                      <Flag className="h-4 w-4 text-gray-400 mr-2" />
                      <dt className="text-sm text-gray-500 mr-2">Sanctions Match:</dt>
                      <dd className={`text-sm ${customer.sanctionsMatch ? 'text-red-600' : 'text-green-600'}`}>
                        {customer.sanctionsMatch ? 'Yes' : 'No'}
                      </dd>
                    </div>
                    <div className="flex items-center">
                      <MapPin className="h-4 w-4 text-gray-400 mr-2" />
                      <dt className="text-sm text-gray-500 mr-2">Nationality:</dt>
                      <dd className="text-sm text-gray-900">{customer.nationality || 'Not provided'}</dd>
                    </div>
                    <div className="flex items-center">
                      <MapPin className="h-4 w-4 text-gray-400 mr-2" />
                      <dt className="text-sm text-gray-500 mr-2">Residency:</dt>
                      <dd className="text-sm text-gray-900">{customer.residencyCountry || 'Not provided'}</dd>
                    </div>
                  </dl>
                </div>
              </div>
            </div>
          )}

          {state.activeTab === 'risk' && (
            <div className="p-6">
              <h3 className="text-lg font-medium text-gray-900 mb-6">Risk Profile & Events</h3>
              <div className="space-y-4">
                {state.riskEvents.length === 0 ? (
                  <p className="text-gray-500 text-center py-8">No risk events found</p>
                ) : (
                  state.riskEvents.map((event) => (
                    <div key={event.eventId} className="border border-gray-200 rounded-lg p-4">
                      <div className="flex items-start justify-between">
                        <div className="flex-1">
                          <div className="flex items-center mb-2">
                            <h4 className="text-sm font-medium text-gray-900">{event.eventType}</h4>
                            <span className={`ml-2 inline-flex items-center px-2.5 py-0.5 rounded-full text-xs font-medium ${
                              event.severity === 'CRITICAL' ? 'bg-red-100 text-red-800' :
                              event.severity === 'HIGH' ? 'bg-orange-100 text-orange-800' :
                              event.severity === 'MEDIUM' ? 'bg-yellow-100 text-yellow-800' :
                              'bg-green-100 text-green-800'
                            }`}>
                              {event.severity}
                            </span>
                            {event.resolved && (
                              <CheckCircle className="ml-2 h-4 w-4 text-green-500" />
                            )}
                          </div>
                          <p className="text-sm text-gray-600 mb-2">{event.description}</p>
                          <div className="flex items-center text-xs text-gray-500">
                            <Clock className="mr-1 h-3 w-3" />
                            {event.detectedAt}
                            {event.resolved && event.resolutionNotes && (
                              <span className="ml-4 text-green-600">
                                Resolved: {event.resolutionNotes}
                              </span>
                            )}
                          </div>
                        </div>
                        <div className="text-right">
                          <p className="text-sm font-medium text-gray-900">
                            Risk Impact: +{event.riskScoreImpact}
                          </p>
                        </div>
                      </div>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}

          {state.activeTab === 'transactions' && (
            <div className="p-6">
              <h3 className="text-lg font-medium text-gray-900 mb-6">Recent Transactions</h3>
              <div className="space-y-4">
                {state.transactions.length === 0 ? (
                  <p className="text-gray-500 text-center py-8">No transactions found</p>
                ) : (
                  state.transactions.map((transaction) => (
                    <div key={transaction.id} className="border border-gray-200 rounded-lg p-4 hover:bg-gray-50">
                      <div className="flex items-center justify-between">
                        <div className="flex items-center">
                          <div className="flex-shrink-0">
                            {transaction.flagged ? (
                              <AlertTriangle className="h-8 w-8 text-red-500" />
                            ) : (
                              <CheckCircle className="h-8 w-8 text-green-500" />
                            )}
                          </div>
                          <div className="ml-4">
                            <Link
                              to={`/transactions/${transaction.id}`}
                              className="text-sm font-medium text-gray-900 hover:text-blue-600"
                            >
                              {transaction.id}
                            </Link>
                            <p className="text-sm text-gray-500">
                              {transaction.type} • {transaction.status} • {transaction.created_at}
                            </p>
                          </div>
                        </div>
                        <div className="text-right">
                          <p className="text-sm font-medium text-gray-900">
                            ${transaction.amount?.toLocaleString()} {transaction.currency}
                          </p>
                          <p className="text-xs text-gray-500">
                            Risk Score: {transaction.risk_score || 0}
                          </p>
                        </div>
                      </div>
                    </div>
                  ))
                )}
              </div>
            </div>
          )}

          {state.activeTab === 'kyc' && (
            <div className="p-6">
              <h3 className="text-lg font-medium text-gray-900 mb-6">KYC Management</h3>
              <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
                <div>
                  <h4 className="text-sm font-medium text-gray-500 uppercase tracking-wider mb-3">Current Status</h4>
                  <div className={`p-4 rounded-lg border ${getKYCColor(customer.kycStatus)}`}>
                    <div className="flex items-center justify-between">
                      <div>
                        <p className="text-sm font-medium">KYC Status</p>
                        <p className="text-lg font-bold">{customer.kycStatus}</p>
                        {customer.kycCompletedDate && (
                          <p className="text-sm">Completed: {customer.kycCompletedDate}</p>
                        )}
                        {customer.kycExpiryDate && (
                          <p className="text-sm">Expires: {customer.kycExpiryDate}</p>
                        )}
                      </div>
                      <div className="text-right">
                        <p className="text-sm text-gray-500">Documents</p>
                        <p className={`text-lg font-bold ${customer.kycDocumentsUploaded ? 'text-green-600' : 'text-red-600'}`}>
                          {customer.kycDocumentsUploaded ? 'Uploaded' : 'Missing'}
                        </p>
                      </div>
                    </div>
                  </div>
                </div>
                <div>
                  <h4 className="text-sm font-medium text-gray-500 uppercase tracking-wider mb-3">Update KYC Status</h4>
                  <div className="space-y-2">
                    {['PENDING', 'IN_PROGRESS', 'VERIFIED', 'FAILED', 'EXPIRED'].map((status) => (
                      <button
                        key={status}
                        onClick={() => updateKYCStatus(status)}
                        className={`w-full text-left px-3 py-2 rounded-md text-sm font-medium ${
                          customer.kycStatus === status
                            ? 'bg-blue-100 text-blue-800 border-blue-300'
                            : 'text-gray-700 bg-white border-gray-300 hover:bg-gray-50'
                        } border`}
                      >
                        {status}
                      </button>
                    ))}
                  </div>
                </div>
              </div>
            </div>
          )}
        </div>
      </div>
    </div>
  );
};

export default CustomerProfile;
