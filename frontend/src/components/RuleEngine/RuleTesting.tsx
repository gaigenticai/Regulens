/**
 * Rule Testing Component
 * Interface for testing fraud detection rules with sample data
 */

import React, { useState } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../ui/card';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Textarea } from '../ui/textarea';
import { Label } from '../ui/label';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '../ui/select';
import { Badge } from '../ui/badge';
import { Alert, AlertDescription } from '../ui/alert';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '../ui/tabs';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '../ui/table';
import {
  Play,
  TestTube,
  CheckCircle,
  XCircle,
  AlertTriangle,
  Clock,
  Database,
  FileText,
  RefreshCw,
  Download,
  Upload
} from 'lucide-react';

interface TestTransaction {
  id: string;
  amount: number;
  currency: string;
  user_id: string;
  timestamp: string;
  merchant: string;
  location: string;
  device_fingerprint?: string;
  ip_address?: string;
  user_agent?: string;
}

interface TestResult {
  rule_id: string;
  rule_name: string;
  result: 'PASS' | 'FAIL' | 'ERROR';
  confidence_score: number;
  risk_level: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  execution_time_ms: number;
  triggered_conditions?: string[];
  error_message?: string;
}

interface BatchTestResult {
  transaction_id: string;
  is_fraudulent: boolean;
  overall_risk: 'LOW' | 'MEDIUM' | 'HIGH' | 'CRITICAL';
  fraud_score: number;
  rule_results: TestResult[];
  processing_time_ms: number;
}

// Mock test data
const sampleTransactions: TestTransaction[] = [
  {
    id: 'TXN-001',
    amount: 15000.00,
    currency: 'USD',
    user_id: 'USER-123',
    timestamp: '2024-01-15T14:30:00Z',
    merchant: 'Electronics Store',
    location: 'New York, NY',
    device_fingerprint: 'abc123def456',
    ip_address: '192.168.1.100'
  },
  {
    id: 'TXN-002',
    amount: 2500.00,
    currency: 'USD',
    user_id: 'USER-456',
    timestamp: '2024-01-15T14:35:00Z',
    merchant: 'Online Retailer',
    location: 'Los Angeles, CA',
    device_fingerprint: 'xyz789uvw123',
    ip_address: '10.0.0.50'
  },
  {
    id: 'TXN-003',
    amount: 50000.00,
    currency: 'USD',
    user_id: 'USER-789',
    timestamp: '2024-01-15T14:40:00Z',
    merchant: 'Luxury Goods Store',
    location: 'Miami, FL',
    device_fingerprint: 'def456ghi789',
    ip_address: '172.16.0.25'
  }
];

const mockRules = [
  { id: 'RULE-001', name: 'High Amount Check' },
  { id: 'RULE-002', name: 'Velocity Pattern' },
  { id: 'RULE-003', name: 'Geographic Anomaly' }
];

