import React, { useState } from 'react';
import { MessageCircle, Send } from 'lucide-react';
import { useChatbotConversations, useSendMessage } from '@/hooks/useChatbot';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const RegulatoryChatbot: React.FC = () => {
  const [message, setMessage] = useState('');
  const [chatHistory, setChatHistory] = useState<Array<{role: string; text: string}>>([]);
  const { data: conversations = [] } = useChatbotConversations();
  const sendMessage = useSendMessage();

  const handleSend = async () => {
    if (!message.trim()) return;
    
    const userMessage = message;
    setChatHistory(prev => [...prev, { role: 'user', text: userMessage }]);
    setMessage('');
    
    const response = await sendMessage.mutateAsync({ message: userMessage });
    setChatHistory(prev => [...prev, { role: 'bot', text: response.response }]);
  };

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">Regulatory Chatbot</h1>
        <p className="text-gray-600 mt-1">Instant knowledge base access powered by GPT-4</p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-3 gap-6">
        <div className="lg:col-span-2">
          <div className="bg-white rounded-lg shadow-sm border border-gray-200 h-[600px] flex flex-col">
            <div className="p-4 border-b border-gray-200 bg-blue-50">
              <h2 className="text-lg font-semibold text-gray-900">Chat with Regulatory Expert</h2>
            </div>
            
            <div className="flex-1 overflow-y-auto p-4 space-y-4">
              {chatHistory.length === 0 ? (
                <div className="text-center py-12">
                  <MessageCircle className="w-12 h-12 text-gray-400 mx-auto mb-4" />
                  <h3 className="text-lg font-medium text-gray-900 mb-2">Start a conversation</h3>
                  <p className="text-gray-600">Ask me anything about regulatory compliance</p>
                </div>
              ) : (
                chatHistory.map((msg, idx) => (
                  <div key={idx} className={`flex ${msg.role === 'user' ? 'justify-end' : 'justify-start'}`}>
                    <div className={`max-w-[80%] rounded-lg px-4 py-2 ${
                      msg.role === 'user' 
                        ? 'bg-blue-600 text-white' 
                        : 'bg-gray-100 text-gray-900'
                    }`}>
                      <p className="text-sm">{msg.text}</p>
                    </div>
                  </div>
                ))
              )}
              {sendMessage.isPending && <LoadingSpinner message="Thinking..." />}
            </div>

            <div className="p-4 border-t border-gray-200">
              <div className="flex gap-2">
                <input
                  type="text"
                  value={message}
                  onChange={(e) => setMessage(e.target.value)}
                  onKeyPress={(e) => e.key === 'Enter' && handleSend()}
                  className="flex-1 px-4 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                />
                <button
                  onClick={handleSend}
                  disabled={!message.trim() || sendMessage.isPending}
                  className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed"
                >
                  <Send className="w-5 h-5" />
                </button>
              </div>
            </div>
          </div>
        </div>

        <div className="lg:col-span-1">
          <div className="bg-white rounded-lg shadow-sm border border-gray-200">
            <div className="p-4 border-b border-gray-200">
              <h2 className="text-lg font-semibold text-gray-900">Recent Conversations</h2>
            </div>
            <div className="divide-y divide-gray-200 max-h-[540px] overflow-y-auto">
              {conversations.length === 0 ? (
                <div className="p-8 text-center">
                  <p className="text-gray-600 text-sm">No previous conversations</p>
                </div>
              ) : (
                conversations.map((conv) => (
                  <div key={conv.conversation_id} className="p-4 hover:bg-gray-50 cursor-pointer">
                    <div className="flex items-start justify-between">
                      <div>
                        <h3 className="text-sm font-medium text-gray-900">{conv.platform}</h3>
                        <p className="text-xs text-gray-600 mt-1">{conv.message_count} messages</p>
                        <p className="text-xs text-gray-500 mt-1">
                          {new Date(conv.started_at).toLocaleDateString()}
                        </p>
                      </div>
                      <span className={`px-2 py-1 text-xs rounded-full ${
                        conv.is_active ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-800'
                      }`}>
                        {conv.is_active ? 'Active' : 'Closed'}
                      </span>
                    </div>
                  </div>
                ))
              )}
            </div>
          </div>
        </div>
      </div>
    </div>
  );
};

export default RegulatoryChatbot;

