/**
 * Connection Status Component - Week 3 Phase 4
 * Real-time WebSocket connection indicator
 */

import React, { useState, useEffect } from 'react';
import { Wifi, WifiOff, RefreshCw, AlertCircle } from 'lucide-react';
import useWebSocket from '@/hooks/useWebSocket';

interface ConnectionStatusProps {
  userId?: string;
  showDetails?: boolean;
  className?: string;
}

export const ConnectionStatus: React.FC<ConnectionStatusProps> = ({
  userId,
  showDetails = false,
  className = '',
}) => {
  const { isConnected, isReconnecting, connectionId } = useWebSocket(userId, {
    autoConnect: true,
  });

  const [latency, setLatency] = useState(0);
  const [messageCount, setMessageCount] = useState(0);

  useEffect(() => {
    // Simulate latency measurement
    if (isConnected) {
      const latencyTimer = setInterval(() => {
        setLatency(Math.floor(Math.random() * 50) + 10);
      }, 5000);

      return () => clearInterval(latencyTimer);
    }
  }, [isConnected]);

  const statusColor = isReconnecting
    ? 'text-yellow-600 bg-yellow-50'
    : isConnected
    ? 'text-green-600 bg-green-50'
    : 'text-red-600 bg-red-50';

  const statusBorderColor = isReconnecting
    ? 'border-yellow-200'
    : isConnected
    ? 'border-green-200'
    : 'border-red-200';

  const statusIcon = isReconnecting ? (
    <RefreshCw className="w-4 h-4 animate-spin" />
  ) : isConnected ? (
    <Wifi className="w-4 h-4" />
  ) : (
    <WifiOff className="w-4 h-4" />
  );

  const statusText = isReconnecting
    ? 'Reconnecting...'
    : isConnected
    ? 'Connected'
    : 'Disconnected';

  return (
    <div
      className={`
        px-3 py-2 rounded-lg border flex items-center gap-2
        ${statusColor} ${statusBorderColor} ${className}
      `}
    >
      {statusIcon}
      <span className="text-sm font-medium">{statusText}</span>

      {showDetails && isConnected && (
        <>
          <div className="w-px h-4 bg-current opacity-20" />
          <span className="text-xs opacity-75">{latency}ms</span>
        </>
      )}
    </div>
  );
};

export default ConnectionStatus;
