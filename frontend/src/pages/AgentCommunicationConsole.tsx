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

interface ErrorState {
  hasError: boolean;
  message: string;
  code?: string;
  timestamp: number;
}

export default function AgentCommunicationConsole() {
  const [messages, setMessages] = useState<AgentMessage[]>([]);
  const [consensus, setConsensus] = useState<ConsensusData[]>([]);
  const [activeTab, setActiveTab] = useState<'messages' | 'consensus' | 'send'>('messages');
  const [autoRefresh, setAutoRefresh] = useState(true);
  const [error, setError] = useState<ErrorState | null>(null);

  // Form state for sending messages
  const [fromAgent, setFromAgent] = useState('test_agent_1');
  const [toAgent, setToAgent] = useState('test_agent_2');
  const [messageType, setMessageType] = useState('TASK_ASSIGNMENT');
  const [messageContent, setMessageContent] = useState('{"task": "Process compliance check", "priority": "high"}');
  const [priority, setPriority] = useState(3);
  const [sending, setSending] = useState(false);

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
    setError(null); // Clear previous errors

    try {
      // For demo purposes, we'll load messages for a test agent
      const response = await fetch('/api/agents/message/receive?agent_id=test_agent&limit=50');
      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      if (data.success) {
        // Transform the data to match the expected format
        const transformedMessages = data.messages.map((msg: any) => ({
          message_id: msg.message_id,
          from_agent: msg.from_agent,
          to_agent: msg.to_agent,
          message_type: msg.message_type,
          content: typeof msg.content === 'object' ? JSON.stringify(msg.content) : msg.content,
          timestamp: msg.created_at || new Date().toISOString(),
          status: msg.status
        }));
        setMessages(transformedMessages);
      } else {
        throw new Error(data.error || 'Failed to load messages');
      }
    } catch (error) {
      console.error('Failed to load messages:', error);

      // Set proper error state
      setError({
        hasError: true,
        message: error instanceof Error ? error.message : 'Unknown error occurred',
        timestamp: Date.now()
      });

      // Clear messages on error
      setMessages([]);
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

  const sendMessage = async () => {
    setSending(true);
    try {
      let content;
      try {
        content = JSON.parse(messageContent);
      } catch (e) {
        content = messageContent; // Send as string if not valid JSON
      }

      const response = await fetch('/api/agents/message/send', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          from_agent: fromAgent,
          to_agent: toAgent,
          message_type: messageType,
          content: content,
          priority: priority
        })
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      if (data.success) {
        alert('Message sent successfully! ID: ' + data.message_id);
        // Clear form
        setMessageContent('{"task": "Process compliance check", "priority": "high"}');
        // Refresh messages
        loadMessages();
      } else {
        throw new Error(data.error || 'Failed to send message');
      }
    } catch (error) {
      console.error('Failed to send message:', error);
      alert('Failed to send message: ' + (error instanceof Error ? error.message : 'Unknown error'));
    } finally {
      setSending(false);
    }
  };

  const acknowledgeMessage = async (messageId: string) => {
    try {
      const response = await fetch('/api/agents/message/acknowledge', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          message_id: messageId,
          agent_id: fromAgent // Use the current "from agent" as the acknowledging agent
        })
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      if (data.success) {
        alert('Message acknowledged successfully!');
        loadMessages(); // Refresh messages
      } else {
        throw new Error(data.error || 'Failed to acknowledge message');
      }
    } catch (error) {
      console.error('Failed to acknowledge message:', error);
      alert('Failed to acknowledge message: ' + (error instanceof Error ? error.message : 'Unknown error'));
    }
  };

  const broadcastMessage = async () => {
    setSending(true);
    try {
      let content;
      try {
        content = JSON.parse(messageContent);
      } catch (e) {
        content = messageContent;
      }

      const response = await fetch('/api/agents/message/broadcast', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
        },
        body: JSON.stringify({
          from_agent: fromAgent,
          message_type: messageType,
          content: content,
          priority: priority
        })
      });

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      if (data.success) {
        alert('Broadcast message sent successfully!');
        setMessageContent('{"task": "Process compliance check", "priority": "high"}');
        loadMessages();
      } else {
        throw new Error(data.error || 'Failed to broadcast message');
      }
    } catch (error) {
      console.error('Failed to broadcast message:', error);
      alert('Failed to broadcast message: ' + (error instanceof Error ? error.message : 'Unknown error'));
    } finally {
      setSending(false);
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
          {['messages', 'send', 'consensus'].map((tab) => (
            <button
              key={tab}
              onClick={() => setActiveTab(tab as any)}
              className={`py-4 px-1 border-b-2 font-medium text-sm capitalize ${
                activeTab === tab
                  ? 'border-blue-500 text-blue-600'
                  : 'border-transparent text-gray-500 hover:text-gray-700 hover:border-gray-300'
              }`}
            >
              {tab === 'send' ? 'Send Message' : tab}
            </button>
          ))}
        </nav>
      </div>

      {/* Error Banner */}
      {error && (
        <div className="bg-red-50 border border-red-200 rounded-lg p-4 mb-6">
          <div className="flex items-start">
            <svg className="w-5 h-5 text-red-600 mt-0.5 mr-3" fill="currentColor" viewBox="0 0 20 20">
              <path fillRule="evenodd" d="M10 18a8 8 0 100-16 8 8 0 000 16zM8.707 7.293a1 1 0 00-1.414 1.414L8.586 10l-1.293 1.293a1 1 0 101.414 1.414L10 11.414l1.293 1.293a1 1 0 001.414-1.414L11.414 10l1.293-1.293a1 1 0 00-1.414-1.414L10 8.586 8.707 7.293z" clipRule="evenodd" />
            </svg>
            <div>
              <h3 className="text-sm font-medium text-red-800">
                Failed to load communication data
              </h3>
              <p className="text-sm text-red-700 mt-1">
                {error.message}
              </p>
              <button
                onClick={() => { setError(null); loadMessages(); loadConsensus(); }}
                className="mt-2 text-sm text-red-600 hover:text-red-800 font-medium"
              >
                Try Again
              </button>
            </div>
          </div>
        </div>
      )}

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
                <div className="flex items-center space-x-2">
                  <span className={`px-2 py-1 text-xs rounded ${getMessageColor(msg.message_type)}`}>
                    {msg.message_type.replace('_', ' ')}
                  </span>
                  {msg.status === 'delivered' && (
                    <button
                      onClick={() => acknowledgeMessage(msg.message_id)}
                      className="px-3 py-1 bg-green-600 text-white text-xs rounded hover:bg-green-700"
                    >
                      Acknowledge
                    </button>
                  )}
                </div>
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

      {activeTab === 'send' && (
        <div className="bg-white p-6 rounded-lg shadow border border-gray-200">
          <h2 className="text-xl font-semibold text-gray-900 mb-4">Send Inter-Agent Message</h2>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mb-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                From Agent
              </label>
              <input
                type="text"
                value={fromAgent}
                onChange={(e) => setFromAgent(e.target.value)}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                placeholder="e.g., regulatory_assessor"
              />
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                To Agent
              </label>
              <input
                type="text"
                value={toAgent}
                onChange={(e) => setToAgent(e.target.value)}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
                placeholder="e.g., audit_intelligence (leave empty for broadcast)"
              />
            </div>
          </div>

          <div className="grid grid-cols-1 md:grid-cols-2 gap-4 mb-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Message Type
              </label>
              <select
                value={messageType}
                onChange={(e) => setMessageType(e.target.value)}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              >
                <option value="TASK_ASSIGNMENT">Task Assignment</option>
                <option value="COMPLIANCE_CHECK">Compliance Check</option>
                <option value="RISK_ALERT">Risk Alert</option>
                <option value="COLLABORATION_REQUEST">Collaboration Request</option>
                <option value="STATUS_UPDATE">Status Update</option>
                <option value="DATA_REQUEST">Data Request</option>
                <option value="ACKNOWLEDGMENT">Acknowledgment</option>
              </select>
            </div>

            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Priority (1-5, 1=highest)
              </label>
              <input
                type="number"
                min="1"
                max="5"
                value={priority}
                onChange={(e) => setPriority(parseInt(e.target.value) || 3)}
                className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              />
            </div>
          </div>

          <div className="mb-4">
            <label className="block text-sm font-medium text-gray-700 mb-1">
              Message Content (JSON)
            </label>
            <textarea
              value={messageContent}
              onChange={(e) => setMessageContent(e.target.value)}
              rows={4}
              className="w-full px-3 py-2 border border-gray-300 rounded-md focus:outline-none focus:ring-2 focus:ring-blue-500"
              placeholder='{"task": "Process compliance check", "priority": "high"}'
            />
          </div>

          <div className="flex space-x-4">
            <button
              onClick={sendMessage}
              disabled={sending}
              className="px-6 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed"
            >
              {sending ? 'Sending...' : 'Send Message'}
            </button>

            <button
              onClick={broadcastMessage}
              disabled={sending}
              className="px-6 py-2 bg-purple-600 text-white rounded-lg hover:bg-purple-700 disabled:opacity-50 disabled:cursor-not-allowed"
            >
              {sending ? 'Broadcasting...' : 'Broadcast to All'}
            </button>
          </div>
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

