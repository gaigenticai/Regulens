/**
 * Realtime Notifications Component - Week 3 Phase 4
 * Toast-style notifications for WebSocket events
 */

import React, { useState, useCallback, useEffect } from 'react';
import { AlertCircle, CheckCircle, Info, X } from 'lucide-react';
import useWebSocket from '@/hooks/useWebSocket';
import { WebSocketMessage } from '@/services/websocket';

interface Notification {
  id: string;
  type: 'alert' | 'success' | 'info' | 'error';
  title: string;
  message: string;
  timestamp: number;
}

interface RealtimeNotificationsProps {
  userId?: string;
  maxNotifications?: number;
}

export const RealtimeNotifications: React.FC<RealtimeNotificationsProps> = ({
  userId,
  maxNotifications = 5,
}) => {
  const [notifications, setNotifications] = useState<Notification[]>([]);
  const { on, off } = useWebSocket(userId, { autoConnect: true });

  const addNotification = useCallback((notification: Omit<Notification, 'id' | 'timestamp'>) => {
    const id = `${Date.now()}-${Math.random()}`;
    const newNotification: Notification = {
      ...notification,
      id,
      timestamp: Date.now(),
    };

    setNotifications((prev) => {
      const updated = [newNotification, ...prev];
      return updated.slice(0, maxNotifications);
    });

    // Auto-dismiss after 5 seconds
    setTimeout(() => {
      removeNotification(id);
    }, 5000);
  }, [maxNotifications]);

  const removeNotification = useCallback((id: string) => {
    setNotifications((prev) => prev.filter((n) => n.id !== id));
  }, []);

  // Subscribe to ALERT messages
  useEffect(() => {
    const unsubscribe = on('ALERT', (message: WebSocketMessage) => {
      const alertData = message.payload.alert || {};
      addNotification({
        type: 'alert',
        title: alertData.alert_type || 'Alert',
        message: alertData.message || 'New alert received',
      });
    });

    return unsubscribe;
  }, [on, addNotification]);

  // Subscribe to CONSENSUS_UPDATE messages
  useEffect(() => {
    const unsubscribe = on('CONSENSUS_UPDATE', (message: WebSocketMessage) => {
      const consensusData = message.payload.consensus || {};
      if (consensusData.event === 'consensus_result') {
        addNotification({
          type: 'success',
          title: 'Consensus Reached',
          message: `Consensus finalized: ${consensusData.result?.decision || 'Decision made'}`,
        });
      }
    });

    return unsubscribe;
  }, [on, addNotification]);

  // Subscribe to RULE_EVALUATION_RESULT messages
  useEffect(() => {
    const unsubscribe = on('RULE_EVALUATION_RESULT', (message: WebSocketMessage) => {
      const evalData = message.payload.evaluation || {};
      addNotification({
        type: 'info',
        title: 'Rule Evaluated',
        message: `Rule ${evalData.rule_id}: ${evalData.result?.status || 'Evaluated'}`,
      });
    });

    return unsubscribe;
  }, [on, addNotification]);

  // Subscribe to DECISION_ANALYSIS_RESULT messages
  useEffect(() => {
    const unsubscribe = on('DECISION_ANALYSIS_RESULT', (message: WebSocketMessage) => {
      const decisionData = message.payload.decision || {};
      addNotification({
        type: 'success',
        title: 'Decision Analysis Complete',
        message: `Analysis ${decisionData.analysis_id}: Ready`,
      });
    });

    return unsubscribe;
  }, [on, addNotification]);

  const getNotificationIcon = (type: Notification['type']) => {
    switch (type) {
      case 'alert':
        return <AlertCircle className="w-5 h-5" />;
      case 'success':
        return <CheckCircle className="w-5 h-5" />;
      case 'error':
        return <AlertCircle className="w-5 h-5" />;
      default:
        return <Info className="w-5 h-5" />;
    }
  };

  const getNotificationColor = (type: Notification['type']) => {
    switch (type) {
      case 'alert':
        return 'bg-red-50 border-red-200 text-red-800';
      case 'success':
        return 'bg-green-50 border-green-200 text-green-800';
      case 'error':
        return 'bg-red-50 border-red-200 text-red-800';
      default:
        return 'bg-blue-50 border-blue-200 text-blue-800';
    }
  };

  const getIconColor = (type: Notification['type']) => {
    switch (type) {
      case 'alert':
        return 'text-red-600';
      case 'success':
        return 'text-green-600';
      case 'error':
        return 'text-red-600';
      default:
        return 'text-blue-600';
    }
  };

  return (
    <div className="fixed bottom-4 right-4 space-y-2 z-50 max-w-md">
      {notifications.map((notification) => (
        <div
          key={notification.id}
          className={`
            p-4 rounded-lg border shadow-lg flex items-start gap-3
            animate-slide-in-right transition-all
            ${getNotificationColor(notification.type)}
          `}
        >
          <div className={`flex-shrink-0 mt-0.5 ${getIconColor(notification.type)}`}>
            {getNotificationIcon(notification.type)}
          </div>

          <div className="flex-1 min-w-0">
            <h3 className="font-semibold text-sm">{notification.title}</h3>
            <p className="text-sm opacity-90 mt-0.5 truncate">{notification.message}</p>
          </div>

          <button
            onClick={() => removeNotification(notification.id)}
            className="flex-shrink-0 mt-0.5 opacity-50 hover:opacity-100 transition-opacity"
          >
            <X className="w-4 h-4" />
          </button>
        </div>
      ))}
    </div>
  );
};

export default RealtimeNotifications;
