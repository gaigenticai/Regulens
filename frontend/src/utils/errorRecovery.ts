/**
 * Error Recovery Mechanisms
 * Production-grade error recovery and retry logic
 */

import { APIError, ErrorCategory, ErrorReporter } from './errorHandling';

// Recovery strategy types
export enum RecoveryStrategy {
  IMMEDIATE_RETRY = 'immediate_retry',
  EXPONENTIAL_BACKOFF = 'exponential_backoff',
  CIRCUIT_BREAKER = 'circuit_breaker',
  FALLBACK_RESPONSE = 'fallback_response',
  USER_INTERVENTION = 'user_intervention',
  SERVICE_REDIRECT = 'service_redirect'
}

// Recovery configuration
export interface RecoveryConfig {
  strategy: RecoveryStrategy;
  maxRetries: number;
  baseDelay: number;
  maxDelay: number;
  backoffMultiplier: number;
  timeout: number;
  fallbackEnabled: boolean;
  circuitBreakerThreshold: number;
  circuitBreakerTimeout: number;
}

// Recovery result
export interface RecoveryResult {
  success: boolean;
  strategy: RecoveryStrategy;
  attempts: number;
  totalDelay: number;
  finalError?: APIError;
  recoveredData?: any;
  metadata: Record<string, any>;
}

// Circuit breaker state
interface CircuitBreakerState {
  failures: number;
  lastFailureTime: number;
  state: 'closed' | 'open' | 'half-open';
}

// Global circuit breaker states
const circuitBreakerStates = new Map<string, CircuitBreakerState>();

// Recovery manager class
export class ErrorRecoveryManager {
  private static instance: ErrorRecoveryManager;
  private recoveryConfigs = new Map<string, RecoveryConfig>();

  private constructor() {
    this.initializeDefaultConfigs();
  }

  static getInstance(): ErrorRecoveryManager {
    if (!ErrorRecoveryManager.instance) {
      ErrorRecoveryManager.instance = new ErrorRecoveryManager();
    }
    return ErrorRecoveryManager.instance;
  }

  // Configure recovery strategy for specific error types
  setRecoveryConfig(errorCategory: string, config: Partial<RecoveryConfig>): void {
    const defaultConfig: RecoveryConfig = {
      strategy: RecoveryStrategy.EXPONENTIAL_BACKOFF,
      maxRetries: 3,
      baseDelay: 1000,
      maxDelay: 30000,
      backoffMultiplier: 2,
      timeout: 60000,
      fallbackEnabled: true,
      circuitBreakerThreshold: 5,
      circuitBreakerTimeout: 60000
    };

    this.recoveryConfigs.set(errorCategory, { ...defaultConfig, ...config });
  }

  // Execute recovery for an error
  async executeRecovery(
    error: APIError,
    recoveryFunction: () => Promise<any>,
    customConfig?: Partial<RecoveryConfig>
  ): Promise<RecoveryResult> {
    const config = this.getRecoveryConfig(error, customConfig);
    const result: RecoveryResult = {
      success: false,
      strategy: config.strategy,
      attempts: 0,
      totalDelay: 0,
      metadata: {}
    };

    // Check circuit breaker
    if (config.strategy === RecoveryStrategy.CIRCUIT_BREAKER) {
      const circuitState = this.getCircuitBreakerState(error.endpoint);
      if (circuitState.state === 'open') {
        result.finalError = error;
        result.metadata.circuitBreakerOpen = true;
        return result;
      }
    }

    // Execute recovery based on strategy
    try {
      switch (config.strategy) {
        case RecoveryStrategy.IMMEDIATE_RETRY:
          return await this.executeImmediateRetry(error, recoveryFunction, config, result);

        case RecoveryStrategy.EXPONENTIAL_BACKOFF:
          return await this.executeExponentialBackoff(error, recoveryFunction, config, result);

        case RecoveryStrategy.CIRCUIT_BREAKER:
          return await this.executeCircuitBreakerRetry(error, recoveryFunction, config, result);

        case RecoveryStrategy.FALLBACK_RESPONSE:
          return await this.executeFallbackRecovery(error, recoveryFunction, config, result);

        case RecoveryStrategy.USER_INTERVENTION:
          return this.executeUserInterventionRecovery(error, config, result);

        default:
          result.finalError = error;
          return result;
      }
    } catch (recoveryError) {
      result.finalError = error;
      result.metadata.recoveryError = recoveryError;
      return result;
    }
  }

  private async executeImmediateRetry(
    error: APIError,
    recoveryFunction: () => Promise<any>,
    config: RecoveryConfig,
    result: RecoveryResult
  ): Promise<RecoveryResult> {
    for (let attempt = 1; attempt <= config.maxRetries; attempt++) {
      try {
        result.attempts = attempt;
        const data = await recoveryFunction();
        result.success = true;
        result.recoveredData = data;
        return result;
      } catch (retryError) {
        result.attempts = attempt;
        if (attempt === config.maxRetries) {
          result.finalError = error;
        }
      }
    }
    return result;
  }

