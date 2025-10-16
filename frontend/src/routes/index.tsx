/**
 * Route Configuration
 * Defines all application routes with lazy loading
 */

import { lazy, ComponentType } from 'react';

// Lazy load pages for code splitting - Production-grade implementations only
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

// Agents & AI - Production-grade implementations
const Agents = lazy(() => import('@/pages/Agents'));
const AgentDetail = lazy(() => import('@/pages/AgentDetail'));

// Transaction & Fraud
const Transactions = lazy(() => import('@/pages/Transactions'));
const TransactionDetail = lazy(() => import('@/pages/TransactionDetail'));
const CustomerProfile = lazy(() => import('@/pages/CustomerProfile'));
const FraudRules = lazy(() => import('@/pages/FraudRules'));
const AdvancedRuleEngine = lazy(() => import('@/pages/AdvancedRuleEngine'));

// Data & Analytics
const KnowledgeBase = lazy(() => import('@/pages/KnowledgeBase'));

// Phase 5 - Advanced Features
const PatternAnalysis = lazy(() => import('@/pages/PatternAnalysis'));
const LLMAnalysis = lazy(() => import('@/pages/LLMAnalysis'));
const AgentCollaboration = lazy(() => import('@/pages/AgentCollaboration'));

// Feature 1: Real-Time Collaboration Dashboard
const CollaborationDashboard = lazy(() => import('@/pages/CollaborationDashboard'));
const CollaborationSessionDetail = lazy(() => import('@/pages/CollaborationSessionDetail'));

// Feature 2: Regulatory Change Alerts with Email
const AlertManagement = lazy(() => import('@/pages/AlertManagement'));

// Feature 3: Export/Reporting Module
const ExportsReports = lazy(() => import('@/pages/ExportsReports'));

// Simulator Management
const SimulatorManagement = lazy(() => import('@/pages/SimulatorManagement'));

// Additional Simulator Components
const ScenarioCreation = lazy(() => import('@/pages/ScenarioCreation'));
const SimulationExecution = lazy(() => import('@/pages/SimulationExecution'));
const SimulationResults = lazy(() => import('@/pages/SimulationResults'));
const ImpactAnalysis = lazy(() => import('@/pages/ImpactAnalysis'));

// Feature 4: API Key Management UI
const LLMKeyManagement = lazy(() => import('@/pages/LLMKeyManagement'));

// Feature 5: Predictive Compliance Risk Scoring
const RiskScoring = lazy(() => import('@/pages/RiskScoring'));

// Feature 7: Regulatory Change Simulator
const RegulatorySimulator = lazy(() => import('@/pages/RegulatorySimulator'));

// Feature 8: Advanced Analytics & BI Dashboard
const AnalyticsDashboard = lazy(() => import('@/pages/AnalyticsDashboard'));

// Feature 10: Natural Language Policy Builder
const NLPolicyBuilder = lazy(() => import('@/pages/NLPolicyBuilder'));

// Policy Generation System
const PolicyGeneration = lazy(() => import('@/pages/PolicyGeneration'));

// Feature 12: Regulatory Chatbot
const RegulatoryChatbot = lazy(() => import('@/pages/RegulatoryChatbot'));

// Feature 13: Integration Marketplace
const IntegrationMarketplace = lazy(() => import('@/pages/IntegrationMarketplace'));

// Feature 14: Tool Categories Testing
const ToolCategoriesTesting = lazy(() => import('@/pages/ToolCategoriesTesting'));

// Feature 15: Compliance Training Module
const ComplianceTraining = lazy(() => import('@/pages/ComplianceTraining'));

// Training Management System
const TrainingManagement = lazy(() => import('@/pages/TrainingManagement'));

// System & Monitoring - Production-grade implementations
const SystemMetrics = lazy(() => import('@/pages/SystemMetrics'));
const CircuitBreakers = lazy(() => import('@/pages/CircuitBreakers'));

// Settings - Production-grade implementation
const Settings = lazy(() => import('@/pages/Settings'));

