/**
 * Rule Engine Management Component
 * Comprehensive interface for managing fraud detection rules
 */

import React, { useState, useEffect } from 'react';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from '../ui/card';
import { Button } from '../ui/button';
import { Input } from '../ui/input';
import { Badge } from '../ui/badge';
import { Switch } from '../ui/switch';
import { Table, TableBody, TableCell, TableHead, TableHeader, TableRow } from '../ui/table';
import { Dialog, DialogContent, DialogDescription, DialogHeader, DialogTitle, DialogTrigger } from '../ui/dialog';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from '../ui/select';
import { Tabs, TabsContent, TabsList, TabsTrigger } from '../ui/tabs';
import {
  Search,
  Plus,
  Edit,
  Trash2,
  Play,
  Pause,
  Eye,
  Filter,
  MoreHorizontal,
  AlertCircle,
  CheckCircle,
  Clock,
  TrendingUp
} from 'lucide-react';
import { DropdownMenu, DropdownMenuContent, DropdownMenuItem, DropdownMenuTrigger } from '../ui/dropdown-menu';

// Mock data for rules - in production this would come from API
const mockRules = [
  {
    id: 'RULE-001',
    name: 'High Amount Transaction Check',
    description: 'Flag transactions above $10,000 for manual review',
    type: 'VALIDATION',
    priority: 'HIGH',
    isActive: true,
    executionCount: 15420,
    successRate: 98.5,
    lastExecuted: '2024-01-15T10:30:00Z',
    createdBy: 'admin',
    createdAt: '2024-01-01T00:00:00Z'
  },
  {
    id: 'RULE-002',
    name: 'Velocity Pattern Detection',
    description: 'Detect unusual transaction velocity patterns',
    type: 'PATTERN',
    priority: 'MEDIUM',
    isActive: true,
    executionCount: 8750,
    successRate: 94.2,
    lastExecuted: '2024-01-15T10:25:00Z',
    createdBy: 'fraud_team',
    createdAt: '2024-01-02T00:00:00Z'
  },
  {
    id: 'RULE-003',
    name: 'Geographic Anomaly Check',
    description: 'Flag transactions from unusual geographic locations',
    type: 'SCORING',
    priority: 'MEDIUM',
    isActive: false,
    executionCount: 2340,
    successRate: 91.8,
    lastExecuted: '2024-01-14T16:45:00Z',
    createdBy: 'security_team',
    createdAt: '2024-01-03T00:00:00Z'
  },
  {
    id: 'RULE-004',
    name: 'Machine Learning Fraud Model',
    description: 'Advanced ML-based fraud detection using transaction patterns',
    type: 'MACHINE_LEARNING',
    priority: 'CRITICAL',
    isActive: true,
    executionCount: 45670,
    successRate: 96.3,
    lastExecuted: '2024-01-15T10:32:00Z',
    createdBy: 'ml_team',
    createdAt: '2024-01-05T00:00:00Z'
  }
];