const RuleTesting: React.FC = () => {
  const [selectedRule, setSelectedRule] = useState('');
  const [testTransaction, setTestTransaction] = useState<TestTransaction>(sampleTransactions[0]);
  const [customTransactionData, setCustomTransactionData] = useState('');
  const [testResults, setTestResults] = useState<TestResult[]>([]);
  const [batchResults, setBatchResults] = useState<BatchTestResult[]>([]);
  const [isRunningTest, setIsRunningTest] = useState(false);
  const [isRunningBatch, setIsRunningBatch] = useState(false);
  const [activeTab, setActiveTab] = useState('single');
  const [selectedTransactions, setSelectedTransactions] = useState<string[]>([]);

  const handleSingleTest = async () => {
    if (!selectedRule) {
      alert('Please select a rule to test');
      return;
    }

    setIsRunningTest(true);

    try {
      // In production, this would call the API
      console.log('Testing rule:', selectedRule, 'with transaction:', testTransaction);

      // Simulate API call
      await new Promise(resolve => setTimeout(resolve, 1500));

      // Mock result
      const mockResult: TestResult = {
        rule_id: selectedRule,
        rule_name: mockRules.find(r => r.id === selectedRule)?.name || 'Unknown Rule',
        result: Math.random() > 0.7 ? 'FAIL' : 'PASS',
        confidence_score: Math.random() * 100,
        risk_level: ['LOW', 'MEDIUM', 'HIGH', 'CRITICAL'][Math.floor(Math.random() * 4)] as any,
        execution_time_ms: Math.floor(Math.random() * 50) + 10,
        triggered_conditions: Math.random() > 0.5 ? ['High amount detected', 'Unusual location'] : undefined
      };

      setTestResults([mockResult]);

    } catch (error) {
      console.error('Test failed:', error);
      alert('Test execution failed');
    } finally {
      setIsRunningTest(false);
    }
  };

  const handleBatchTest = async () => {
    if (!selectedRule) {
      alert('Please select a rule to test');
      return;
    }

    if (selectedTransactions.length === 0) {
      alert('Please select at least one transaction to test');
      return;
    }

    setIsRunningBatch(true);

    try {
      // In production, this would call the batch API
      console.log('Batch testing rule:', selectedRule, 'with transactions:', selectedTransactions);

      // Simulate API call
      await new Promise(resolve => setTimeout(resolve, 3000));

      // Mock batch results
      const mockBatchResults: BatchTestResult[] = selectedTransactions.map(txnId => {
        const txn = sampleTransactions.find(t => t.id === txnId);
        return {
          transaction_id: txnId,
          is_fraudulent: Math.random() > 0.8,
          overall_risk: ['LOW', 'MEDIUM', 'HIGH', 'CRITICAL'][Math.floor(Math.random() * 4)] as any,
          fraud_score: Math.random() * 100,
          rule_results: [{
            rule_id: selectedRule,
            rule_name: mockRules.find(r => r.id === selectedRule)?.name || 'Unknown Rule',
            result: Math.random() > 0.7 ? 'FAIL' : 'PASS',
            confidence_score: Math.random() * 100,
            risk_level: ['LOW', 'MEDIUM', 'HIGH', 'CRITICAL'][Math.floor(Math.random() * 4)] as any,
            execution_time_ms: Math.floor(Math.random() * 50) + 10
          }],
          processing_time_ms: Math.floor(Math.random() * 100) + 20
        };
      });

      setBatchResults(mockBatchResults);

    } catch (error) {
      console.error('Batch test failed:', error);
      alert('Batch test execution failed');
    } finally {
      setIsRunningBatch(false);
    }
  };

  const loadCustomTransaction = () => {
    try {
      const parsed = JSON.parse(customTransactionData);
      setTestTransaction(parsed);
    } catch (error) {
      alert('Invalid JSON format');
    }
  };

  const getResultIcon = (result: string) => {
    switch (result) {
      case 'PASS': return <CheckCircle className="h-4 w-4 text-green-500" />;
      case 'FAIL': return <XCircle className="h-4 w-4 text-red-500" />;
      case 'ERROR': return <AlertTriangle className="h-4 w-4 text-orange-500" />;
      default: return <Clock className="h-4 w-4 text-gray-500" />;
    }
  };

  const getRiskColor = (risk: string) => {
    switch (risk) {
      case 'LOW': return 'bg-green-100 text-green-800';
      case 'MEDIUM': return 'bg-yellow-100 text-yellow-800';
      case 'HIGH': return 'bg-orange-100 text-orange-800';
      case 'CRITICAL': return 'bg-red-100 text-red-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold">Rule Testing</h2>
          <p className="text-gray-600">Test fraud detection rules with sample transaction data</p>
        </div>
        <div className="flex gap-2">
          <Button variant="outline" size="sm">
            <Download className="h-4 w-4 mr-2" />
            Export Results
          </Button>
          <Button variant="outline" size="sm">
            <Upload className="h-4 w-4 mr-2" />
            Load Test Data
          </Button>
        </div>
      </div>

      {/* Rule Selection */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <TestTube className="h-5 w-5" />
            Test Configuration
          </CardTitle>
          <CardDescription>
            Select a rule and configure test parameters
          </CardDescription>
        </CardHeader>
        <CardContent>
          <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
            <div className="space-y-2">
              <Label htmlFor="rule-select">Select Rule *</Label>
              <Select value={selectedRule} onValueChange={setSelectedRule}>
                <SelectTrigger>
                </SelectTrigger>
                <SelectContent>
                  {mockRules.map((rule) => (
                    <SelectItem key={rule.id} value={rule.id}>
                      {rule.name} ({rule.id})
                    </SelectItem>
                  ))}
                </SelectContent>
              </Select>
            </div>

            <div className="flex items-end">
              <Button
                onClick={handleSingleTest}
                disabled={isRunningTest || !selectedRule}
                className="w-full"
              >
                {isRunningTest ? (
                  <>
                    <RefreshCw className="h-4 w-4 mr-2 animate-spin" />
                    Running Test...
                  </>
                ) : (
                  <>
                    <Play className="h-4 w-4 mr-2" />
                    Run Single Test
                  </>
                )}
              </Button>
            </div>
          </div>
        </CardContent>
      </Card>

      {/* Test Interface */}
      <Tabs value={activeTab} onValueChange={setActiveTab}>
        <TabsList className="grid w-full grid-cols-2">
          <TabsTrigger value="single">Single Test</TabsTrigger>
          <TabsTrigger value="batch">Batch Test</TabsTrigger>
        </TabsList>

        {/* Single Test Tab */}
        <TabsContent value="single" className="space-y-6">
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            {/* Test Data Input */}
            <Card>
              <CardHeader>
                <CardTitle className="flex items-center gap-2">
                  <Database className="h-5 w-5" />
                  Test Transaction Data
                </CardTitle>
                <CardDescription>
                  Configure the transaction data for testing
                </CardDescription>
              </CardHeader>
              <CardContent className="space-y-4">
                <Tabs defaultValue="sample" className="w-full">
                  <TabsList className="grid w-full grid-cols-2">
                    <TabsTrigger value="sample">Sample Data</TabsTrigger>
                    <TabsTrigger value="custom">Custom JSON</TabsTrigger>
                  </TabsList>

                  <TabsContent value="sample" className="space-y-4">
                    <Select
                      value={testTransaction.id}
                      onValueChange={(value) => {
                        const txn = sampleTransactions.find(t => t.id === value);
                        if (txn) setTestTransaction(txn);
                      }}
                    >
                      <SelectTrigger>
                      </SelectTrigger>
                      <SelectContent>
                        {sampleTransactions.map((txn) => (
                          <SelectItem key={txn.id} value={txn.id}>
                            {txn.id} - ${txn.amount} ({txn.merchant})
                          </SelectItem>
                        ))}
                      </SelectContent>
                    </Select>

                    <div className="grid grid-cols-2 gap-4 text-sm">
                      <div>
                        <Label className="text-xs text-gray-500">Amount</Label>
                        <p className="font-medium">${testTransaction.amount.toLocaleString()}</p>
                      </div>
                      <div>
                        <Label className="text-xs text-gray-500">Merchant</Label>
                        <p className="font-medium">{testTransaction.merchant}</p>
                      </div>
                      <div>
                        <Label className="text-xs text-gray-500">Location</Label>
                        <p className="font-medium">{testTransaction.location}</p>
                      </div>
                      <div>
                        <Label className="text-xs text-gray-500">User ID</Label>
                        <p className="font-medium">{testTransaction.user_id}</p>
                      </div>
                    </div>
                  </TabsContent>

                  <TabsContent value="custom" className="space-y-4">
                    <Textarea
                      value={customTransactionData}
                      onChange={(e) => setCustomTransactionData(e.target.value)}
                      rows={10}
                    />
                    <Button variant="outline" onClick={loadCustomTransaction}>
                      Load Custom Data
                    </Button>
                  </TabsContent>
                </Tabs>
              </CardContent>
            </Card>

            {/* Test Results */}
            <Card>
              <CardHeader>
                <CardTitle>Test Results</CardTitle>
                <CardDescription>
                  Rule evaluation results and analysis
                </CardDescription>
              </CardHeader>
              <CardContent>
                {testResults.length === 0 ? (
                  <div className="text-center text-gray-500 py-8">
                    <TestTube className="h-12 w-12 mx-auto mb-4 opacity-50" />
                    <p>Run a test to see results here</p>
                  </div>
                ) : (
                  <div className="space-y-4">
                    {testResults.map((result, index) => (
                      <div key={index} className="border rounded-lg p-4">
                        <div className="flex items-center justify-between mb-2">
                          <div className="flex items-center gap-2">
                            {getResultIcon(result.result)}
                            <span className="font-medium">{result.rule_name}</span>
                          </div>
                          <Badge className={getRiskColor(result.risk_level)}>
                            {result.risk_level}
                          </Badge>
                        </div>

                        <div className="grid grid-cols-2 gap-4 text-sm">
                          <div>
                            <Label className="text-xs text-gray-500">Confidence</Label>
                            <p className="font-medium">{result.confidence_score.toFixed(1)}%</p>
                          </div>
                          <div>
                            <Label className="text-xs text-gray-500">Execution Time</Label>
                            <p className="font-medium">{result.execution_time_ms}ms</p>
                          </div>
                        </div>

                        {result.triggered_conditions && result.triggered_conditions.length > 0 && (
                          <div className="mt-3">
                            <Label className="text-xs text-gray-500">Triggered Conditions</Label>
                            <div className="flex flex-wrap gap-1 mt-1">
                              {result.triggered_conditions.map((condition, idx) => (
                                <Badge key={idx} variant="outline" className="text-xs">
                                  {condition}
                                </Badge>
                              ))}
                            </div>
                          </div>
                        )}

                        {result.error_message && (
                          <Alert className="mt-3">
                            <AlertTriangle className="h-4 w-4" />
                            <AlertDescription className="text-sm">
                              {result.error_message}
                            </AlertDescription>
                          </Alert>
                        )}
                      </div>
                    ))}
                  </div>
                )}
              </CardContent>
            </Card>
          </div>
        </TabsContent>

        {/* Batch Test Tab */}
        <TabsContent value="batch" className="space-y-6">
          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            {/* Transaction Selection */}
            <Card>
              <CardHeader>
                <CardTitle>Select Transactions</CardTitle>
                <CardDescription>
                  Choose transactions to test in batch
                </CardDescription>
              </CardHeader>
              <CardContent>
                <Table>
                  <TableHeader>
                    <TableRow>
                      <TableHead className="w-12">
                        <input
                          type="checkbox"
                          checked={selectedTransactions.length === sampleTransactions.length}
                          onChange={(e) => {
                            if (e.target.checked) {
                              setSelectedTransactions(sampleTransactions.map(t => t.id));
                            } else {
                              setSelectedTransactions([]);
                            }
                          }}
                        />
                      </TableHead>
                      <TableHead>Transaction</TableHead>
                      <TableHead>Amount</TableHead>
                      <TableHead>Merchant</TableHead>
                    </TableRow>
                  </TableHeader>
                  <TableBody>
                    {sampleTransactions.map((txn) => (
                      <TableRow key={txn.id}>
                        <TableCell>
                          <input
                            type="checkbox"
                            checked={selectedTransactions.includes(txn.id)}
                            onChange={(e) => {
                              if (e.target.checked) {
                                setSelectedTransactions(prev => [...prev, txn.id]);
                              } else {
                                setSelectedTransactions(prev => prev.filter(id => id !== txn.id));
                              }
                            }}
                          />
                        </TableCell>
                        <TableCell className="font-medium">{txn.id}</TableCell>
                        <TableCell>${txn.amount.toLocaleString()}</TableCell>
                        <TableCell>{txn.merchant}</TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>

                <div className="mt-4">
                  <Button
                    onClick={handleBatchTest}
                    disabled={isRunningBatch || !selectedRule || selectedTransactions.length === 0}
                    className="w-full"
                  >
                    {isRunningBatch ? (
                      <>
                        <RefreshCw className="h-4 w-4 mr-2 animate-spin" />
                        Running Batch Test...
                      </>
                    ) : (
                      <>
                        <Play className="h-4 w-4 mr-2" />
                        Run Batch Test ({selectedTransactions.length} transactions)
                      </>
                    )}
                  </Button>
                </div>
              </CardContent>
            </Card>

            {/* Batch Results */}
            <Card>
              <CardHeader>
                <CardTitle>Batch Test Results</CardTitle>
                <CardDescription>
                  Results from batch rule evaluation
                </CardDescription>
              </CardHeader>
              <CardContent>
                {batchResults.length === 0 ? (
                  <div className="text-center text-gray-500 py-8">
                    <FileText className="h-12 w-12 mx-auto mb-4 opacity-50" />
                    <p>Run a batch test to see results here</p>
                  </div>
                ) : (
                  <div className="space-y-4">
                    {batchResults.map((result) => (
                      <div key={result.transaction_id} className="border rounded-lg p-4">
                        <div className="flex items-center justify-between mb-2">
                          <span className="font-medium">{result.transaction_id}</span>
                          <div className="flex items-center gap-2">
                            {result.is_fraudulent ? (
                              <XCircle className="h-4 w-4 text-red-500" />
                            ) : (
                              <CheckCircle className="h-4 w-4 text-green-500" />
                            )}
                            <Badge className={getRiskColor(result.overall_risk)}>
                              {result.overall_risk}
                            </Badge>
                          </div>
                        </div>

                        <div className="grid grid-cols-2 gap-4 text-sm">
                          <div>
                            <Label className="text-xs text-gray-500">Fraud Score</Label>
                            <p className="font-medium">{result.fraud_score.toFixed(1)}%</p>
                          </div>
                          <div>
                            <Label className="text-xs text-gray-500">Processing Time</Label>
                            <p className="font-medium">{result.processing_time_ms}ms</p>
                          </div>
                        </div>

                        <div className="mt-3">
                          <Label className="text-xs text-gray-500">Rule Results</Label>
                          <div className="flex gap-2 mt-1">
                            {result.rule_results.map((ruleResult, idx) => (
                              <Badge
                                key={idx}
                                variant={ruleResult.result === 'PASS' ? 'default' : 'destructive'}
                                className="text-xs"
                              >
                                {ruleResult.result}
                              </Badge>
                            ))}
                          </div>
                        </div>
                      </div>
                    ))}

                    {/* Summary */}
                    <div className="border-t pt-4">
                      <div className="grid grid-cols-4 gap-4 text-center">
                        <div>
                          <div className="text-2xl font-bold text-green-600">
                            {batchResults.filter(r => !r.is_fraudulent).length}
                          </div>
                          <div className="text-xs text-gray-500">Clean</div>
                        </div>
                        <div>
                          <div className="text-2xl font-bold text-red-600">
                            {batchResults.filter(r => r.is_fraudulent).length}
                          </div>
                          <div className="text-xs text-gray-500">Flagged</div>
                        </div>
                        <div>
                          <div className="text-2xl font-bold">
                            {(batchResults.reduce((sum, r) => sum + r.processing_time_ms, 0) / batchResults.length).toFixed(0)}ms
                          </div>
                          <div className="text-xs text-gray-500">Avg Time</div>
                        </div>
                        <div>
                          <div className="text-2xl font-bold">
                            {(batchResults.reduce((sum, r) => sum + r.fraud_score, 0) / batchResults.length).toFixed(1)}%
                          </div>
                          <div className="text-xs text-gray-500">Avg Score</div>
                        </div>
                      </div>
                    </div>
                  </div>
                )}
              </CardContent>
            </Card>
          </div>
        </TabsContent>
      </Tabs>
    </div>
  );
};

export default RuleTesting;