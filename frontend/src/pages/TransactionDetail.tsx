/**
 * Transaction Detail Page - Production Implementation
 * Detailed transaction analysis and fraud detection using real backend APIs
 * Connects to Transaction Guardian Agent and Risk Assessment Engine
 */

import React, { useState, useEffect } from 'react';
import { useParams, Link } from 'react-router-dom';
import {
  CreditCard, ArrowLeft, AlertTriangle, Shield, TrendingUp,
  Clock, User, MapPin, DollarSign, Activity, CheckCircle,
  XCircle, AlertCircle, BarChart3, Network, Eye
} from 'lucide-react';
import { apiClient } from '@/services/api';
import type * as API from '@/types/api';

// Use API types instead of local interface
type CustomerProfile = API.CustomerProfile;

interface BehaviorPattern {
  pattern_type: string;
  confidence_score: number;
  pattern_data: Record<string, any>;
  last_observed: string;
}

const TransactionDetail: React.FC = () => {
  const { id } = useParams<{ id: string }>();
  const [transaction, setTransaction] = useState<API.Transaction | null>(null);
  const [riskAssessment, setRiskAssessment] = useState<API.FraudAnalysis | null>(null);
  const [customerProfile, setCustomerProfile] = useState<CustomerProfile | null>(null);
  const [behaviorPatterns, setBehaviorPatterns] = useState<BehaviorPattern[]>([]);
  const [relatedTransactions, setRelatedTransactions] = useState<API.Transaction[]>([]);
  const [loading, setLoading] = useState(true);
  const [error, setError] = useState<string | null>(null);
  const [activeTab, setActiveTab] = useState<'overview' | 'risk' | 'customer' | 'patterns' | 'related'>('overview');

  useEffect(() => {
    if (id) {
      loadTransactionData(id);
    }
  }, [id]);

  const loadTransactionData = async (transactionId: string) => {
    try {
      setLoading(true);
      setError(null);

      // Load transaction details
      const transaction = await apiClient.getTransactionById(transactionId);
      setTransaction(transaction);

      // Try to load fraud analysis
      try {
        const riskAnalysis = await apiClient.getFraudAnalysis(transactionId);
        setRiskAssessment(riskAnalysis);
      } catch (err) {
        console.log('No fraud analysis available for this transaction');
      }

      // Load related transactions for the same customer
      if (transaction.from) {
        try {
          const related = await apiClient.getTransactions({ limit: 10 });
          setRelatedTransactions(related.filter(t => t.id !== transactionId).slice(0, 5));
        } catch (err) {
          console.error('Failed to load related transactions:', err);
        }
      }

      // Load real customer profile from API
      if (transaction.from) {
        try {
          const customerProfile = await apiClient.getCustomerProfile(transaction.from);
          setCustomerProfile(customerProfile);
        } catch (error) {
          console.error('Failed to load customer profile:', error);
          // Set error state instead of mock data
          setCustomerProfile({
            customer_id: transaction.from,
            full_name: 'Error Loading Customer',
            risk_rating: 'ERROR',
            kyc_status: 'ERROR',
            pep_status: false,
            watchlist_flags: [],
            nationality: '',
            residency_country: '',
            account_status: 'error',
            total_transactions: 0,
            flagged_transactions: 0
          });
        }
      }

    } catch (err) {
      setError('Failed to load transaction details');
      console.error('Error loading transaction:', err);
    } finally {
      setLoading(false);
    }
  };

  const getRiskColor = (riskScore: number) => {
    if (riskScore >= 0.8) return 'text-red-600 bg-red-50';
    if (riskScore >= 0.6) return 'text-orange-600 bg-orange-50';
    if (riskScore >= 0.4) return 'text-yellow-600 bg-yellow-50';
    return 'text-green-600 bg-green-50';
  };

  const getRiskLabel = (riskScore: number) => {
    if (riskScore >= 0.8) return 'High Risk';
    if (riskScore >= 0.6) return 'Medium Risk';
    if (riskScore >= 0.4) return 'Low Risk';
    return 'Very Low Risk';
  };

  const getStatusIcon = (status: string, flagged: boolean) => {
    if (flagged) return <AlertTriangle className="w-5 h-5 text-red-500" />;
    if (status === 'completed') return <CheckCircle className="w-5 h-5 text-green-500" />;
    if (status === 'processing') return <Clock className="w-5 h-5 text-blue-500" />;
    if (status === 'failed') return <XCircle className="w-5 h-5 text-red-500" />;
    return <AlertCircle className="w-5 h-5 text-gray-500" />;
  };

  if (loading) {
    return (
      <div className="p-6">
        <div className="animate-pulse space-y-6">
          <div className="h-8 bg-gray-200 rounded w-1/3"></div>
          <div className="space-y-4">
            <div className="h-32 bg-gray-200 rounded"></div>
            <div className="grid grid-cols-2 gap-4">
              <div className="h-24 bg-gray-200 rounded"></div>
              <div className="h-24 bg-gray-200 rounded"></div>
            </div>
          </div>
        </div>
      </div>
    );
  }

  if (error || !transaction) {
    return (
      <div className="p-6">
        <div className="flex items-center gap-4 mb-6">
          <Link to="/transactions" className="p-2 hover:bg-gray-100 rounded-lg">
            <ArrowLeft className="w-5 h-5" />
          </Link>
          <h1 className="text-2xl font-bold text-gray-900">Transaction Details</h1>
        </div>
        <div className="bg-red-50 border border-red-200 rounded-lg p-6">
          <div className="flex items-center gap-3">
            <AlertCircle className="w-6 h-6 text-red-600" />
            <div>
              <h3 className="font-semibold text-red-800">Error Loading Transaction</h3>
              <p className="text-red-600">{error || 'Transaction not found'}</p>
            </div>
          </div>
        </div>
      </div>
    );
  }

  return (
    <div className="p-6 space-y-6">
      {/* Header */}
      <div className="flex items-center gap-4">
        <Link to="/transactions" className="p-2 hover:bg-gray-100 rounded-lg">
          <ArrowLeft className="w-5 h-5" />
        </Link>
        <div className="flex-1">
          <h1 className="text-2xl font-bold text-gray-900">Transaction Details</h1>
          <p className="text-gray-600">ID: {transaction.id}</p>
        </div>
        <div className="flex items-center gap-2">
          {getStatusIcon(transaction.status, transaction.status === 'flagged')}
          <span className={`px-3 py-1 rounded-full text-sm font-medium ${
            transaction.status === 'flagged' ? 'bg-red-100 text-red-800' : 'bg-green-100 text-green-800'
          }`}>
            {transaction.status === 'flagged' ? 'Flagged' : transaction.status}
          </span>
        </div>
      </div>

      {/* Risk Alert */}
      {transaction.status === 'flagged' && (
        <div className="bg-red-50 border-l-4 border-red-400 p-4 rounded-r-lg">
          <div className="flex items-center gap-3">
            <AlertTriangle className="w-6 h-6 text-red-600" />
            <div>
              <h3 className="font-semibold text-red-800">High-Risk Transaction Flagged</h3>
              <p className="text-red-700">Risk score: {(transaction.riskScore * 100).toFixed(1)}%</p>
            </div>
          </div>
        </div>
      )}

      {/* Tab Navigation */}
      <div className="border-b border-gray-200">
        <nav className="flex space-x-8">
          {[
            { id: 'overview', label: 'Overview', icon: Eye },
            { id: 'risk', label: 'Risk Analysis', icon: Shield },
            { id: 'customer', label: 'Customer Profile', icon: User },
            { id: 'patterns', label: 'Behavior Patterns', icon: Network },
            { id: 'related', label: 'Related Transactions', icon: Activity }
          ].map(({ id, label, icon: Icon }) => (
            <button
              key={id}
              onClick={() => setActiveTab(id as any)}
              className={`flex items-center gap-2 py-2 px-1 border-b-2 font-medium text-sm ${
                activeTab === id
                  ? 'border-blue-500 text-blue-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
            >
              <Icon className="w-4 h-4" />
              {label}
            </button>
          ))}
        </nav>
      </div>

      {/* Tab Content */}
      {activeTab === 'overview' && (
        <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
          {/* Transaction Summary */}
          <div className="bg-white rounded-lg shadow p-6">
            <div className="flex items-center gap-3 mb-4">
              <CreditCard className="w-6 h-6 text-blue-600" />
              <h2 className="text-lg font-semibold">Transaction Summary</h2>
            </div>
            <div className="space-y-4">
              <div className="flex justify-between items-center py-2 border-b">
                <span className="text-gray-600">Amount</span>
                <div className="flex items-center gap-1">
                  <DollarSign className="w-4 h-4 text-green-600" />
                  <span className="font-semibold text-lg">{transaction.amount.toLocaleString()} {transaction.currency}</span>
                </div>
              </div>
              <div className="flex justify-between items-center py-2 border-b">
                <span className="text-gray-600">Type</span>
                <span className="font-medium">{transaction.type}</span>
              </div>
              <div className="flex justify-between items-center py-2 border-b">
                <span className="text-gray-600">Date</span>
                <span className="font-medium">{new Date(transaction.timestamp).toLocaleString()}</span>
              </div>
              <div className="flex justify-between items-center py-2 border-b">
                <span className="text-gray-600">Status</span>
                <span className="font-medium capitalize">{transaction.status}</span>
              </div>
              <div className="flex justify-between items-center py-2">
                <span className="text-gray-600">Description</span>
                <span className="font-medium">{transaction.description}</span>
              </div>
            </div>
          </div>

          {/* Risk Score */}
          <div className="bg-white rounded-lg shadow p-6">
            <div className="flex items-center gap-3 mb-4">
              <Shield className="w-6 h-6 text-orange-600" />
              <h2 className="text-lg font-semibold">Risk Assessment</h2>
            </div>
            <div className="text-center">
              <div className={`inline-flex items-center gap-2 px-4 py-2 rounded-lg ${getRiskColor(transaction.riskScore / 100)}`}>
                <span className="text-2xl font-bold">{transaction.riskScore.toFixed(1)}%</span>
                <span className="font-medium">{getRiskLabel(transaction.riskScore / 100)}</span>
              </div>
              {riskAssessment && (
                <div className="mt-4 text-left">
                  <p className="text-sm text-gray-700">{riskAssessment.recommendation}</p>
                  <p className="text-xs text-gray-500 mt-2">
                    Confidence: {(riskAssessment.confidence * 100).toFixed(1)}%
                  </p>
                </div>
              )}
            </div>
          </div>

          {/* Sender/Receiver Details */}
          <div className="bg-white rounded-lg shadow p-6 lg:col-span-2">
            <h2 className="text-lg font-semibold mb-4">Transaction Parties</h2>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div>
                <h3 className="font-medium text-gray-900 mb-2">Sender</h3>
                <div className="space-y-2">
                  <p><span className="text-gray-600">Name:</span> {transaction.from}</p>
                  <p><span className="text-gray-600">Account:</span> {transaction.fromAccount}</p>
                </div>
              </div>
              <div>
                <h3 className="font-medium text-gray-900 mb-2">Receiver</h3>
                <div className="space-y-2">
                  <p><span className="text-gray-600">Name:</span> {transaction.to}</p>
                  <p><span className="text-gray-600">Account:</span> {transaction.toAccount}</p>
                </div>
              </div>
            </div>
          </div>
        </div>
      )}

      {activeTab === 'risk' && riskAssessment && (
        <div className="space-y-6">
          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-lg font-semibold mb-4">Detailed Risk Analysis</h2>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div>
                <h3 className="font-medium mb-3">Risk Indicators</h3>
                <div className="space-y-2">
                  {riskAssessment.indicators && riskAssessment.indicators.length > 0 ? (
                    riskAssessment.indicators.map((indicator, index) => (
                      <div key={index} className="flex items-start gap-2 py-1">
                        <AlertTriangle className="w-4 h-4 text-orange-600 mt-0.5 flex-shrink-0" />
                        <span className="text-sm text-gray-700">{indicator}</span>
                      </div>
                    ))
                  ) : (
                    <p className="text-gray-500 text-sm">No risk indicators detected</p>
                  )}
                </div>
              </div>
              <div>
                <h3 className="font-medium mb-3">Recommendation</h3>
                <div className="flex items-start gap-2">
                  <CheckCircle className="w-4 h-4 text-green-600 mt-0.5 flex-shrink-0" />
                  <span className="text-sm text-gray-700">{riskAssessment.recommendation}</span>
                </div>
              </div>
            </div>
            <div className="mt-6 pt-4 border-t">
              <p className="text-sm text-gray-600">
                Assessment ID: {riskAssessment.assessmentId} • 
                Analyzed: {new Date(riskAssessment.timestamp * 1000).toLocaleString()}
              </p>
            </div>
          </div>
        </div>
      )}

      {activeTab === 'customer' && customerProfile && (
        <div className="space-y-6">
          <div className="bg-white rounded-lg shadow p-6">
            <div className="flex items-center justify-between mb-4">
              <h2 className="text-lg font-semibold">Customer Profile</h2>
              {customerProfile.customer_id && (
                <Link
                  to={`/customers/${customerProfile.customer_id}`}
                  className="inline-flex items-center px-3 py-1.5 border border-gray-300 shadow-sm text-sm leading-4 font-medium rounded-md text-gray-700 bg-white hover:bg-gray-50 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-blue-500"
                >
                  <Eye className="mr-1.5 h-4 w-4" />
                  View Full Profile
                </Link>
              )}
            </div>
            <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
              <div className="space-y-4">
                <div>
                  <label className="text-sm text-gray-600">Full Name</label>
                  <p className="font-medium">{customerProfile.full_name}</p>
                </div>
                <div>
                  <label className="text-sm text-gray-600">Risk Rating</label>
                  <span className={`inline-block px-2 py-1 rounded text-sm font-medium ${
                    customerProfile.risk_rating === 'HIGH' ? 'bg-red-100 text-red-800' :
                    customerProfile.risk_rating === 'MEDIUM' ? 'bg-yellow-100 text-yellow-800' :
                    'bg-green-100 text-green-800'
                  }`}>
                    {customerProfile.risk_rating}
                  </span>
                </div>
                <div>
                  <label className="text-sm text-gray-600">KYC Status</label>
                  <p className="font-medium">{customerProfile.kyc_status}</p>
                </div>
              </div>
              <div className="space-y-4">
                <div>
                  <label className="text-sm text-gray-600">Nationality</label>
                  <p className="font-medium">{customerProfile.nationality}</p>
                </div>
                <div>
                  <label className="text-sm text-gray-600">Residency</label>
                  <p className="font-medium">{customerProfile.residency_country}</p>
                </div>
                <div>
                  <label className="text-sm text-gray-600">PEP Status</label>
                  <span className={`inline-block px-2 py-1 rounded text-sm font-medium ${
                    customerProfile.pep_status ? 'bg-red-100 text-red-800' : 'bg-green-100 text-green-800'
                  }`}>
                    {customerProfile.pep_status ? 'Yes' : 'No'}
                  </span>
                </div>
              </div>
            </div>
            {customerProfile.watchlist_flags.length > 0 && (
              <div className="mt-6 pt-4 border-t">
                <label className="text-sm text-gray-600 mb-2 block">Watchlist Flags</label>
                <div className="flex flex-wrap gap-2">
                  {customerProfile.watchlist_flags.map((flag, index) => (
                    <span key={index} className="px-2 py-1 bg-red-100 text-red-800 rounded text-sm">
                      {flag}
                    </span>
                  ))}
                </div>
              </div>
            )}
          </div>
        </div>
      )}

      {activeTab === 'patterns' && (
        <div className="space-y-6">
          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-lg font-semibold mb-4">Behavior Patterns</h2>
            {behaviorPatterns.length > 0 ? (
              <div className="space-y-4">
                {behaviorPatterns.map((pattern, index) => (
                  <div key={index} className="border rounded-lg p-4">
                    <div className="flex justify-between items-start mb-2">
                      <h3 className="font-medium">{pattern.pattern_type.replace('_', ' ')}</h3>
                      <span className="text-sm text-gray-500">
                        Confidence: {(pattern.confidence_score * 100).toFixed(1)}%
                      </span>
                    </div>
                    <div className="text-sm text-gray-600">
                      <p>Last observed: {new Date(pattern.last_observed).toLocaleString()}</p>
                      <pre className="mt-2 p-2 bg-gray-50 rounded text-xs overflow-auto">
                        {JSON.stringify(pattern.pattern_data, null, 2)}
                      </pre>
                    </div>
                  </div>
                ))}
              </div>
            ) : (
              <p className="text-gray-500 text-center py-8">No behavior patterns detected</p>
            )}
          </div>
        </div>
      )}

      {activeTab === 'related' && (
        <div className="space-y-6">
          <div className="bg-white rounded-lg shadow p-6">
            <h2 className="text-lg font-semibold mb-4">Related Transactions</h2>
            {relatedTransactions.length > 0 ? (
              <div className="space-y-3">
                {relatedTransactions.map((relatedTx) => (
                  <Link
                    key={relatedTx.id}
                    to={`/transactions/${relatedTx.id}`}
                    className="block border rounded-lg p-4 hover:bg-gray-50 transition-colors"
                  >
                    <div className="flex justify-between items-start">
                      <div>
                        <p className="font-medium">{relatedTx.description || 'Transaction'}</p>
                        <p className="text-sm text-gray-600">
                          {relatedTx.from} → {relatedTx.to}
                        </p>
                      </div>
                      <div className="text-right">
                        <p className="font-semibold">{relatedTx.amount.toLocaleString()} {relatedTx.currency}</p>
                        <p className="text-sm text-gray-500">
                          {new Date(relatedTx.timestamp).toLocaleDateString()}
                        </p>
                      </div>
                    </div>
                  </Link>
                ))}
              </div>
            ) : (
              <p className="text-gray-500 text-center py-8">No related transactions found</p>
            )}
          </div>
        </div>
      )}
    </div>
  );
};

export default TransactionDetail;