// New Advanced Features - Production-grade implementations
const MemoryManagement = lazy(() => import('@/pages/MemoryManagement'));
const FeedbackAnalytics = lazy(() => import('@/pages/FeedbackAnalytics'));
const MCDAAdvanced = lazy(() => import('@/pages/MCDAAdvanced'));
const RiskDashboard = lazy(() => import('@/pages/RiskDashboard'));
const EmbeddingsExplorer = lazy(() => import('@/pages/EmbeddingsExplorer'));
const AgentCommunicationConsole = lazy(() => import('@/pages/AgentCommunicationConsole'));
const FunctionCallingDebugger = lazy(() => import('@/pages/FunctionCallingDebugger'));
const DataQualityMonitor = lazy(() => import('@/pages/DataQualityMonitor'));

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
    path: '/customers/:customerId',
    name: 'Customer Profile',
    element: CustomerProfile,
    showInNav: false,
  },
  {
    path: '/fraud-rules',
    name: 'Fraud Rules',
    element: FraudRules,
    showInNav: false,
  },
  {
    path: '/advanced-rule-engine',
    name: 'Advanced Rule Engine',
    element: AdvancedRuleEngine,
    showInNav: true,
    icon: 'Shield',
    requiredPermissions: ['rules.manage'],
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
    path: '/collaboration',
    name: 'Collaboration Dashboard',
    element: CollaborationDashboard,
    showInNav: true,
    icon: 'Brain',
  },
  {
    path: '/collaboration/:sessionId',
    name: 'Session Detail',
    element: CollaborationSessionDetail,
    showInNav: false,
  },
  {
    path: '/alerts',
    name: 'Alert Management',
    element: AlertManagement,
    showInNav: true,
    icon: 'Bell',
  },
  {
    path: '/exports',
    name: 'Exports & Reports',
    element: ExportsReports,
    showInNav: true,
    icon: 'Download',
  },
  {
    path: '/llm-keys',
    name: 'LLM API Keys',
    element: LLMKeyManagement,
    showInNav: true,
    icon: 'Key',
  },
  {
    path: '/risk-scoring',
    name: 'Risk Scoring',
    element: RiskScoring,
    showInNav: true,
    icon: 'AlertTriangle',
  },
  {
    path: '/simulator',
    name: 'Regulatory Simulator',
    element: RegulatorySimulator,
    showInNav: true,
    icon: 'Play',
  },
  {
    path: '/simulator-management',
    name: 'Simulator Management',
    element: SimulatorManagement,
    showInNav: true,
    icon: 'Settings',
    requiredPermissions: ['simulator.admin'],
  },
  {
    path: '/impact-analysis',
    name: 'Impact Analysis',
    element: ImpactAnalysis,
    showInNav: true,
    icon: 'Calculator',
    requiredPermissions: ['simulator.view'],
  },
  {
    path: '/scenario-creation',
    name: 'Create Scenario',
    element: ScenarioCreation,
    showInNav: false,
    icon: 'Plus',
  },
  {
    path: '/simulation-execution',
    name: 'Simulation Execution',
    element: SimulationExecution,
    showInNav: false,
    icon: 'Play',
  },
  {
    path: '/simulation-results',
    name: 'Simulation Results',
    element: SimulationResults,
    showInNav: false,
    icon: 'BarChart3',
  },
  {
    path: '/analytics',
    name: 'Analytics Dashboard',
    element: AnalyticsDashboard,
    showInNav: true,
    icon: 'BarChart3',
  },
  {
    path: '/nl-policies',
    name: 'NL Policy Builder',
    element: NLPolicyBuilder,
    showInNav: true,
    icon: 'Sparkles',
  },
  {
    path: '/policy-generation',
    name: 'Policy Generation',
    element: PolicyGeneration,
    showInNav: true,
    icon: 'FileText',
    requiredPermissions: ['policies.generate'],
  },
  {
    path: '/chatbot',
    name: 'Regulatory Chatbot',
    element: RegulatoryChatbot,
    showInNav: true,
    icon: 'MessageCircle',
  },
  {
    path: '/integrations',
    name: 'Integrations',
    element: IntegrationMarketplace,
    showInNav: true,
    icon: 'Package',
  },
  {
    path: '/tool-categories-testing',
    name: 'Tool Categories Testing',
    element: ToolCategoriesTesting,
    showInNav: true,
    icon: 'Activity',
  },
  {
    path: '/training',
    name: 'Compliance Training',
    element: ComplianceTraining,
    showInNav: true,
    icon: 'GraduationCap',
  },
  {
    path: '/training-management',
    name: 'Training Management',
    element: TrainingManagement,
    showInNav: true,
    icon: 'GraduationCap',
    requiredPermissions: ['training.view'],
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
    path: '/settings',
    name: 'Settings',
    element: Settings,
    showInNav: true,
    icon: 'Settings',
  },
  {
    path: '/memory-management',
    name: 'Memory Management',
    element: MemoryManagement,
    showInNav: true,
    icon: 'Database',
  },
  {
    path: '/feedback-analytics',
    name: 'Feedback Analytics',
    element: FeedbackAnalytics,
    showInNav: true,
    icon: 'TrendingUp',
  },
  {
    path: '/mcda-advanced',
    name: 'MCDA Advanced',
    element: MCDAAdvanced,
    showInNav: true,
    icon: 'Sliders',
  },
  {
    path: '/risk-dashboard',
    name: 'Risk Dashboard',
    element: RiskDashboard,
    showInNav: true,
    icon: 'AlertTriangle',
  },
  {
    path: '/embeddings-explorer',
    name: 'Embeddings Explorer',
    element: EmbeddingsExplorer,
    showInNav: true,
    icon: 'Search',
  },
  {
    path: '/agent-communication-console',
    name: 'Agent Communication',
    element: AgentCommunicationConsole,
    showInNav: true,
    icon: 'MessageSquare',
  },
  {
    path: '/function-calling-debugger',
    name: 'Function Debugger',
    element: FunctionCallingDebugger,
    showInNav: true,
    icon: 'Bug',
  },
  {
    path: '/data-quality-monitor',
    name: 'Data Quality',
    element: DataQualityMonitor,
    showInNav: true,
    icon: 'CheckCircle',
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
