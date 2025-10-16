/**
 * Rule Analytics Component
 * Comprehensive analytics dashboard for fraud detection rule performance
 */

import React, { useState } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../ui/card';
import { Button } from '../ui/button';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '../ui/select';
import { Badge } from '../ui/badge';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '../ui/tabs';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '../ui/table';
import {
  BarChart3,
  TrendingUp,
  TrendingDown,
  AlertTriangle,
  CheckCircle,
  Clock,
  Target,
  Zap,
  Shield,
  Activity,
  Calendar,
  Filter
} from 'lucide-react';

// Mock analytics data
const rulePerformanceData = [
  {
    rule_id: 'RULE-001',
    rule_name: 'High Amount Transaction Check',
    total_executions: 15420,
    successful_executions: 14950,
    fraud_detections: 234,
    false_positives: 45,
    average_execution_time: 12.5,
    success_rate: 96.9,
    trend: 'up' as const,
    trend_percentage: 2.3
  },
  {
    rule_id: 'RULE-002',
    rule_name: 'Velocity Pattern Detection',
    total_executions: 8750,
    successful_executions: 8230,
    fraud_detections: 156,
    false_positives: 28,
    average_execution_time: 18.7,
    success_rate: 94.1,
    trend: 'up' as const,
    trend_percentage: 1.8
  },
  {
    rule_id: 'RULE-003',
    rule_name: 'Geographic Anomaly Check',
    total_executions: 2340,
    successful_executions: 2180,
    fraud_detections: 89,
    false_positives: 12,
    average_execution_time: 25.3,
    success_rate: 93.2,
    trend: 'down' as const,
    trend_percentage: -0.5
  },
  {
    rule_id: 'RULE-004',
    rule_name: 'Machine Learning Fraud Model',
    total_executions: 45670,
    successful_executions: 44200,
    fraud_detections: 1245,
    false_positives: 89,
    average_execution_time: 45.2,
    success_rate: 96.8,
    trend: 'up' as const,
    trend_percentage: 5.2
  }
];

const fraudDetectionTrends = [
  { date: '2024-01-01', total_transactions: 1200, fraudulent: 12, blocked: 8, reviewed: 4 },
  { date: '2024-01-02', total_transactions: 1350, fraudulent: 15, blocked: 11, reviewed: 4 },
  { date: '2024-01-03', total_transactions: 1180, fraudulent: 8, blocked: 6, reviewed: 2 },
  { date: '2024-01-04', total_transactions: 1420, fraudulent: 18, blocked: 14, reviewed: 4 },
  { date: '2024-01-05', total_transactions: 1280, fraudulent: 11, blocked: 8, reviewed: 3 },
  { date: '2024-01-06', total_transactions: 1390, fraudulent: 16, blocked: 12, reviewed: 4 },
  { date: '2024-01-07', total_transactions: 1250, fraudulent: 9, blocked: 7, reviewed: 2 }
];

const riskDistribution = {
  low: 68.5,
  medium: 23.2,
  high: 6.8,
  critical: 1.5
};

