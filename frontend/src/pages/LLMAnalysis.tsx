/**
 * LLM Analysis Page
 * AI-powered analysis with real-time streaming responses
 * NO MOCKS - Real LLM integration with SSE streaming
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Sparkles,
  Send,
  StopCircle,
  Trash2,
  MessageSquare,
  Brain,
  Zap,
  Clock,
  DollarSign,
  Database,
  TrendingUp,
  Download
} from 'lucide-react';
import {
  useLLMModels,
  useLLMStream,
  useLLMConversations,
  useCreateLLMConversation,
  useLLMUsageStats
} from '@/hooks/useLLM';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';
import { formatDistanceToNow } from 'date-fns';

const LLMAnalysis: React.FC = () => {
  const [selectedModel, setSelectedModel] = useState<string>('');
  const [prompt, setPrompt] = useState('');
  const [temperature, setTemperature] = useState(0.7);
  const [maxTokens, setMaxTokens] = useState(2000);
  const [systemPrompt, setSystemPrompt] = useState('You are a helpful AI assistant specialized in regulatory compliance and risk analysis.');
  const [currentConversationId, setCurrentConversationId] = useState<string | null>(null);

  const { data: models = [], isLoading: modelsLoading } = useLLMModels();
  const { data: conversations = [] } = useLLMConversations({ limit: 10 });
  const { data: usageStats } = useLLMUsageStats({ timeRange: '7d' });
  const createConversation = useCreateLLMConversation();

  const {
    isStreaming,
    streamedContent,
    error: streamError,
    tokenCount,
    startStream,
    stopStream,
    resetStream
  } = useLLMStream();

  const messagesEndRef = useRef<HTMLDivElement>(null);
  const [messageHistory, setMessageHistory] = useState<Array<{ role: string; content: string }>>([]);

  useEffect(() => {
    if (models.length > 0 && !selectedModel) {
      setSelectedModel(models[0].id);
    }
  }, [models, selectedModel]);

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [streamedContent, messageHistory]);

  const handleSendMessage = async () => {
    if (!prompt.trim() || !selectedModel) return;

    const userMessage = { role: 'user', content: prompt };
    setMessageHistory((prev) => [...prev, userMessage]);

    try {
      let conversationId = currentConversationId;

      if (!conversationId) {
        const newConv = await createConversation.mutateAsync({
          modelId: selectedModel,
          systemPrompt,
          title: prompt.substring(0, 50),
        });
        conversationId = newConv.id;
        setCurrentConversationId(conversationId);
      }

      setPrompt('');
      resetStream();

      await startStream({
        modelId: selectedModel,
        prompt,
        systemPrompt,
        temperature,
        maxTokens,
        onToken: (token) => {
          // Token streaming handled by hook
        },
        onComplete: (fullContent) => {
          setMessageHistory((prev) => [...prev, { role: 'assistant', content: fullContent }]);
        },
        onError: (error) => {
          console.error('Stream error:', error);
        },
      });
    } catch (error) {
      console.error('Failed to send message:', error);
    }
  };

  const handleNewConversation = () => {
    setCurrentConversationId(null);
    setMessageHistory([]);
    resetStream();
  };

  if (modelsLoading) {
    return <LoadingSpinner fullScreen message="Loading LLM models..." />;
  }

  const selectedModelData = models.find((m) => m.id === selectedModel);

  return (
    <div className="h-[calc(100vh-8rem)] flex gap-6">
      {/* Left Sidebar - Conversations & Settings */}
      <div className="w-80 flex flex-col gap-4">
        {/* Model Selection */}
        <div className="bg-white rounded-lg border border-gray-200 p-4">
          <h3 className="text-sm font-semibold text-gray-900 mb-3 flex items-center gap-2">
            <Brain className="w-4 h-4 text-blue-600" />
            AI Model
          </h3>
          <select
            value={selectedModel}
            onChange={(e) => setSelectedModel(e.target.value)}
            className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 text-sm"
          >
            {models.map((model) => (
              <option key={model.id} value={model.id}>
                {model.name} - {model.provider}
              </option>
            ))}
          </select>

          {selectedModelData && (
            <div className="mt-3 space-y-2 text-xs text-gray-600">
              <div className="flex justify-between">
                <span>Context:</span>
                <span className="font-medium">{selectedModelData.contextWindow.toLocaleString()} tokens</span>
              </div>
              <div className="flex justify-between">
                <span>Max Output:</span>
                <span className="font-medium">{selectedModelData.maxOutputTokens.toLocaleString()}</span>
              </div>
              <div className="flex justify-between">
                <span>Status:</span>
                <span className={clsx(
                  'font-medium',
                  selectedModelData.status === 'available' ? 'text-green-600' : 'text-yellow-600'
                )}>
                  {selectedModelData.status}
                </span>
              </div>
            </div>
          )}
        </div>

        {/* Parameters */}
        <div className="bg-white rounded-lg border border-gray-200 p-4">
          <h3 className="text-sm font-semibold text-gray-900 mb-3 flex items-center gap-2">
            <Zap className="w-4 h-4 text-purple-600" />
            Parameters
          </h3>

          <div className="space-y-3">
            <div>
              <label className="text-xs text-gray-600 mb-1 block">
                Temperature: {temperature.toFixed(2)}
              </label>
              <input
                type="range"
                min="0"
                max="2"
                step="0.1"
                value={temperature}
                onChange={(e) => setTemperature(parseFloat(e.target.value))}
                className="w-full"
              />
              <div className="flex justify-between text-xs text-gray-500 mt-1">
                <span>Precise</span>
                <span>Creative</span>
              </div>
            </div>

            <div>
              <label className="text-xs text-gray-600 mb-1 block">
                Max Tokens: {maxTokens}
              </label>
              <input
                type="range"
                min="100"
                max="4000"
                step="100"
                value={maxTokens}
                onChange={(e) => setMaxTokens(parseInt(e.target.value))}
                className="w-full"
              />
            </div>
          </div>
        </div>

        {/* Usage Stats */}
        {usageStats && (
          <div className="bg-white rounded-lg border border-gray-200 p-4">
            <h3 className="text-sm font-semibold text-gray-900 mb-3 flex items-center gap-2">
              <TrendingUp className="w-4 h-4 text-green-600" />
              Usage (7 days)
            </h3>
            <div className="space-y-2 text-xs">
              <div className="flex justify-between">
                <span className="text-gray-600">Total Requests:</span>
                <span className="font-semibold text-gray-900">{usageStats.totalRequests}</span>
              </div>
              <div className="flex justify-between">
                <span className="text-gray-600">Tokens Used:</span>
                <span className="font-semibold text-gray-900">{usageStats.totalTokens.toLocaleString()}</span>
              </div>
              <div className="flex justify-between">
                <span className="text-gray-600 flex items-center gap-1">
                  <DollarSign className="w-3 h-3" />
                  Est. Cost:
                </span>
                <span className="font-semibold text-green-600">${usageStats.totalCost.toFixed(2)}</span>
              </div>
            </div>
          </div>
        )}

        {/* Recent Conversations */}
        <div className="bg-white rounded-lg border border-gray-200 p-4 flex-1 overflow-y-auto">
          <div className="flex items-center justify-between mb-3">
            <h3 className="text-sm font-semibold text-gray-900 flex items-center gap-2">
              <MessageSquare className="w-4 h-4 text-blue-600" />
              Recent
            </h3>
            <button
              onClick={handleNewConversation}
              className="text-xs text-blue-600 hover:text-blue-700"
            >
              New
            </button>
          </div>

          <div className="space-y-2">
            {conversations.map((conv) => (
              <button
                key={conv.id}
                onClick={() => setCurrentConversationId(conv.id)}
                className={clsx(
                  'w-full text-left p-2 rounded-lg border transition-colors',
                  currentConversationId === conv.id
                    ? 'border-blue-500 bg-blue-50'
                    : 'border-gray-200 hover:border-gray-300'
                )}
              >
                <div className="text-xs font-medium text-gray-900 line-clamp-1">{conv.title}</div>
                <div className="text-xs text-gray-500 mt-1">
                  {formatDistanceToNow(new Date(conv.createdAt), { addSuffix: true })}
                </div>
              </button>
            ))}
          </div>
        </div>
      </div>

      {/* Main Chat Area */}
      <div className="flex-1 bg-white rounded-lg border border-gray-200 flex flex-col">
        {/* Header */}
        <div className="p-4 border-b border-gray-200 flex items-center justify-between">
          <div>
            <h1 className="text-xl font-bold text-gray-900 flex items-center gap-2">
              <Sparkles className="w-6 h-6 text-purple-600" />
              LLM Analysis
            </h1>
            <p className="text-sm text-gray-600">Real-time AI streaming responses</p>
          </div>

          {isStreaming && (
            <div className="flex items-center gap-3">
              <div className="flex items-center gap-2 text-sm text-gray-600">
                <Clock className="w-4 h-4" />
                {tokenCount} tokens
              </div>
              <button
                onClick={stopStream}
                className="flex items-center gap-2 px-3 py-1 bg-red-100 text-red-700 rounded-lg hover:bg-red-200 transition-colors text-sm"
              >
                <StopCircle className="w-4 h-4" />
                Stop
              </button>
            </div>
          )}
        </div>

        {/* Messages */}
        <div className="flex-1 overflow-y-auto p-6 space-y-4">
          {messageHistory.length === 0 && !streamedContent && (
            <div className="text-center py-12">
              <Sparkles className="w-16 h-16 text-gray-300 mx-auto mb-4" />
              <h3 className="text-lg font-semibold text-gray-900 mb-2">Start a Conversation</h3>
              <p className="text-gray-600">Ask anything about compliance, risk analysis, or regulations</p>
            </div>
          )}

          {messageHistory.map((msg, idx) => (
            <div
              key={idx}
              className={clsx(
                'flex gap-3',
                msg.role === 'user' ? 'justify-end' : 'justify-start'
              )}
            >
              {msg.role === 'assistant' && (
                <div className="w-8 h-8 rounded-full bg-purple-100 flex items-center justify-center flex-shrink-0">
                  <Brain className="w-4 h-4 text-purple-600" />
                </div>
              )}

              <div
                className={clsx(
                  'max-w-2xl rounded-lg p-4',
                  msg.role === 'user'
                    ? 'bg-blue-600 text-white'
                    : 'bg-gray-100 text-gray-900'
                )}
              >
                <div className="text-sm whitespace-pre-wrap">{msg.content}</div>
              </div>

              {msg.role === 'user' && (
                <div className="w-8 h-8 rounded-full bg-blue-100 flex items-center justify-center flex-shrink-0">
                  <span className="text-blue-600 text-sm font-semibold">U</span>
                </div>
              )}
            </div>
          ))}

          {/* Streaming Response */}
          {isStreaming && streamedContent && (
            <div className="flex gap-3 justify-start">
              <div className="w-8 h-8 rounded-full bg-purple-100 flex items-center justify-center flex-shrink-0">
                <Brain className="w-4 h-4 text-purple-600" />
              </div>
              <div className="max-w-2xl rounded-lg p-4 bg-gray-100 text-gray-900">
                <div className="text-sm whitespace-pre-wrap">{streamedContent}</div>
                <div className="flex items-center gap-2 mt-2 text-xs text-gray-500">
                  <div className="flex items-center gap-1">
                    <div className="w-2 h-2 bg-purple-600 rounded-full animate-pulse" />
                    Streaming...
                  </div>
                </div>
              </div>
            </div>
          )}

          {streamError && (
            <div className="p-4 bg-red-50 border border-red-200 rounded-lg text-sm text-red-700">
              Error: {streamError.message}
            </div>
          )}

          <div ref={messagesEndRef} />
        </div>

        {/* Input Area */}
        <div className="p-4 border-t border-gray-200">
          <div className="flex gap-3">
            <textarea
              value={prompt}
              onChange={(e) => setPrompt(e.target.value)}
              onKeyDown={(e) => {
                if (e.key === 'Enter' && !e.shiftKey) {
                  e.preventDefault();
                  handleSendMessage();
                }
              }}
              className="flex-1 px-4 py-3 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500 resize-none"
              rows={3}
              disabled={isStreaming}
            />
            <button
              onClick={handleSendMessage}
              disabled={!prompt.trim() || isStreaming || !selectedModel}
              className="px-6 py-3 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50 disabled:cursor-not-allowed transition-colors flex items-center gap-2"
            >
              <Send className="w-5 h-5" />
              Send
            </button>
          </div>

          <div className="mt-2 text-xs text-gray-500">
            Streaming enabled • Model: {selectedModelData?.name} • Temperature: {temperature}
          </div>
        </div>
      </div>
    </div>
  );
};

export default LLMAnalysis;
