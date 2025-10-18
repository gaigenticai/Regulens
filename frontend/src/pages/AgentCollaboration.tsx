/**
 * Agent Collaboration Page
 * Real-time multi-agent collaboration with WebSocket
 * NO MOCKS - Real WebSocket communication and session management
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Users,
  MessageSquare,
  Send,
  CheckSquare,
  Plus,
  UserPlus,
  Wifi,
  WifiOff,
  Clock,
  CheckCircle,
  Circle,
  File,
  Vote
} from 'lucide-react';
import {
  useCollaborationSessions,
  useCollaborationSession,
  useCollaborationWebSocket,
  useCollaborationMessages,
  useCollaborationTasks,
  useCollaborationAgents,
  useCreateCollaborationSession,
  useSendCollaborationMessage,
  useCreateCollaborationTask
} from '@/hooks/useCollaboration';
import { LoadingSpinner } from '@/components/LoadingSpinner';
import { clsx } from 'clsx';
import { formatDistanceToNow } from 'date-fns';

const AgentCollaboration: React.FC = () => {
  const [selectedSessionId, setSelectedSessionId] = useState<string | null>(null);
  const [messageInput, setMessageInput] = useState('');
  const [showCreateModal, setShowCreateModal] = useState(false);
  const [showTaskModal, setShowTaskModal] = useState(false);
  const [currentUserId] = useState('user-1'); // From auth context in production

  const { data: sessions = [], isLoading: sessionsLoading } = useCollaborationSessions({ limit: 20 });
  const { data: session } = useCollaborationSession(selectedSessionId || undefined);
  const { data: agents = [] } = useCollaborationAgents(selectedSessionId || undefined);
  const { data: tasks = [] } = useCollaborationTasks(selectedSessionId || undefined);
  const { data: messages = [] } = useCollaborationMessages(selectedSessionId || undefined, { limit: 100 });

  const {
    isConnected,
    messages: wsMessages,
    sendMessage: sendWsMessage,
  } = useCollaborationWebSocket(selectedSessionId || undefined);

  const createSession = useCreateCollaborationSession();
  const sendMessage = useSendCollaborationMessage();
  const createTask = useCreateCollaborationTask();

  const messagesEndRef = useRef<HTMLDivElement>(null);

  const [newSession, setNewSession] = useState({
    title: '',
    description: '',
    agentIds: [] as string[],
    objective: '',
  });

  const [newTask, setNewTask] = useState({
    title: '',
    description: '',
    priority: 'medium' as 'low' | 'medium' | 'high' | 'critical',
  });

  useEffect(() => {
    messagesEndRef.current?.scrollIntoView({ behavior: 'smooth' });
  }, [messages, wsMessages]);

  useEffect(() => {
    if (sessions.length > 0 && !selectedSessionId) {
      setSelectedSessionId(sessions[0].id);
    }
  }, [sessions, selectedSessionId]);

  const handleSendMessage = async () => {
    if (!messageInput.trim() || !selectedSessionId) return;

    try {
      await sendMessage.mutateAsync({
        sessionId: selectedSessionId,
        agentId: currentUserId,
        content: messageInput,
        type: 'text',
      });

      sendWsMessage({
        content: messageInput,
        agentId: currentUserId,
        type: 'text',
      });

      setMessageInput('');
    } catch (error) {
      console.error('Failed to send message:', error);
    }
  };

  const handleCreateSession = async () => {
    if (!newSession.title) return;

    try {
      const created = await createSession.mutateAsync(newSession);
      setSelectedSessionId(created.id);
      setShowCreateModal(false);
      setNewSession({ title: '', description: '', agentIds: [], objective: '' });
    } catch (error) {
      console.error('Failed to create session:', error);
    }
  };

  const handleCreateTask = async () => {
    if (!newTask.title || !selectedSessionId) return;

    try {
      await createTask.mutateAsync({
        sessionId: selectedSessionId,
        ...newTask,
      });
      setShowTaskModal(false);
      setNewTask({ title: '', description: '', priority: 'medium' });
    } catch (error) {
      console.error('Failed to create task:', error);
    }
  };

  const allMessages = [...messages, ...wsMessages].sort(
    (a, b) => new Date(a.timestamp).getTime() - new Date(b.timestamp).getTime()
  );

  if (sessionsLoading) {
    return <LoadingSpinner fullScreen message="Loading collaboration sessions..." />;
  }

  const taskStatusIcons = {
    pending: Circle,
    in_progress: Clock,
    completed: CheckCircle,
    blocked: Circle,
  };

  const priorityColors = {
    low: 'text-blue-600',
    medium: 'text-yellow-600',
    high: 'text-orange-600',
    critical: 'text-red-600',
  };

  return (
    <div className="h-[calc(100vh-8rem)] flex gap-6">
      {/* Left Sidebar - Sessions */}
      <div className="w-80 bg-white rounded-lg border border-gray-200 flex flex-col">
        <div className="p-4 border-b border-gray-200 flex items-center justify-between">
          <h3 className="font-semibold text-gray-900 flex items-center gap-2">
            <Users className="w-5 h-5 text-blue-600" />
            Sessions
          </h3>
          <button
            onClick={() => setShowCreateModal(true)}
            className="p-1 hover:bg-gray-100 rounded transition-colors"
          >
            <Plus className="w-5 h-5 text-gray-600" />
          </button>
        </div>

        <div className="flex-1 overflow-y-auto p-3 space-y-2">
          {sessions.map((sess) => (
            <button
              key={sess.id}
              onClick={() => setSelectedSessionId(sess.id)}
              className={clsx(
                'w-full text-left p-3 rounded-lg border transition-all',
                selectedSessionId === sess.id
                  ? 'border-blue-500 bg-blue-50'
                  : 'border-gray-200 hover:border-gray-300 hover:bg-gray-50'
              )}
            >
              <div className="font-medium text-gray-900 text-sm line-clamp-1">{sess.title}</div>
              <div className="flex items-center gap-2 mt-2 text-xs text-gray-500">
                <Users className="w-3 h-3" />
                {sess.participants.length} participants
              </div>
              <div className="text-xs text-gray-400 mt-1">
                {formatDistanceToNow(new Date(sess.lastActivity), { addSuffix: true })}
              </div>
            </button>
          ))}
        </div>
      </div>

      {/* Main Chat Area */}
      <div className="flex-1 bg-white rounded-lg border border-gray-200 flex flex-col">
        {/* Header */}
        {session && (
          <div className="p-4 border-b border-gray-200">
            <div className="flex items-center justify-between">
              <div>
                <h2 className="text-xl font-bold text-gray-900">{session.title}</h2>
                <p className="text-sm text-gray-600 mt-1">{session.description || session.objective}</p>
              </div>

              <div className="flex items-center gap-3">
                <div
                  className={clsx(
                    'flex items-center gap-2 px-3 py-1 rounded-lg text-sm',
                    isConnected
                      ? 'bg-green-50 text-green-700'
                      : 'bg-gray-50 text-gray-700'
                  )}
                >
                  {isConnected ? (
                    <>
                      <Wifi className="w-4 h-4" />
                      Connected
                    </>
                  ) : (
                    <>
                      <WifiOff className="w-4 h-4" />
                      Polling
                    </>
                  )}
                </div>

                <span className="px-3 py-1 bg-blue-50 text-blue-700 rounded-lg text-sm font-medium">
                  {session.status}
                </span>
              </div>
            </div>
          </div>
        )}

        {/* Messages */}
        <div className="flex-1 overflow-y-auto p-4 space-y-3">
          {allMessages.length === 0 ? (
            <div className="text-center py-12">
              <MessageSquare className="w-16 h-16 text-gray-300 mx-auto mb-4" />
              <h3 className="text-lg font-semibold text-gray-900 mb-2">No Messages Yet</h3>
              <p className="text-gray-600">Start the conversation with your team</p>
            </div>
          ) : (
            allMessages.map((msg, idx) => (
              <div
                key={idx}
                className={clsx(
                  'flex gap-3',
                  msg.agentId === currentUserId ? 'justify-end' : 'justify-start'
                )}
              >
                {msg.agentId !== currentUserId && (
                  <div className="w-8 h-8 rounded-full bg-purple-100 flex items-center justify-center flex-shrink-0">
                    <Users className="w-4 h-4 text-purple-600" />
                  </div>
                )}

                <div
                  className={clsx(
                    'max-w-md rounded-lg p-3',
                    msg.agentId === currentUserId
                      ? 'bg-blue-600 text-white'
                      : 'bg-gray-100 text-gray-900'
                  )}
                >
                  {msg.agentId !== currentUserId && (
                    <div className="text-xs font-medium mb-1 opacity-75">{msg.sender || msg.agentId}</div>
                  )}
                  <div className="text-sm">{msg.content}</div>
                  <div className="text-xs opacity-75 mt-1">
                    {formatDistanceToNow(new Date(msg.timestamp), { addSuffix: true })}
                  </div>
                </div>

                {msg.agentId === currentUserId && (
                  <div className="w-8 h-8 rounded-full bg-blue-100 flex items-center justify-center flex-shrink-0">
                    <span className="text-blue-600 text-sm font-semibold">U</span>
                  </div>
                )}
              </div>
            ))
          )}
          <div ref={messagesEndRef} />
        </div>

        {/* Input */}
        <div className="p-4 border-t border-gray-200">
          <div className="flex gap-3">
            <input
              type="text"
              value={messageInput}
              onChange={(e) => setMessageInput(e.target.value)}
              onKeyDown={(e) => e.key === 'Enter' && handleSendMessage()}
              className="flex-1 px-4 py-2 border border-gray-300 rounded-lg focus:outline-none focus:ring-2 focus:ring-blue-500"
            />
            <button
              onClick={handleSendMessage}
              disabled={!messageInput.trim()}
              className="px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50 flex items-center gap-2"
            >
              <Send className="w-4 h-4" />
              Send
            </button>
          </div>
        </div>
      </div>

      {/* Right Sidebar - Agents & Tasks */}
      <div className="w-80 flex flex-col gap-4">
        {/* Agents */}
        <div className="bg-white rounded-lg border border-gray-200 p-4">
          <h3 className="font-semibold text-gray-900 mb-3 flex items-center gap-2">
            <UserPlus className="w-4 h-4 text-blue-600" />
            Agents ({agents.length})
          </h3>
          <div className="space-y-2">
            {agents.map((agent) => (
              <div key={agent.id} className="flex items-center gap-2 p-2 rounded-lg hover:bg-gray-50">
                <div className={clsx(
                  'w-2 h-2 rounded-full',
                  agent.status === 'online' ? 'bg-green-500' : 'bg-gray-300'
                )} />
                <span className="text-sm text-gray-900">{agent.name}</span>
                <span className="text-xs text-gray-500 ml-auto">{agent.role}</span>
              </div>
            ))}
          </div>
        </div>

        {/* Tasks */}
        <div className="bg-white rounded-lg border border-gray-200 p-4 flex-1 overflow-y-auto">
          <div className="flex items-center justify-between mb-3">
            <h3 className="font-semibold text-gray-900 flex items-center gap-2">
              <CheckSquare className="w-4 h-4 text-green-600" />
              Tasks ({tasks.length})
            </h3>
            <button
              onClick={() => setShowTaskModal(true)}
              className="p-1 hover:bg-gray-100 rounded"
            >
              <Plus className="w-4 h-4 text-gray-600" />
            </button>
          </div>

          <div className="space-y-2">
            {tasks.map((task) => {
              const StatusIcon = taskStatusIcons[task.status];
              return (
                <div key={task.id} className="p-2 border border-gray-200 rounded-lg hover:bg-gray-50">
                  <div className="flex items-start gap-2">
                    <StatusIcon className={clsx('w-4 h-4 mt-0.5', priorityColors[task.priority])} />
                    <div className="flex-1 min-w-0">
                      <div className="text-sm font-medium text-gray-900 line-clamp-2">{task.title}</div>
                      <div className="flex items-center gap-2 mt-1 text-xs text-gray-500">
                        <span className="capitalize">{task.status.replace('_', ' ')}</span>
                        {task.progress > 0 && <span>• {task.progress}%</span>}
                      </div>
                    </div>
                  </div>
                </div>
              );
            })}
          </div>
        </div>
      </div>

      {/* Create Session Modal */}
      {showCreateModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center p-4 z-50">
          <div className="bg-white rounded-lg max-w-lg w-full p-6">
            <h2 className="text-xl font-bold mb-4">Create Collaboration Session</h2>
            <div className="space-y-4">
              <input
                type="text"
                value={newSession.title}
                onChange={(e) => setNewSession({ ...newSession, title: e.target.value })}
                className="w-full px-3 py-2 border rounded-lg"
              />
              <textarea
                value={newSession.description}
                onChange={(e) => setNewSession({ ...newSession, description: e.target.value })}
                rows={3}
                className="w-full px-3 py-2 border rounded-lg"
              />
              <input
                type="text"
                value={newSession.objective}
                onChange={(e) => setNewSession({ ...newSession, objective: e.target.value })}
                className="w-full px-3 py-2 border rounded-lg"
              />
            </div>
            <div className="flex justify-end gap-3 mt-6">
              <button onClick={() => setShowCreateModal(false)} className="px-4 py-2 border rounded-lg">
                Cancel
              </button>
              <button
                onClick={handleCreateSession}
                disabled={!newSession.title}
                className="px-4 py-2 bg-blue-600 text-white rounded-lg disabled:opacity-50"
              >
                Create
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Create Task Modal */}
      {showTaskModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center p-4 z-50">
          <div className="bg-white rounded-lg max-w-lg w-full p-6">
            <h2 className="text-xl font-bold mb-4">Create Task</h2>
            <div className="space-y-4">
              <input
                type="text"
                value={newTask.title}
                onChange={(e) => setNewTask({ ...newTask, title: e.target.value })}
                className="w-full px-3 py-2 border rounded-lg"
              />
              <textarea
                value={newTask.description}
                onChange={(e) => setNewTask({ ...newTask, description: e.target.value })}
                rows={3}
                className="w-full px-3 py-2 border rounded-lg"
              />
              <select
                value={newTask.priority}
                onChange={(e) => setNewTask({ ...newTask, priority: e.target.value as any })}
                className="w-full px-3 py-2 border rounded-lg"
              >
                <option value="low">Low Priority</option>
                <option value="medium">Medium Priority</option>
                <option value="high">High Priority</option>
                <option value="critical">Critical Priority</option>
              </select>
            </div>
            <div className="flex justify-end gap-3 mt-6">
              <button onClick={() => setShowTaskModal(false)} className="px-4 py-2 border rounded-lg">
                Cancel
              </button>
              <button
                onClick={handleCreateTask}
                disabled={!newTask.title}
                className="px-4 py-2 bg-blue-600 text-white rounded-lg disabled:opacity-50"
              >
                Create
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
};

export default AgentCollaboration;