const RuleAnalytics: React.FC = () => {
  const [timeRange, setTimeRange] = useState('7d');
  const [selectedMetric, setSelectedMetric] = useState('all');
  const [activeTab, setActiveTab] = useState('overview');

  const totalTransactions = fraudDetectionTrends.reduce((sum, day) => sum + day.total_transactions, 0);
  const totalFraudulent = fraudDetectionTrends.reduce((sum, day) => sum + day.fraudulent, 0);
  const totalBlocked = fraudDetectionTrends.reduce((sum, day) => sum + day.blocked, 0);
  const fraudRate = ((totalFraudulent / totalTransactions) * 100).toFixed(2);

  return (
    <div className="space-y-6">
      {/* Header with Filters */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold">Rule Analytics</h2>
          <p className="text-gray-600">Performance metrics and fraud detection analytics</p>
        </div>
        <div className="flex gap-2">
          <Select value={timeRange} onValueChange={setTimeRange}>
            <SelectTrigger className="w-32">
              <SelectValue />
            </SelectTrigger>
            <SelectContent>
              <SelectItem value="24h">Last 24h</SelectItem>
              <SelectItem value="7d">Last 7 days</SelectItem>
              <SelectItem value="30d">Last 30 days</SelectItem>
              <SelectItem value="90d">Last 90 days</SelectItem>
            </SelectContent>
          </Select>
          <Select value={selectedMetric} onValueChange={setSelectedMetric}>
            <SelectTrigger className="w-40">
              <SelectValue />
            </SelectTrigger>
            <SelectContent>
              <SelectItem value="all">All Metrics</SelectItem>
              <SelectItem value="performance">Performance</SelectItem>
              <SelectItem value="accuracy">Accuracy</SelectItem>
              <SelectItem value="efficiency">Efficiency</SelectItem>
            </SelectContent>
          </Select>
        </div>
      </div>

      {/* Key Metrics Cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Total Transactions</CardTitle>
            <Activity className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">{totalTransactions.toLocaleString()}</div>
            <p className="text-xs text-muted-foreground">
              Processed in {timeRange}
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Fraud Detection Rate</CardTitle>
            <AlertTriangle className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold text-red-600">{fraudRate}%</div>
            <p className="text-xs text-muted-foreground">
              {totalFraudulent} fraudulent transactions
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Transactions Blocked</CardTitle>
            <Shield className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold text-green-600">{totalBlocked}</div>
            <p className="text-xs text-muted-foreground">
              Prevented losses
            </p>
          </CardContent>
        </Card>

        <Card>
          <CardHeader className="flex flex-row items-center justify-between space-y-0 pb-2">
            <CardTitle className="text-sm font-medium">Average Response Time</CardTitle>
            <Zap className="h-4 w-4 text-muted-foreground" />
          </CardHeader>
          <CardContent>
            <div className="text-2xl font-bold">23.4ms</div>
            <p className="text-xs text-muted-foreground">
              Real-time processing
            </p>
          </CardContent>
        </Card>
      </div>

      {/* Risk Distribution */}
      <Card>
        <CardHeader>
          <CardTitle>Risk Distribution</CardTitle>
          <CardDescription>
            Distribution of transaction risk levels over the selected period
          </CardDescription>
        </CardHeader>
        <CardContent>
          <div className="space-y-4">
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <div className="w-4 h-4 bg-green-500 rounded"></div>
                <span className="text-sm">Low Risk</span>
              </div>
              <span className="text-sm font-medium">{riskDistribution.low}%</span>
            </div>
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div className="bg-green-500 h-2 rounded-full" style={{width: `${riskDistribution.low}%`}}></div>
            </div>

            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <div className="w-4 h-4 bg-yellow-500 rounded"></div>
                <span className="text-sm">Medium Risk</span>
              </div>
              <span className="text-sm font-medium">{riskDistribution.medium}%</span>
            </div>
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div className="bg-yellow-500 h-2 rounded-full" style={{width: `${riskDistribution.medium}%`}}></div>
            </div>

            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <div className="w-4 h-4 bg-orange-500 rounded"></div>
                <span className="text-sm">High Risk</span>
              </div>
              <span className="text-sm font-medium">{riskDistribution.high}%</span>
            </div>
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div className="bg-orange-500 h-2 rounded-full" style={{width: `${riskDistribution.high}%`}}></div>
            </div>

            <div className="flex items-center justify-between">
              <div className="flex items-center gap-2">
                <div className="w-4 h-4 bg-red-500 rounded"></div>
                <span className="text-sm">Critical Risk</span>
              </div>
              <span className="text-sm font-medium">{riskDistribution.critical}%</span>
            </div>
            <div className="w-full bg-gray-200 rounded-full h-2">
              <div className="bg-red-500 h-2 rounded-full" style={{width: `${riskDistribution.critical}%`}}></div>
            </div>
          </div>
        </CardContent>
      </Card>

      {/* Detailed Analytics Tabs */}
      <Tabs value={activeTab} onValueChange={setActiveTab}>
        <TabsList className="grid w-full grid-cols-3">
          <TabsTrigger value="overview">Rule Performance</TabsTrigger>
          <TabsTrigger value="trends">Detection Trends</TabsTrigger>
          <TabsTrigger value="insights">Insights & Reports</TabsTrigger>
        </TabsList>

        {/* Rule Performance Tab */}
        <TabsContent value="overview" className="space-y-6">
          <Card>
            <CardHeader>
              <CardTitle>Rule Performance Metrics</CardTitle>
              <CardDescription>
                Detailed performance statistics for each fraud detection rule
              </CardDescription>
            </CardHeader>
            <CardContent>
              <Table>
                <TableHeader>
                  <TableRow>
                    <TableHead>Rule</TableHead>
                    <TableHead>Executions</TableHead>
                    <TableHead>Success Rate</TableHead>
                    <TableHead>Fraud Detections</TableHead>
                    <TableHead>False Positives</TableHead>
                    <TableHead>Avg Time</TableHead>
                    <TableHead>Trend</TableHead>
                  </TableRow>
                </TableHeader>
                <TableBody>
                  {rulePerformanceData.map((rule) => (
                    <TableRow key={rule.rule_id}>
                      <TableCell>
                        <div>
                          <div className="font-medium">{rule.rule_name}</div>
                          <div className="text-sm text-gray-500">{rule.rule_id}</div>
                        </div>
                      </TableCell>
                      <TableCell>
                        <div className="font-medium">{rule.total_executions.toLocaleString()}</div>
                        <div className="text-sm text-gray-500">
                          {rule.successful_executions.toLocaleString()} successful
                        </div>
                      </TableCell>
                      <TableCell>
                        <div className="flex items-center gap-2">
                          <div className="font-medium">{rule.success_rate}%</div>
                          {rule.success_rate >= 95 ? (
                            <CheckCircle className="h-4 w-4 text-green-500" />
                          ) : rule.success_rate >= 90 ? (
                            <Clock className="h-4 w-4 text-yellow-500" />
                          ) : (
                            <AlertTriangle className="h-4 w-4 text-red-500" />
                          )}
                        </div>
                      </TableCell>
                      <TableCell>
                        <div className="font-medium text-red-600">{rule.fraud_detections}</div>
                      </TableCell>
                      <TableCell>
                        <div className="font-medium text-orange-600">{rule.false_positives}</div>
                      </TableCell>
                      <TableCell>
                        <div className="font-medium">{rule.average_execution_time}ms</div>
                      </TableCell>
                      <TableCell>
                        <div className="flex items-center gap-1">
                          {rule.trend === 'up' ? (
                            <TrendingUp className="h-4 w-4 text-green-500" />
                          ) : (
                            <TrendingDown className="h-4 w-4 text-red-500" />
                          )}
                          <span className={`text-sm ${rule.trend === 'up' ? 'text-green-600' : 'text-red-600'}`}>
                            {rule.trend_percentage > 0 ? '+' : ''}{rule.trend_percentage}%
                          </span>
                        </div>
                      </TableCell>
                    </TableRow>
                  ))}
                </TableBody>
              </Table>
            </CardContent>
          </Card>
        </TabsContent>

        {/* Detection Trends Tab */}
        <TabsContent value="trends" className="space-y-6">
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            <Card>
              <CardHeader>
                <CardTitle>Daily Fraud Detection</CardTitle>
                <CardDescription>
                  Fraudulent transactions detected over time
                </CardDescription>
              </CardHeader>
              <CardContent>
                <div className="space-y-4">
                  {fraudDetectionTrends.map((day) => (
                    <div key={day.date} className="flex items-center justify-between">
                      <div className="flex items-center gap-3">
                        <Calendar className="h-4 w-4 text-gray-400" />
                        <span className="text-sm">
                          {new Date(day.date).toLocaleDateString('en-US', {
                            month: 'short',
                            day: 'numeric'
                          })}
                        </span>
                      </div>
                      <div className="flex gap-4 text-sm">
                        <span className="text-gray-600">
                          {day.total_transactions} total
                        </span>
                        <span className="text-red-600 font-medium">
                          {day.fraudulent} fraudulent
                        </span>
                        <span className="text-green-600 font-medium">
                          {day.blocked} blocked
                        </span>
                      </div>
                    </div>
                  ))}
                </div>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <CardTitle>Performance Summary</CardTitle>
                <CardDescription>
                  Key performance indicators for the selected period
                </CardDescription>
              </CardHeader>
              <CardContent className="space-y-4">
                <div className="grid grid-cols-2 gap-4">
                  <div className="text-center p-4 border rounded-lg">
                    <div className="text-2xl font-bold text-green-600">
                      {((totalBlocked / totalFraudulent) * 100).toFixed(1)}%
                    </div>
                    <div className="text-sm text-gray-600">Block Rate</div>
                  </div>
                  <div className="text-center p-4 border rounded-lg">
                    <div className="text-2xl font-bold text-blue-600">
                      {((totalTransactions - totalFraudulent) / totalTransactions * 100).toFixed(1)}%
                    </div>
                    <div className="text-sm text-gray-600">Clean Rate</div>
                  </div>
                  <div className="text-center p-4 border rounded-lg">
                    <div className="text-2xl font-bold text-purple-600">
                      {(totalTransactions / fraudDetectionTrends.length).toFixed(0)}
                    </div>
                    <div className="text-sm text-gray-600">Daily Avg</div>
                  </div>
                  <div className="text-center p-4 border rounded-lg">
                    <div className="text-2xl font-bold text-orange-600">
                      {fraudDetectionTrends.length}
                    </div>
                    <div className="text-sm text-gray-600">Days Tracked</div>
                  </div>
                </div>
              </CardContent>
            </Card>
          </div>
        </TabsContent>

        {/* Insights & Reports Tab */}
        <TabsContent value="insights" className="space-y-6">
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            <Card>
              <CardHeader>
                <CardTitle>Top Performing Rules</CardTitle>
                <CardDescription>
                  Rules with highest fraud detection accuracy
                </CardDescription>
              </CardHeader>
              <CardContent>
                <div className="space-y-4">
                  {rulePerformanceData
                    .sort((a, b) => b.success_rate - a.success_rate)
                    .slice(0, 3)
                    .map((rule, index) => (
                      <div key={rule.rule_id} className="flex items-center justify-between p-3 border rounded-lg">
                        <div className="flex items-center gap-3">
                          <div className="w-8 h-8 bg-blue-100 rounded-full flex items-center justify-center text-sm font-bold">
                            {index + 1}
                          </div>
                          <div>
                            <div className="font-medium">{rule.rule_name}</div>
                            <div className="text-sm text-gray-500">
                              {rule.fraud_detections} detections
                            </div>
                          </div>
                        </div>
                        <Badge className="bg-green-100 text-green-800">
                          {rule.success_rate}% accuracy
                        </Badge>
                      </div>
                    ))}
                </div>
              </CardContent>
            </Card>

            <Card>
              <CardHeader>
                <CardTitle>Areas for Improvement</CardTitle>
                <CardDescription>
                  Rules that may need optimization
                </CardDescription>
              </CardHeader>
              <CardContent>
                <div className="space-y-4">
                  {rulePerformanceData
                    .filter(rule => rule.success_rate < 95 || rule.false_positives > 10)
                    .map((rule) => (
                      <div key={rule.rule_id} className="p-3 border border-orange-200 rounded-lg bg-orange-50">
                        <div className="flex items-center gap-2 mb-2">
                          <AlertTriangle className="h-4 w-4 text-orange-600" />
                          <span className="font-medium text-orange-800">{rule.rule_name}</span>
                        </div>
                        <div className="text-sm text-orange-700">
                          {rule.success_rate < 95 && `Low accuracy: ${rule.success_rate}%`}
                          {rule.success_rate < 95 && rule.false_positives > 10 && ' • '}
                          {rule.false_positives > 10 && `${rule.false_positives} false positives`}
                        </div>
                      </div>
                    ))}
                </div>
              </CardContent>
            </Card>
          </div>

          {/* Action Items */}
          <Card>
            <CardHeader>
              <CardTitle>Recommended Actions</CardTitle>
              <CardDescription>
                Suggestions to improve fraud detection performance
              </CardDescription>
            </CardHeader>
            <CardContent>
              <div className="space-y-3">
                <div className="flex items-start gap-3 p-3 border rounded-lg">
                  <Target className="h-5 w-5 text-blue-600 mt-0.5" />
                  <div>
                    <div className="font-medium">Optimize Rule Thresholds</div>
                    <div className="text-sm text-gray-600">
                      Adjust thresholds for rules with high false positive rates to improve accuracy
                    </div>
                  </div>
                </div>

                <div className="flex items-start gap-3 p-3 border rounded-lg">
                  <BarChart3 className="h-5 w-5 text-green-600 mt-0.5" />
                  <div>
                    <div className="font-medium">Enable Machine Learning Rules</div>
                    <div className="text-sm text-gray-600">
                      ML-based rules show 5.2% improvement trend - consider expanding their usage
                    </div>
                  </div>
                </div>

                <div className="flex items-start gap-3 p-3 border rounded-lg">
                  <Clock className="h-5 w-5 text-purple-600 mt-0.5" />
                  <div>
                    <div className="font-medium">Performance Monitoring</div>
                    <div className="text-sm text-gray-600">
                      Set up alerts for rules exceeding 50ms average execution time
                    </div>
                  </div>
                </div>
              </div>
            </CardContent>
          </Card>
        </TabsContent>
      </Tabs>
    </div>
  );
};

export default RuleAnalytics;