const RuleEngineManagement: React.FC = () => {
  const [rules, setRules] = useState(mockRules);
  const [filteredRules, setFilteredRules] = useState(mockRules);
  const [searchTerm, setSearchTerm] = useState('');
  const [filterType, setFilterType] = useState('all');
  const [filterPriority, setFilterPriority] = useState('all');
  const [filterStatus, setFilterStatus] = useState('all');
  const [selectedRule, setSelectedRule] = useState<any>(null);
  const [isLoading, setIsLoading] = useState(false);

  // Filter rules based on search and filters
  useEffect(() => {
    let filtered = rules.filter(rule => {
      const matchesSearch = rule.name.toLowerCase().includes(searchTerm.toLowerCase()) ||
                          rule.description.toLowerCase().includes(searchTerm.toLowerCase());
      const matchesType = filterType === 'all' || rule.type === filterType;
      const matchesPriority = filterPriority === 'all' || rule.priority === filterPriority;
      const matchesStatus = filterStatus === 'all' ||
                          (filterStatus === 'active' && rule.isActive) ||
                          (filterStatus === 'inactive' && !rule.isActive);

      return matchesSearch && matchesType && matchesPriority && matchesStatus;
    });

    setFilteredRules(filtered);
  }, [rules, searchTerm, filterType, filterPriority, filterStatus]);

  const getPriorityColor = (priority: string) => {
    switch (priority) {
      case 'CRITICAL': return 'bg-red-100 text-red-800 border-red-200';
      case 'HIGH': return 'bg-orange-100 text-orange-800 border-orange-200';
      case 'MEDIUM': return 'bg-yellow-100 text-yellow-800 border-yellow-200';
      case 'LOW': return 'bg-green-100 text-green-800 border-green-200';
      default: return 'bg-gray-100 text-gray-800 border-gray-200';
    }
  };

  const getTypeColor = (type: string) => {
    switch (type) {
      case 'VALIDATION': return 'bg-blue-100 text-blue-800';
      case 'SCORING': return 'bg-purple-100 text-purple-800';
      case 'PATTERN': return 'bg-indigo-100 text-indigo-800';
      case 'MACHINE_LEARNING': return 'bg-pink-100 text-pink-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const handleToggleRule = async (ruleId: string) => {
    setIsLoading(true);
    // In production, this would call the API
    setTimeout(() => {
      setRules(prev => prev.map(rule =>
        rule.id === ruleId ? { ...rule, isActive: !rule.isActive } : rule
      ));
      setIsLoading(false);
    }, 1000);
  };

  const handleDeleteRule = async (ruleId: string) => {
    if (!confirm('Are you sure you want to delete this rule?')) return;

    setIsLoading(true);
    // In production, this would call the API
    setTimeout(() => {
      setRules(prev => prev.filter(rule => rule.id !== ruleId));
      setIsLoading(false);
    }, 1000);
  };

  return (
    <div className="space-y-6">
      {/* Header with Actions */}
      <div className="flex items-center justify-between">
        <div>
          <h2 className="text-2xl font-bold">Rule Management</h2>
          <p className="text-gray-600">Manage and monitor fraud detection rules</p>
        </div>
        <Button className="flex items-center gap-2">
          <Plus className="h-4 w-4" />
          Create Rule
        </Button>
      </div>

      {/* Filters and Search */}
      <Card>
        <CardHeader>
          <CardTitle className="text-lg">Filters & Search</CardTitle>
        </CardHeader>
        <CardContent>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-5 gap-4">
            <div className="relative">
              <Search className="absolute left-3 top-1/2 transform -translate-y-1/2 text-gray-400 h-4 w-4" />
              <Input
                placeholder="Search rules..."
                value={searchTerm}
                onChange={(e) => setSearchTerm(e.target.value)}
                className="pl-10"
              />
            </div>

            <Select value={filterType} onValueChange={setFilterType}>
              <SelectTrigger>
                <SelectValue placeholder="Rule Type" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="all">All Types</SelectItem>
                <SelectItem value="VALIDATION">Validation</SelectItem>
                <SelectItem value="SCORING">Scoring</SelectItem>
                <SelectItem value="PATTERN">Pattern</SelectItem>
                <SelectItem value="MACHINE_LEARNING">ML</SelectItem>
              </SelectContent>
            </Select>

            <Select value={filterPriority} onValueChange={setFilterPriority}>
              <SelectTrigger>
                <SelectValue placeholder="Priority" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="all">All Priorities</SelectItem>
                <SelectItem value="CRITICAL">Critical</SelectItem>
                <SelectItem value="HIGH">High</SelectItem>
                <SelectItem value="MEDIUM">Medium</SelectItem>
                <SelectItem value="LOW">Low</SelectItem>
              </SelectContent>
            </Select>

            <Select value={filterStatus} onValueChange={setFilterStatus}>
              <SelectTrigger>
                <SelectValue placeholder="Status" />
              </SelectTrigger>
              <SelectContent>
                <SelectItem value="all">All Status</SelectItem>
                <SelectItem value="active">Active</SelectItem>
                <SelectItem value="inactive">Inactive</SelectItem>
              </SelectContent>
            </Select>

            <Button variant="outline" onClick={() => {
              setSearchTerm('');
              setFilterType('all');
              setFilterPriority('all');
              setFilterStatus('all');
            }}>
              Clear Filters
            </Button>
          </div>
        </CardContent>
      </Card>

      {/* Rules Table */}
      <Card>
        <CardHeader>
          <CardTitle className="flex items-center gap-2">
            <Filter className="h-5 w-5" />
            Rules ({filteredRules.length})
          </CardTitle>
          <CardDescription>
            Showing {filteredRules.length} of {rules.length} rules
          </CardDescription>
        </CardHeader>
        <CardContent>
          <Table>
            <TableHeader>
              <TableRow>
                <TableHead>Rule</TableHead>
                <TableHead>Type</TableHead>
                <TableHead>Priority</TableHead>
                <TableHead>Status</TableHead>
                <TableHead>Performance</TableHead>
                <TableHead>Last Executed</TableHead>
                <TableHead>Actions</TableHead>
              </TableRow>
            </TableHeader>
            <TableBody>
              {filteredRules.map((rule) => (
                <TableRow key={rule.id}>
                  <TableCell>
                    <div>
                      <div className="font-medium">{rule.name}</div>
                      <div className="text-sm text-gray-500">{rule.id}</div>
                      <div className="text-xs text-gray-400 mt-1">
                        {rule.description.length > 50
                          ? rule.description.substring(0, 50) + '...'
                          : rule.description
                        }
                      </div>
                    </div>
                  </TableCell>
                  <TableCell>
                    <Badge className={getTypeColor(rule.type)}>
                      {rule.type}
                    </Badge>
                  </TableCell>
                  <TableCell>
                    <Badge className={getPriorityColor(rule.priority)}>
                      {rule.priority}
                    </Badge>
                  </TableCell>
                  <TableCell>
                    <div className="flex items-center gap-2">
                      <Switch
                        checked={rule.isActive}
                        onCheckedChange={() => handleToggleRule(rule.id)}
                        disabled={isLoading}
                      />
                      <span className="text-sm">
                        {rule.isActive ? 'Active' : 'Inactive'}
                      </span>
                    </div>
                  </TableCell>
                  <TableCell>
                    <div className="space-y-1">
                      <div className="text-sm">
                        <span className="font-medium">{rule.executionCount.toLocaleString()}</span>
                        <span className="text-gray-500"> executions</span>
                      </div>
                      <div className="flex items-center gap-1">
                        <CheckCircle className="h-3 w-3 text-green-500" />
                        <span className="text-sm">{rule.successRate}% success</span>
                      </div>
                    </div>
                  </TableCell>
                  <TableCell>
                    <div className="text-sm">
                      {new Date(rule.lastExecuted).toLocaleDateString()}
                    </div>
                    <div className="text-xs text-gray-500">
                      {new Date(rule.lastExecuted).toLocaleTimeString()}
                    </div>
                  </TableCell>
                  <TableCell>
                    <DropdownMenu>
                      <DropdownMenuTrigger asChild>
                        <Button variant="ghost" size="sm">
                          <MoreHorizontal className="h-4 w-4" />
                        </Button>
                      </DropdownMenuTrigger>
                      <DropdownMenuContent align="end">
                        <DropdownMenuItem onClick={() => setSelectedRule(rule)}>
                          <Eye className="h-4 w-4 mr-2" />
                          View Details
                        </DropdownMenuItem>
                        <DropdownMenuItem>
                          <Edit className="h-4 w-4 mr-2" />
                          Edit Rule
                        </DropdownMenuItem>
                        <DropdownMenuItem>
                          <Play className="h-4 w-4 mr-2" />
                          Test Rule
                        </DropdownMenuItem>
                        <DropdownMenuItem
                          onClick={() => handleDeleteRule(rule.id)}
                          className="text-red-600"
                        >
                          <Trash2 className="h-4 w-4 mr-2" />
                          Delete Rule
                        </DropdownMenuItem>
                      </DropdownMenuContent>
                    </DropdownMenu>
                  </TableCell>
                </TableRow>
              ))}
            </TableBody>
          </Table>
        </CardContent>
      </Card>

      {/* Rule Details Dialog */}
      <Dialog open={!!selectedRule} onOpenChange={() => setSelectedRule(null)}>
        <DialogContent className="max-w-4xl max-h-[80vh] overflow-y-auto">
          <DialogHeader>
            <DialogTitle>Rule Details: {selectedRule?.name}</DialogTitle>
            <DialogDescription>
              Comprehensive information about this fraud detection rule
            </DialogDescription>
          </DialogHeader>

          {selectedRule && (
            <div className="space-y-6">
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="text-sm font-medium">Rule ID</label>
                  <p className="text-sm text-gray-600">{selectedRule.id}</p>
                </div>
                <div>
                  <label className="text-sm font-medium">Type</label>
                  <Badge className={getTypeColor(selectedRule.type)}>
                    {selectedRule.type}
                  </Badge>
                </div>
                <div>
                  <label className="text-sm font-medium">Priority</label>
                  <Badge className={getPriorityColor(selectedRule.priority)}>
                    {selectedRule.priority}
                  </Badge>
                </div>
                <div>
                  <label className="text-sm font-medium">Status</label>
                  <div className="flex items-center gap-2">
                    <div className={`w-2 h-2 rounded-full ${selectedRule.isActive ? 'bg-green-500' : 'bg-red-500'}`} />
                    {selectedRule.isActive ? 'Active' : 'Inactive'}
                  </div>
                </div>
              </div>

              <div>
                <label className="text-sm font-medium">Description</label>
                <p className="text-sm text-gray-600 mt-1">{selectedRule.description}</p>
              </div>

              <div className="grid grid-cols-3 gap-4">
                <Card>
                  <CardContent className="pt-4">
                    <div className="text-2xl font-bold">{selectedRule.executionCount.toLocaleString()}</div>
                    <p className="text-xs text-gray-600">Total Executions</p>
                  </CardContent>
                </Card>
                <Card>
                  <CardContent className="pt-4">
                    <div className="text-2xl font-bold">{selectedRule.successRate}%</div>
                    <p className="text-xs text-gray-600">Success Rate</p>
                  </CardContent>
                </Card>
                <Card>
                  <CardContent className="pt-4">
                    <div className="text-2xl font-bold text-sm">
                      {new Date(selectedRule.lastExecuted).toLocaleDateString()}
                    </div>
                    <p className="text-xs text-gray-600">Last Executed</p>
                  </CardContent>
                </Card>
              </div>
            </div>
          )}
        </DialogContent>
      </Dialog>
    </div>
  );
};

export default RuleEngineManagement;