  private async executeExponentialBackoff(
    error: APIError,
    recoveryFunction: () => Promise<any>,
    config: RecoveryConfig,
    result: RecoveryResult
  ): Promise<RecoveryResult> {
    for (let attempt = 1; attempt <= config.maxRetries; attempt++) {
      try {
        result.attempts = attempt;
        const data = await recoveryFunction();
        result.success = true;
        result.recoveredData = data;
        return result;
      } catch (retryError) {
        result.attempts = attempt;

        if (attempt < config.maxRetries) {
          const delay = Math.min(
            config.baseDelay * Math.pow(config.backoffMultiplier, attempt - 1),
            config.maxDelay
          );
          result.totalDelay += delay;

          await new Promise(resolve => setTimeout(resolve, delay));
        } else {
          result.finalError = error;
        }
      }
    }
    return result;
  }

  private async executeCircuitBreakerRetry(
    error: APIError,
    recoveryFunction: () => Promise<any>,
    config: RecoveryConfig,
    result: RecoveryResult
  ): Promise<RecoveryResult> {
    const circuitState = this.getCircuitBreakerState(error.endpoint);

    // If circuit is open, fail fast
    if (circuitState.state === 'open') {
      result.metadata.circuitBreakerOpen = true;
      result.finalError = error;
      return result;
    }

    try {
      const data = await recoveryFunction();
      result.success = true;
      result.recoveredData = data;

      // Reset circuit breaker on success
      this.resetCircuitBreaker(error.endpoint);

      return result;
    } catch (retryError) {
      // Record failure and potentially open circuit
      this.recordCircuitBreakerFailure(error.endpoint, config.circuitBreakerThreshold);
      result.finalError = error;
      return result;
    }
  }

  private async executeFallbackRecovery(
    error: APIError,
    recoveryFunction: () => Promise<any>,
    config: RecoveryConfig,
    result: RecoveryResult
  ): Promise<RecoveryResult> {
    // Try primary recovery first
    try {
      const data = await recoveryFunction();
      result.success = true;
      result.recoveredData = data;
      return result;
    } catch (primaryError) {
      // Fall back to cached or default data
      if (config.fallbackEnabled) {
        const fallbackData = this.getFallbackData(error);
        if (fallbackData) {
          result.success = true;
          result.recoveredData = fallbackData;
          result.metadata.fallbackUsed = true;
          return result;
        }
      }

      result.finalError = error;
      return result;
    }
  }

  private executeUserInterventionRecovery(
    error: APIError,
    config: RecoveryConfig,
    result: RecoveryResult
  ): RecoveryResult {
    // For user intervention, we don't auto-recover
    // Instead, we prepare the error for user action
    result.finalError = error;
    result.metadata.requiresUserAction = true;
    result.metadata.userActionRequired = this.getUserActionForError(error);

    return result;
  }

  private getRecoveryConfig(error: APIError, customConfig?: Partial<RecoveryConfig>): RecoveryConfig {
    const categoryKey = error.category.toString();

    // Check for custom config first
    if (customConfig) {
      const baseConfig = this.recoveryConfigs.get(categoryKey);
      return { ...(baseConfig || this.getDefaultConfig()), ...customConfig };
    }

    // Get category-specific config
    const config = this.recoveryConfigs.get(categoryKey);
    if (config) {
      return config;
    }

    // Return default config
    return this.getDefaultConfig();
  }

  private getDefaultConfig(): RecoveryConfig {
    return {
      strategy: RecoveryStrategy.EXPONENTIAL_BACKOFF,
      maxRetries: 3,
      baseDelay: 1000,
      maxDelay: 30000,
      backoffMultiplier: 2,
      timeout: 60000,
      fallbackEnabled: true,
      circuitBreakerThreshold: 5,
      circuitBreakerTimeout: 60000
    };
  }

  private getCircuitBreakerState(endpoint: string): CircuitBreakerState {
    if (!circuitBreakerStates.has(endpoint)) {
      circuitBreakerStates.set(endpoint, {
        failures: 0,
        lastFailureTime: 0,
        state: 'closed'
      });
    }
    return circuitBreakerStates.get(endpoint)!;
  }

  private recordCircuitBreakerFailure(endpoint: string, threshold: number): void {
    const state = this.getCircuitBreakerState(endpoint);
    state.failures++;
    state.lastFailureTime = Date.now();

    if (state.failures >= threshold) {
      state.state = 'open';
    }
  }

  private resetCircuitBreaker(endpoint: string): void {
    const state = this.getCircuitBreakerState(endpoint);
    state.failures = 0;
    state.state = 'closed';
  }

