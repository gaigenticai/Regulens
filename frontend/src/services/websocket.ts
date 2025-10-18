/**
 * WebSocket Client Service - Week 3 Phase 3
 * Real-time bidirectional communication with auto-reconnect
 */

import EventEmitter from 'eventemitter3';

export type MessageType = 
  | 'CONNECTION_ESTABLISHED'
  | 'HEARTBEAT'
  | 'SUBSCRIBE'
  | 'UNSUBSCRIBE'
  | 'BROADCAST'
  | 'DIRECT_MESSAGE'
  | 'SESSION_UPDATE'
  | 'RULE_EVALUATION_RESULT'
  | 'DECISION_ANALYSIS_RESULT'
  | 'CONSENSUS_UPDATE'
  | 'LEARNING_FEEDBACK'
  | 'ALERT'
  | 'ERROR';

export interface WebSocketMessage {
  message_id: string;
  type: MessageType;
  sender_id: string;
  recipient_id?: string;
  payload: Record<string, any>;
  requires_acknowledgment?: boolean;
  timestamp: number;
}

export type WebSocketEventType = 
  | 'connected'
  | 'disconnected'
  | 'reconnecting'
  | 'message'
  | 'error'
  | MessageType;

interface ReconnectConfig {
  maxAttempts: number;
  initialDelay: number;
  maxDelay: number;
  backoffMultiplier: number;
}

class WebSocketClient extends EventEmitter<WebSocketEventType> {
  private ws: WebSocket | null = null;
  private url: string;
  private isConnected = false;
  private isConnecting = false;
  private messageQueue: WebSocketMessage[] = [];
  private reconnectAttempts = 0;
  private reconnectConfig: ReconnectConfig;
  private heartbeatInterval: NodeJS.Timer | null = null;
  private reconnectTimeout: NodeJS.Timer | null = null;
  private connectionId: string = '';
  private userId: string = '';

  constructor(url: string = '') {
    super();
    
    this.url = url || `ws://${window.location.hostname}:8081`;
    
    this.reconnectConfig = {
      maxAttempts: 10,
      initialDelay: 1000,
      maxDelay: 30000,
      backoffMultiplier: 1.5,
    };
  }

  /**
   * Connect to WebSocket server
   */
  connect(userId: string): Promise<void> {
    return new Promise((resolve, reject) => {
      if (this.isConnected) {
        resolve();
        return;
      }

      if (this.isConnecting) {
        return;
      }

      this.isConnecting = true;
      this.userId = userId;

      try {
        this.ws = new WebSocket(this.url);

        this.ws.onopen = () => {
          this.onOpen();
          resolve();
        };

        this.ws.onmessage = (event) => {
          this.onMessage(event);
        };

        this.ws.onerror = (event) => {
          const error = new Error('WebSocket error');
          this.emit('error', error);
          reject(error);
        };

        this.ws.onclose = () => {
          this.onClose();
        };
      } catch (error) {
        this.isConnecting = false;
        reject(error);
      }
    });
  }

  /**
   * Disconnect from server
   */
  disconnect(): void {
    if (this.heartbeatInterval) {
      clearInterval(this.heartbeatInterval);
    }

    if (this.reconnectTimeout) {
      clearTimeout(this.reconnectTimeout);
    }

    if (this.ws) {
      this.ws.close();
      this.ws = null;
    }

    this.isConnected = false;
    this.isConnecting = false;
    this.messageQueue = [];
    this.reconnectAttempts = 0;
  }

  /**
   * Send message
   */
  send(message: Partial<WebSocketMessage>): void {
    const fullMessage: WebSocketMessage = {
      message_id: message.message_id || this.generateMessageId(),
      type: message.type || 'BROADCAST',
      sender_id: message.sender_id || this.userId,
      recipient_id: message.recipient_id,
      payload: message.payload || {},
      requires_acknowledgment: message.requires_acknowledgment ?? false,
      timestamp: Date.now(),
    };

    if (!this.isConnected) {
      this.messageQueue.push(fullMessage);
      return;
    }

    this.ws?.send(JSON.stringify(fullMessage));
  }

  /**
   * Subscribe to channel
   */
  subscribe(channel: string): void {
    this.send({
      type: 'SUBSCRIBE',
      payload: { channel },
    });
  }

  /**
   * Unsubscribe from channel
   */
  unsubscribe(channel: string): void {
    this.send({
      type: 'UNSUBSCRIBE',
      payload: { channel },
    });
  }

  /**
   * Broadcast message
   */
  broadcast(payload: Record<string, any>): void {
    this.send({
      type: 'BROADCAST',
      payload,
    });
  }

  /**
   * Send direct message
   */
  sendDirect(recipientId: string, payload: Record<string, any>): void {
    this.send({
      type: 'DIRECT_MESSAGE',
      recipient_id: recipientId,
      payload,
    });
  }

  /**
   * Get connection status
   */
  isReady(): boolean {
    return this.isConnected;
  }

  /**
   * Get connection ID
   */
  getConnectionId(): string {
    return this.connectionId;
  }

  // Private methods

  private onOpen(): void {
    this.isConnected = true;
    this.isConnecting = false;
    this.reconnectAttempts = 0;

    this.emit('connected');

    // Process queued messages
    while (this.messageQueue.length > 0) {
      const msg = this.messageQueue.shift();
      if (msg) {
        this.ws?.send(JSON.stringify(msg));
      }
    }

    // Start heartbeat
    this.startHeartbeat();
  }

  private onMessage(event: MessageEvent): void {
    try {
      const message: WebSocketMessage = JSON.parse(event.data);

      // Store connection ID
      if (message.type === 'CONNECTION_ESTABLISHED' && message.payload.connection_id) {
        this.connectionId = message.payload.connection_id;
      }

      // Emit specific message type
      this.emit(message.type, message);

      // Emit generic message event
      this.emit('message', message);
    } catch (error) {
      console.error('Failed to parse WebSocket message:', error);
    }
  }

  private onClose(): void {
    this.isConnected = false;
    this.emit('disconnected');

    if (this.heartbeatInterval) {
      clearInterval(this.heartbeatInterval);
    }

    // Attempt reconnect
    this.attemptReconnect();
  }

  private attemptReconnect(): void {
    if (this.reconnectAttempts >= this.reconnectConfig.maxAttempts) {
      console.error('Max reconnection attempts reached');
      this.emit('error', new Error('Failed to reconnect after maximum attempts'));
      return;
    }

    this.reconnectAttempts++;
    this.emit('reconnecting');

    const delay = Math.min(
      this.reconnectConfig.initialDelay * 
        Math.pow(this.reconnectConfig.backoffMultiplier, this.reconnectAttempts - 1),
      this.reconnectConfig.maxDelay
    );

    this.reconnectTimeout = setTimeout(() => {
      this.connect(this.userId).catch((error) => {
        console.error('Reconnection attempt failed:', error);
      });
    }, delay);
  }

  private startHeartbeat(): void {
    if (this.heartbeatInterval) {
      clearInterval(this.heartbeatInterval);
    }

    this.heartbeatInterval = setInterval(() => {
      if (this.isConnected) {
        this.send({ type: 'HEARTBEAT' });
      }
    }, 30000); // 30 seconds
  }

  private generateMessageId(): string {
    return `${Date.now()}-${Math.random().toString(36).substr(2, 9)}`;
  }
}

// Export singleton instance
export const websocketClient = new WebSocketClient();
export default websocketClient;
