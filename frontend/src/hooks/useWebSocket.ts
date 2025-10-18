/**
 * useWebSocket Hook - Week 3 Phase 3
 * React hook for managing WebSocket connections with automatic cleanup
 */

import { useEffect, useState, useCallback, useRef } from 'react';
import { websocketClient, WebSocketMessage, MessageType } from '@/services/websocket';

export interface UseWebSocketOptions {
  autoConnect?: boolean;
  channels?: string[];
  onMessage?: (message: WebSocketMessage) => void;
}

export interface UseWebSocketReturn {
  isConnected: boolean;
  isReconnecting: boolean;
  connectionId: string;
  connect: (userId: string) => Promise<void>;
  disconnect: () => void;
  send: (message: Partial<WebSocketMessage>) => void;
  subscribe: (channel: string) => void;
  unsubscribe: (channel: string) => void;
  broadcast: (payload: Record<string, any>) => void;
  sendDirect: (recipientId: string, payload: Record<string, any>) => void;
  on: (event: MessageType | 'connected' | 'disconnected' | 'reconnecting' | 'error' | 'message', handler: (data?: any) => void) => () => void;
  off: (event: MessageType | 'connected' | 'disconnected' | 'reconnecting' | 'error' | 'message', handler: (data?: any) => void) => void;
}

export function useWebSocket(userId: string | undefined, options: UseWebSocketOptions = {}): UseWebSocketReturn {
  const [isConnected, setIsConnected] = useState(false);
  const [isReconnecting, setIsReconnecting] = useState(false);
  const [connectionId, setConnectionId] = useState('');
  const handlersRef = useRef<Map<string, Set<Function>>>(new Map());

  // Handle connection
  const handleConnected = useCallback(() => {
    setIsConnected(true);
    setIsReconnecting(false);
    setConnectionId(websocketClient.getConnectionId());
  }, []);

  // Handle disconnection
  const handleDisconnected = useCallback(() => {
    setIsConnected(false);
  }, []);

  // Handle reconnecting
  const handleReconnecting = useCallback(() => {
    setIsReconnecting(true);
  }, []);

  // Handle incoming messages
  const handleMessage = useCallback((message: WebSocketMessage) => {
    if (options.onMessage) {
      options.onMessage(message);
    }

    // Call type-specific handlers
    const handlers = handlersRef.current.get(message.type);
    if (handlers) {
      handlers.forEach((handler) => handler(message));
    }
  }, [options]);

  // Handle errors
  const handleError = useCallback((error: Error) => {
    console.error('WebSocket error:', error);

    const handlers = handlersRef.current.get('error');
    if (handlers) {
      handlers.forEach((handler) => handler(error));
    }
  }, []);

  // Auto-connect on mount
  useEffect(() => {
    if (!userId || !options.autoConnect) return;

    websocketClient.connect(userId).catch((error) => {
      console.error('Failed to connect WebSocket:', error);
    });

    return () => {
      // Don't disconnect on unmount - keep connection alive for app-wide use
    };
  }, [userId, options.autoConnect]);

  // Subscribe to channels on mount
  useEffect(() => {
    if (!isConnected || !options.channels) return;

    options.channels.forEach((channel) => {
      websocketClient.subscribe(channel);
    });

    return () => {
      options.channels?.forEach((channel) => {
        websocketClient.unsubscribe(channel);
      });
    };
  }, [isConnected, options.channels]);

  // Register event listeners
  useEffect(() => {
    websocketClient.on('connected', handleConnected);
    websocketClient.on('disconnected', handleDisconnected);
    websocketClient.on('reconnecting', handleReconnecting);
    websocketClient.on('message', handleMessage);
    websocketClient.on('error', handleError);

    return () => {
      websocketClient.off('connected', handleConnected);
      websocketClient.off('disconnected', handleDisconnected);
      websocketClient.off('reconnecting', handleReconnecting);
      websocketClient.off('message', handleMessage);
      websocketClient.off('error', handleError);
    };
  }, [handleConnected, handleDisconnected, handleReconnecting, handleMessage, handleError]);

  // Manual connect function
  const connect = useCallback(async (uid: string) => {
    await websocketClient.connect(uid);
  }, []);

  // Manual disconnect function
  const disconnect = useCallback(() => {
    websocketClient.disconnect();
  }, []);

  // Send message
  const send = useCallback((message: Partial<WebSocketMessage>) => {
    websocketClient.send(message);
  }, []);

  // Subscribe to channel
  const subscribe = useCallback((channel: string) => {
    websocketClient.subscribe(channel);
  }, []);

  // Unsubscribe from channel
  const unsubscribe = useCallback((channel: string) => {
    websocketClient.unsubscribe(channel);
  }, []);

  // Broadcast message
  const broadcast = useCallback((payload: Record<string, any>) => {
    websocketClient.broadcast(payload);
  }, []);

  // Send direct message
  const sendDirect = useCallback((recipientId: string, payload: Record<string, any>) => {
    websocketClient.sendDirect(recipientId, payload);
  }, []);

  // Register event handler
  const on = useCallback((event: string, handler: (data?: any) => void) => {
    if (!handlersRef.current.has(event)) {
      handlersRef.current.set(event, new Set());
    }

    handlersRef.current.get(event)?.add(handler);
    websocketClient.on(event as any, handler);

    // Return unsubscribe function
    return () => {
      handlersRef.current.get(event)?.delete(handler);
      websocketClient.off(event as any, handler);
    };
  }, []);

  // Unregister event handler
  const off = useCallback((event: string, handler: (data?: any) => void) => {
    handlersRef.current.get(event)?.delete(handler);
    websocketClient.off(event as any, handler);
  }, []);

  return {
    isConnected,
    isReconnecting,
    connectionId,
    connect,
    disconnect,
    send,
    subscribe,
    unsubscribe,
    broadcast,
    sendDirect,
    on,
    off,
  };
}

export default useWebSocket;