  private getFallbackData(error: APIError): any {
    // In production, this would retrieve cached data or default responses
    // For now, return null to indicate no fallback available
    return null;
  }

  private getUserActionForError(error: APIError): string {
    switch (error.category) {
      case ErrorCategory.AUTHENTICATION:
        return 'Please log in again to continue';
      case ErrorCategory.AUTHORIZATION:
        return 'Contact your administrator for access permissions';
      case ErrorCategory.RATE_LIMIT:
        return 'Wait a few minutes before trying again';
      case ErrorCategory.NETWORK_ERROR:
        return 'Check your internet connection and try again';
      default:
        return 'Please try again or contact support if the problem persists';
    }
  }

  private initializeDefaultConfigs(): void {
    // Network errors - immediate retry with backoff
    this.setRecoveryConfig('network_error', {
      strategy: RecoveryStrategy.EXPONENTIAL_BACKOFF,
      maxRetries: 3,
      baseDelay: 500,
      maxDelay: 5000
    });

    // Server errors - circuit breaker pattern
    this.setRecoveryConfig('server_error', {
      strategy: RecoveryStrategy.CIRCUIT_BREAKER,
      maxRetries: 2,
      circuitBreakerThreshold: 5,
      circuitBreakerTimeout: 30000
    });

    // Authentication errors - user intervention
    this.setRecoveryConfig('authentication_error', {
      strategy: RecoveryStrategy.USER_INTERVENTION,
      maxRetries: 0
    });

    // Rate limit errors - exponential backoff
    this.setRecoveryConfig('rate_limit_error', {
      strategy: RecoveryStrategy.EXPONENTIAL_BACKOFF,
      maxRetries: 1,
      baseDelay: 1000
    });

    // Validation errors - no retry (user must fix)
    this.setRecoveryConfig('validation_error', {
      strategy: RecoveryStrategy.USER_INTERVENTION,
      maxRetries: 0
    });
  }
}

// Utility functions for common recovery scenarios

export const createRecoveryConfig = (
  strategy: RecoveryStrategy,
  options: Partial<RecoveryConfig> = {}
): RecoveryConfig => {
  const defaults: RecoveryConfig = {
    strategy,
    maxRetries: 3,
    baseDelay: 1000,
    maxDelay: 30000,
    backoffMultiplier: 2,
    timeout: 60000,
    fallbackEnabled: true,
    circuitBreakerThreshold: 5,
    circuitBreakerTimeout: 60000
  };

  return { ...defaults, ...options };
};

export const recoverWithRetry = async (
  operation: () => Promise<any>,
  maxRetries: number = 3,
  baseDelay: number = 1000
): Promise<any> => {
  const manager = ErrorRecoveryManager.getInstance();

  for (let attempt = 1; attempt <= maxRetries; attempt++) {
    try {
      return await operation();
    } catch (error) {
      if (attempt === maxRetries) {
        throw error;
      }

      const delay = baseDelay * Math.pow(2, attempt - 1);
      await new Promise(resolve => setTimeout(resolve, delay));
    }
  }
};

export const recoverWithCircuitBreaker = async (
  operation: () => Promise<any>,
  endpoint: string,
  threshold: number = 5,
  timeout: number = 60000
): Promise<any> => {
  const state = circuitBreakerStates.get(endpoint);

  if (state && state.state === 'open') {
    // Check if we should try half-open
    if (Date.now() - state.lastFailureTime > timeout) {
      state.state = 'half-open';
    } else {
      throw new Error('Circuit breaker is open');
    }
  }

  try {
    const result = await operation();

    // Reset on success
    if (state) {
      state.failures = 0;
      state.state = 'closed';
    }

    return result;
  } catch (error) {
    // Record failure
    if (state) {
      state.failures++;
      state.lastFailureTime = Date.now();

      if (state.failures >= threshold) {
        state.state = 'open';
      }
    }

    throw error;
  }
};

export const recoverWithFallback = async (
  primaryOperation: () => Promise<any>,
  fallbackOperation: () => Promise<any>
): Promise<any> => {
  try {
    return await primaryOperation();
  } catch (error) {
    console.warn('Primary operation failed, using fallback:', error);
    return await fallbackOperation();
  }
};

// React hook for error recovery
export const useErrorRecovery = () => {
  const manager = ErrorRecoveryManager.getInstance();

  const recover = async (
    error: APIError,
    recoveryFunction: () => Promise<any>,
    config?: Partial<RecoveryConfig>
  ): Promise<RecoveryResult> => {
    return manager.executeRecovery(error, recoveryFunction, config);
  };

  const configureRecovery = (errorCategory: string, config: Partial<RecoveryConfig>) => {
    manager.setRecoveryConfig(errorCategory, config);
  };

  return { recover, configureRecovery };
};
