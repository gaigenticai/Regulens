/**
 * Route Configuration
 * Defines all application routes with lazy loading
 */

import { lazy, ComponentType } from 'react';

// Lazy load pages for code splitting
const Login = lazy(() => import('@/pages/Login'));
const Dashboard = lazy(() => import('@/pages/Dashboard'));
const NotFound = lazy(() => import('@/pages/NotFound'));

// Activity & Monitoring
const ActivityFeed = lazy(() => import('@/pages/ActivityFeed'));
const RegulatoryChanges = lazy(() => import('@/pages/RegulatoryChanges'));
const AuditTrail = lazy(() => import('@/pages/AuditTrail'));

// MCDA & Decisions
const DecisionEngine = lazy(() => import('@/pages/DecisionEngine'));
const DecisionHistory = lazy(() => import('@/pages/DecisionHistory'));
const DecisionDetail = lazy(() => import('@/pages/DecisionDetail'));

// Agents & AI
const Agents = lazy(() => import('@/pages/Agents'));
const AgentDetail = lazy(() => import('@/pages/AgentDetail'));
const Collaboration = lazy(() => import('@/pages/Collaboration'));
const LLMIntegration = lazy(() => import('@/pages/LLMIntegration'));

// Transaction & Fraud
const Transactions = lazy(() => import('@/pages/Transactions'));
const TransactionDetail = lazy(() => import('@/pages/TransactionDetail'));
const FraudRules = lazy(() => import('@/pages/FraudRules'));

// Data & Analytics
const DataIngestion = lazy(() => import('@/pages/DataIngestion'));
const PatternRecognition = lazy(() => import('@/pages/PatternRecognition'));
const KnowledgeBase = lazy(() => import('@/pages/KnowledgeBase'));

// Phase 5 - Advanced Features
const PatternAnalysis = lazy(() => import('@/pages/PatternAnalysis'));
const LLMAnalysis = lazy(() => import('@/pages/LLMAnalysis'));
const AgentCollaboration = lazy(() => import('@/pages/AgentCollaboration'));

// System & Monitoring
const SystemMetrics = lazy(() => import('@/pages/SystemMetrics'));
const CircuitBreakers = lazy(() => import('@/pages/CircuitBreakers'));
const SystemHealth = lazy(() => import('@/pages/SystemHealth'));

// Settings
const Settings = lazy(() => import('@/pages/Settings'));

export interface AppRoute {
  path: string;
  name?: string;
  icon?: string;
  showInNav?: boolean;
  requiredPermissions?: string[];
  element: ComponentType<any>;
}

export const routes: AppRoute[] = [
  {
    path: '/',
    name: 'Dashboard',
    element: Dashboard,
    showInNav: true,
    icon: 'LayoutDashboard',
  },
  {
    path: '/activity',
    name: 'Activity Feed',
    element: ActivityFeed,
    showInNav: true,
    icon: 'Activity',
  },
  {
    path: '/regulatory',
    name: 'Regulatory Changes',
    element: RegulatoryChanges,
    showInNav: true,
    icon: 'FileText',
  },
  {
    path: '/audit',
    name: 'Audit Trail',
    element: AuditTrail,
    showInNav: true,
    icon: 'Shield',
    requiredPermissions: ['audit.view'],
  },
  {
    path: '/decisions',
    name: 'Decision Engine',
    element: DecisionEngine,
    showInNav: true,
    icon: 'GitBranch',
  },
  {
    path: '/decisions/history',
    name: 'Decision History',
    element: DecisionHistory,
    showInNav: false,
  },
  {
    path: '/decisions/:id',
    name: 'Decision Detail',
    element: DecisionDetail,
    showInNav: false,
  },
  {
    path: '/agents',
    name: 'Agents',
    element: Agents,
    showInNav: true,
    icon: 'Bot',
  },
  {
    path: '/agents/:id',
    name: 'Agent Detail',
    element: AgentDetail,
    showInNav: false,
  },
  {
    path: '/collaboration',
    name: 'Collaboration',
    element: Collaboration,
    showInNav: true,
    icon: 'Users',
  },
  {
    path: '/llm',
    name: 'LLM Integration',
    element: LLMIntegration,
    showInNav: true,
    icon: 'Brain',
  },
  {
    path: '/transactions',
    name: 'Transactions',
    element: Transactions,
    showInNav: true,
    icon: 'CreditCard',
  },
  {
    path: '/transactions/:id',
    name: 'Transaction Detail',
    element: TransactionDetail,
    showInNav: false,
  },
  {
    path: '/fraud-rules',
    name: 'Fraud Rules',
    element: FraudRules,
    showInNav: false,
  },
  {
    path: '/data-ingestion',
    name: 'Data Ingestion',
    element: DataIngestion,
    showInNav: true,
    icon: 'Database',
  },
  {
    path: '/patterns',
    name: 'Pattern Recognition',
    element: PatternRecognition,
    showInNav: true,
    icon: 'TrendingUp',
  },
  {
    path: '/knowledge',
    name: 'Knowledge Base',
    element: KnowledgeBase,
    showInNav: true,
    icon: 'BookOpen',
  },
  {
    path: '/pattern-analysis',
    name: 'Pattern Analysis',
    element: PatternAnalysis,
    showInNav: true,
    icon: 'Network',
  },
  {
    path: '/llm-analysis',
    name: 'LLM Analysis',
    element: LLMAnalysis,
    showInNav: true,
    icon: 'Sparkles',
  },
  {
    path: '/agent-collaboration',
    name: 'Agent Collaboration',
    element: AgentCollaboration,
    showInNav: true,
    icon: 'Users',
  },
  {
    path: '/metrics',
    name: 'System Metrics',
    element: SystemMetrics,
    showInNav: true,
    icon: 'BarChart3',
  },
  {
    path: '/circuit-breakers',
    name: 'Circuit Breakers',
    element: CircuitBreakers,
    showInNav: false,
  },
  {
    path: '/health',
    name: 'System Health',
    element: SystemHealth,
    showInNav: false,
  },
  {
    path: '/settings',
    name: 'Settings',
    element: Settings,
    showInNav: true,
    icon: 'Settings',
  },
];

export const publicRoutes: AppRoute[] = [
  {
    path: '/login',
    element: Login,
  },
  {
    path: '*',
    element: NotFound,
  },
];
