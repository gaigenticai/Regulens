/**
 * Advanced Rule Engine - Main Page
 * Comprehensive fraud detection and rule management interface
 */

import React, { useState } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../components/ui/card';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '../components/ui/tabs';
import { Badge } from '../components/ui/badge';
import { Button } from '../components/ui/button';
import {
  Shield,
  Rule,
  TestTube,
  BarChart3,
  Settings,
  Activity,
  AlertTriangle,
  TrendingUp
} from 'lucide-react';

// Import sub-components
import RuleEngineManagement from '../components/RuleEngine/RuleEngineManagement';
import RuleCreationForm from '../components/RuleEngine/RuleCreationForm';
import RuleTesting from '../components/RuleEngine/RuleTesting';
import RuleAnalytics from '../components/RuleEngine/RuleAnalytics';

const AdvancedRuleEngine: React.FC = () => {
  const [activeTab, setActiveTab] = useState('management');
  const [isLoading, setIsLoading] = useState(false);

  // Mock data for dashboard stats
  const stats = {
    totalRules: 47,
    activeRules: 42,
    fraudDetections: 1234,
    averageExecutionTime: 45, // ms
    riskLevels: {
      low: 65,
      medium: 23,
      high: 8,
      critical: 4
    }
  };

  return (
    <div className="container mx-auto p-6 space-y-6">
      {/* Header Section */}
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold flex items-center gap-3">
            <Shield className="h-8 w-8 text-blue-600" />
            Advanced Rule Engine
          </h1>
          <p className="text-gray-600 mt-2">
            Enterprise-grade fraud detection and policy enforcement system
          </p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm">
            <Settings className="h-4 w-4 mr-2" />
            Engine Config
          </Button>
          <Button variant="outline" size="sm">
            <Activity className="h-4 w-4 mr-2" />
            Engine Status
          </Button>
        </div>
      </div>

      {/* Dashboard Stats */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Total Rules</CardTitle>
            <Rule className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{stats.totalRules}</div>
            <p className="text-xs text-muted-foreground">
              {stats.activeRules} active
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Fraud Detections</CardTitle>
            <AlertTriangle className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{stats.fraudDetections}</div>
            <p className="text-xs text-muted-foreground">
              +12% from last month
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Avg Execution Time</CardTitle>
            <TrendingUp className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{stats.averageExecutionTime}ms</div>
            <p className="text-xs text-muted-foreground">
              Within target range
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Risk Distribution</CardTitle>
            <BarChart3 className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="flex gap-1 mb-2">
              <Badge variant="secondary" className="text-xs">L:{stats.riskLevels.low}</Badge>
              <Badge variant="outline" className="text-xs">M:{stats.riskLevels.medium}</Badge>
              <Badge variant="destructive" className="text-xs">H:{stats.riskLevels.high}</Badge>
              <Badge variant="destructive" className="text-xs border-red-800">C:{stats.riskLevels.critical}</Badge>
            </div>
            <p className="text-xs text-muted-foreground">
              Recent detections
            </p>
          </CardContent>
        </Card>
      </div>

      {/* Main Content Tabs */}
      <Tabs value={activeTab} onValueChange={setActiveTab} className="space-y-4">
        <TabsList className="grid w-full grid-cols-4">
          <TabsTrigger value="management" className="flex items-center gap-2">
            <Rule className="h-4 w-4" />
            Rule Management
          </TabsTrigger>
          <TabsTrigger value="creation" className="flex items-center gap-2">
            <Settings className="h-4 w-4" />
            Rule Creation
          </TabsTrigger>
          <TabsTrigger value="testing" className="flex items-center gap-2">
            <TestTube className="h-4 w-4" />
            Rule Testing
          </TabsTrigger>
          <TabsTrigger value="analytics" className="flex items-center gap-2">
            <BarChart3 className="h-4 w-4" />
            Analytics
          </TabsTrigger>
        </TabsList>

        <TabsContent value="management" className="space-y-4">
          <RuleEngineManagement />
        </TabsContent>

        <TabsContent value="creation" className="space-y-4">
          <RuleCreationForm />
        </TabsContent>

        <TabsContent value="testing" className="space-y-4">
          <RuleTesting />
        </TabsContent>

        <TabsContent value="analytics" className="space-y-4">
          <RuleAnalytics />
        </TabsContent>
      </Tabs>
    </div>
  );
};

export default AdvancedRuleEngine;