/**
 * Agent Communication Console
 * Real-time agent message viewer and consensus visualization
 * Backend routes: 8 agent communication endpoints
 */

import { useState, useEffect } from 'react';

interface AgentMessage {
  message_id: string;
  from_agent: string;
  to_agent: string;
  message_type: string;
  content: string;
  timestamp: string;
  status: 'sent' | 'received' | 'processed';
}

interface ConsensusData {
  topic: string;
  participants: string[];
  agreement_level: number;
  final_decision: string;
  timestamp: string;
}

export default function AgentCommunicationConsole() {
  const [messages, setMessages] = useState<AgentMessage[]>([]);
  const [consensus, setConsensus] = useState<ConsensusData[]>([]);
  const [activeTab, setActiveTab] = useState<'messages' | 'consensus'>('messages');
  const [autoRefresh, setAutoRefresh] = useState(true);

  useEffect(() => {
    loadMessages();
    loadConsensus();

    if (autoRefresh) {
      const interval = setInterval(() => {
        loadMessages();
        loadConsensus();
      }, 5000);
      return () => clearInterval(interval);
    }
  }, [autoRefresh]);

  const loadMessages = async () => {
    try {
      const response = await fetch('/api/communication?limit=50');
      if (response.ok) {
        const data = await response.json();
        setMessages(data.messages || data.events || []);
      }
    } catch (error) {
      console.error('Failed to load messages:', error);
      // Sample data for development
      setMessages([
        {
          message_id: 'msg_001',
          from_agent: 'RegulatoryAssessor',
          to_agent: 'TransactionGuardian',
          message_type: 'COMPLIANCE_CHECK',
          content: 'Transaction requires additional compliance verification for GDPR',
          timestamp: new Date().toISOString(),
          status: 'processed'
        },
        {
          message_id: 'msg_002',
          from_agent: 'TransactionGuardian',
          to_agent: 'RegulatoryAssessor',
          message_type: 'ACKNOWLEDGMENT',
          content: 'Compliance check acknowledged, initiating verification',
          timestamp: new Date(Date.now() - 5000).toISOString(),
          status: 'received'
        }
      ]);
    }
  };

  const loadConsensus = async () => {
    try {
      const response = await fetch('/api/communication/consensus');
      if (response.ok) {
        const data = await response.json();
        setConsensus(data.consensus || []);
      }
    } catch (error) {
      console.error('Failed to load consensus:', error);
      setConsensus([
        {
          topic: 'High-value transaction approval',
          participants: ['RegulatoryAssessor', 'TransactionGuardian', 'AuditIntelligence'],
          agreement_level: 0.89,
          final_decision: 'APPROVE with enhanced monitoring',
          timestamp: new Date().toISOString()
        }
      ]);
    }
  };

  const getMessageColor = (type: string) => {
    switch (type) {
      case 'COMPLIANCE_CHECK':
        return 'bg-blue-100 text-blue-800';
      case 'RISK_ALERT':
        return 'bg-red-100 text-red-800';
      case 'ACKNOWLEDGMENT':
        return 'bg-green-100 text-green-800';
      case 'COLLABORATION_REQUEST':
        return 'bg-purple-100 text-purple-800';
      default:
        return 'bg-gray-100 text-gray-800';
    }
  };

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'sent':
        return 'text-blue-600';
      case 'received':
        return 'text-yellow-600';
      case 'processed':
        return 'text-green-600';
      default:
        return 'text-gray-600';
    }
  };

  return (
    <div className="p-6 max-w-7xl mx-auto">
      <div className="mb-6 flex justify-between items-center">
        <div>
          <h1 className="text-3xl font-bold text-gray-900">Agent Communication Console</h1>
          <p className="text-gray-600 mt-2">
            Real-time multi-agent communication and consensus tracking
          </p>
        </div>
        <div className="flex items-center space-x-4">
          <label className="flex items-center cursor-pointer">
            <input
              type="checkbox"
              checked={autoRefresh}
              onChange={(e) => setAutoRefresh(e.target.checked)}
              className="mr-2"
            />
            <span className="text-sm text-gray-700">Auto-refresh</span>
          </label>
          <button
            onClick={() => { loadMessages(); loadConsensus(); }}
            className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
          >
            Refresh Now
          </button>
        </div>
      </div>

      {/* Tab Navigation */}
      <div className="border-b border-gray-200 mb-6">
        <nav className="-mb-px flex space-x-8">
          {['messages', 'consensus'].map((tab) => (
            <button
              key={tab}
              onClick={() => setActiveTab(tab as any)}
              className={`py-4 px-1 border-b-2 font-medium text-sm capitalize ${
                activeTab === tab
                  ? 'border-blue-500 text-blue-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
            >
              {tab}
            </button>
          ))}
        </nav>
      </div>

      {activeTab === 'messages' && (
        <div className="space-y-3">
          <div className="text-sm text-gray-600 mb-4">
            Showing {messages.length} recent messages
          </div>
          {messages.map((msg) => (
            <div key={msg.message_id} className="bg-white p-4 rounded-lg shadow border border-gray-200">
              <div className="flex justify-between items-start mb-2">
                <div className="flex items-center space-x-2">
                  <span className="font-semibold text-gray-900">{msg.from_agent}</span>
                  <svg className="w-4 h-4 text-gray-400" fill="currentColor" viewBox="0 0 20 20">
                    <path fillRule="evenodd" d="M10.293 3.293a1 1 0 011.414 0l6 6a1 1 0 010 1.414l-6 6a1 1 0 01-1.414-1.414L14.586 11H3a1 1 0 110-2h11.586l-4.293-4.293a1 1 0 010-1.414z" clipRule="evenodd" />
                  </svg>
                  <span className="font-semibold text-gray-900">{msg.to_agent}</span>
                </div>
                <span className={`px-2 py-1 text-xs rounded ${getMessageColor(msg.message_type)}`}>
                  {msg.message_type.replace('_', ' ')}
                </span>
              </div>
              <p className="text-gray-700 mb-2">{msg.content}</p>
              <div className="flex justify-between items-center text-sm">
                <span className="text-gray-500">{new Date(msg.timestamp).toLocaleString()}</span>
                <span className={`font-semibold capitalize ${getStatusColor(msg.status)}`}>
                  {msg.status}
                </span>
              </div>
            </div>
          ))}
          {messages.length === 0 && (
            <div className="text-center py-12 bg-gray-50 rounded-lg">
              <p className="text-gray-500">No messages found</p>
            </div>
          )}
        </div>
      )}

      {activeTab === 'consensus' && (
        <div className="space-y-4">
          {consensus.map((item, index) => (
            <div key={index} className="bg-white p-6 rounded-lg shadow border border-gray-200">
              <div className="mb-4">
                <h3 className="text-lg font-semibold text-gray-900 mb-2">{item.topic}</h3>
                <div className="flex items-center space-x-2 mb-3">
                  <span className="text-sm text-gray-600">Participants:</span>
                  {item.participants.map((agent, idx) => (
                    <span key={idx} className="px-2 py-1 bg-blue-100 text-blue-800 text-xs rounded">
                      {agent}
                    </span>
                  ))}
                </div>
              </div>
              <div className="mb-4">
                <div className="flex justify-between items-center mb-2">
                  <span className="text-sm font-medium text-gray-700">Agreement Level</span>
                  <span className="text-lg font-bold text-green-600">
                    {(item.agreement_level * 100).toFixed(0)}%
                  </span>
                </div>
                <div className="w-full bg-gray-200 rounded-full h-2">
                  <div
                    className="bg-green-600 h-2 rounded-full"
                    style={{ width: `${item.agreement_level * 100}%` }}
                  ></div>
                </div>
              </div>
              <div className="bg-green-50 p-3 rounded-lg">
                <span className="text-sm font-semibold text-gray-700">Final Decision: </span>
                <span className="text-sm text-gray-900">{item.final_decision}</span>
              </div>
              <div className="mt-3 text-xs text-gray-500">
                {new Date(item.timestamp).toLocaleString()}
              </div>
            </div>
          ))}
          {consensus.length === 0 && (
            <div className="text-center py-12 bg-gray-50 rounded-lg">
              <p className="text-gray-500">No consensus decisions found</p>
            </div>
          )}
        </div>
      )}
    </div>
  );
}

