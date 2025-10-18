import React, { useState } from 'react';
import { Plus, Edit, Trash2, Play, Save, X } from 'lucide-react';

interface TestSuite {
  id: string;
  name: string;
  description: string;
  category: string;
  tools: TestCase[];
  created_at: string;
  updated_at: string;
  last_run?: string;
  results?: TestResult[];
  passed?: number;
  failed?: number;
}

interface TestCase {
  id: string;
  tool_name: string;
  operation: string;
  parameters: Record<string, any>;
  expected_result?: any;
}

interface TestResult {
  test_case_id: string;
  passed: boolean;
  result: any;
  error: string | null;
  execution_time: number;
}

interface TestSuiteManagementProps {
  categoryId: string;
  categoryName: string;
}

export const TestSuiteManagement: React.FC<TestSuiteManagementProps> = ({
  categoryId,
  categoryName
}) => {
  const [testSuites, setTestSuites] = useState<TestSuite[]>([]);
  const [isCreating, setIsCreating] = useState(false);
  const [editingSuite, setEditingSuite] = useState<TestSuite | null>(null);
  const [newSuiteName, setNewSuiteName] = useState('');
  const [newSuiteDescription, setNewSuiteDescription] = useState('');

  const createTestSuite = () => {
    if (!newSuiteName.trim()) return;

    const newSuite: TestSuite = {
      id: `suite_${Date.now()}`,
      name: newSuiteName.trim(),
      description: newSuiteDescription.trim(),
      category: categoryId,
      tools: [],
      created_at: new Date().toISOString(),
      updated_at: new Date().toISOString(),
    };

    setTestSuites(prev => [...prev, newSuite]);
    setNewSuiteName('');
    setNewSuiteDescription('');
    setIsCreating(false);
  };

  const updateTestSuite = () => {
    if (!editingSuite || !newSuiteName.trim()) return;

    const updatedSuite: TestSuite = {
      ...editingSuite,
      name: newSuiteName.trim(),
      description: newSuiteDescription.trim(),
      updated_at: new Date().toISOString(),
    };

    setTestSuites(prev => prev.map(suite =>
      suite.id === editingSuite.id ? updatedSuite : suite
    ));
    setEditingSuite(null);
    setNewSuiteName('');
    setNewSuiteDescription('');
  };

  const deleteTestSuite = (suiteId: string) => {
    setTestSuites(prev => prev.filter(suite => suite.id !== suiteId));
  };

  const startEdit = (suite: TestSuite) => {
    setEditingSuite(suite);
    setNewSuiteName(suite.name);
    setNewSuiteDescription(suite.description);
  };

  const cancelEdit = () => {
    setEditingSuite(null);
    setNewSuiteName('');
    setNewSuiteDescription('');
  };

  const runTestSuite = async (suite: TestSuite) => {
    try {
      console.log('Running test suite:', suite.name);

      // Execute each test case in the suite
      const results = [];
      for (const testCase of suite.tools) {
        const result = await executeTestCase(testCase);
        results.push(result);
      }

      // Update suite with results
      const updatedSuite = {
        ...suite,
        last_run: new Date().toISOString(),
        results: results,
        passed: results.filter(r => r.passed).length,
        failed: results.filter(r => !r.passed).length
      };

      setTestSuites(prev => prev.map(s =>
        s.id === suite.id ? updatedSuite : s
      ));

      console.log('Test suite completed:', updatedSuite);
    } catch (error) {
      console.error('Failed to run test suite:', error);
    }
  };

  const executeTestCase = async (testCase: TestCase): Promise<TestResult> => {
    try {
      // Make API call to execute the test case
      const response = await fetch('/api/tools/test', {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          'Authorization': `Bearer ${localStorage.getItem('token') || 'demo-token'}`
        },
        body: JSON.stringify({
          tool_name: testCase.tool_name,
          operation: testCase.operation,
          parameters: testCase.parameters
        })
      });

      const result = await response.json();

      return {
        test_case_id: testCase.id,
        passed: response.ok && (!testCase.expected_result ||
                JSON.stringify(result) === JSON.stringify(testCase.expected_result)),
        result: result,
        error: response.ok ? null : result.error,
        execution_time: Date.now()
      };
    } catch (error) {
      return {
        test_case_id: testCase.id,
        passed: false,
        result: null,
        error: error instanceof Error ? error.message : 'Unknown error',
        execution_time: Date.now()
      };
    }
  };

  const addTestCase = (suiteId: string) => {
    const newTestCase: TestCase = {
      id: `test_${Date.now()}`,
      tool_name: '',
      operation: '',
      parameters: {}
    };

    setTestSuites(prev => prev.map(suite =>
      suite.id === suiteId
        ? { ...suite, tools: [...suite.tools, newTestCase] }
        : suite
    ));
  };

  return (
    <div className="space-y-6">
      {/* Header */}
      <div className="flex justify-between items-center">
        <div>
          <h3 className="text-lg font-semibold text-gray-900">
            Test Suites for {categoryName}
          </h3>
          <p className="text-gray-600 mt-1">
            Create and manage test suites for automated testing
          </p>
        </div>
        <button
          onClick={() => setIsCreating(true)}
          className="flex items-center gap-2 bg-blue-600 text-white px-4 py-2 rounded-lg hover:bg-blue-700 transition-colors"
        >
          <Plus className="w-4 h-4" />
          Create Test Suite
        </button>
      </div>

      {/* Create/Edit Form */}
      {(isCreating || editingSuite) && (
        <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-6">
          <h4 className="text-md font-semibold text-gray-900 mb-4">
            {isCreating ? 'Create New Test Suite' : 'Edit Test Suite'}
          </h4>
          <div className="space-y-4">
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Suite Name
              </label>
              <input
                type="text"
                value={newSuiteName}
                onChange={(e) => setNewSuiteName(e.target.value)}
                className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                placeholder="Enter test suite name"
              />
            </div>
            <div>
              <label className="block text-sm font-medium text-gray-700 mb-1">
                Description
              </label>
              <textarea
                value={newSuiteDescription}
                onChange={(e) => setNewSuiteDescription(e.target.value)}
                rows={3}
                className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500 focus:border-transparent"
                placeholder="Describe what this test suite covers"
              />
            </div>
            <div className="flex gap-3">
              <button
                onClick={isCreating ? createTestSuite : updateTestSuite}
                className="flex items-center gap-2 bg-green-600 text-white px-4 py-2 rounded-lg hover:bg-green-700 transition-colors"
              >
                <Save className="w-4 h-4" />
                {isCreating ? 'Create' : 'Update'}
              </button>
              <button
                onClick={() => {
                  setIsCreating(false);
                  cancelEdit();
                }}
                className="flex items-center gap-2 bg-gray-600 text-white px-4 py-2 rounded-lg hover:bg-gray-700 transition-colors"
              >
                <X className="w-4 h-4" />
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}

      {/* Test Suites List */}
      <div className="space-y-4">
        {testSuites.length === 0 ? (
          <div className="bg-white rounded-lg shadow-sm border border-gray-200 p-8 text-center">
            <div className="text-gray-400 mb-4">
              <Plus className="w-12 h-12 mx-auto" />
            </div>
            <h4 className="text-lg font-medium text-gray-900 mb-2">No Test Suites</h4>
            <p className="text-gray-600 mb-4">
              Create your first test suite to start automated testing for {categoryName}.
            </p>
            <button
              onClick={() => setIsCreating(true)}
              className="bg-blue-600 text-white px-4 py-2 rounded-lg hover:bg-blue-700 transition-colors"
            >
              Create Test Suite
            </button>
          </div>
        ) : (
          testSuites.map((suite) => (
            <div key={suite.id} className="bg-white rounded-lg shadow-sm border border-gray-200">
              <div className="p-6">
                <div className="flex justify-between items-start mb-4">
                  <div className="flex-1">
                    <h4 className="text-lg font-semibold text-gray-900">{suite.name}</h4>
                    <p className="text-gray-600 mt-1">{suite.description}</p>
                    <div className="flex gap-4 mt-2 text-sm text-gray-500">
                      <span>{suite.tools.length} test cases</span>
                      <span>Created: {new Date(suite.created_at).toLocaleDateString()}</span>
                      <span>Updated: {new Date(suite.updated_at).toLocaleDateString()}</span>
                    </div>
                  </div>
                  <div className="flex gap-2">
                    <button
                      onClick={() => runTestSuite(suite)}
                      className="flex items-center gap-2 bg-green-600 text-white px-3 py-2 rounded-lg hover:bg-green-700 transition-colors"
                      title="Run Test Suite"
                    >
                      <Play className="w-4 h-4" />
                      Run
                    </button>
                    <button
                      onClick={() => addTestCase(suite.id)}
                      className="flex items-center gap-2 bg-blue-600 text-white px-3 py-2 rounded-lg hover:bg-blue-700 transition-colors"
                      title="Add Test Case"
                    >
                      <Plus className="w-4 h-4" />
                      Add Case
                    </button>
                    <button
                      onClick={() => startEdit(suite)}
                      className="flex items-center gap-2 bg-yellow-600 text-white px-3 py-2 rounded-lg hover:bg-yellow-700 transition-colors"
                      title="Edit Test Suite"
                    >
                      <Edit className="w-4 h-4" />
                      Edit
                    </button>
                    <button
                      onClick={() => deleteTestSuite(suite.id)}
                      className="flex items-center gap-2 bg-red-600 text-white px-3 py-2 rounded-lg hover:bg-red-700 transition-colors"
                      title="Delete Test Suite"
                    >
                      <Trash2 className="w-4 h-4" />
                      Delete
                    </button>
                  </div>
                </div>

                {/* Test Cases Preview */}
                <div className="border-t border-gray-200 pt-4">
                  <h5 className="text-sm font-medium text-gray-900 mb-3">Test Cases</h5>
                  {suite.tools.length === 0 ? (
                    <p className="text-gray-500 text-sm italic">No test cases added yet</p>
                  ) : (
                    <div className="space-y-2">
                      {suite.tools.slice(0, 3).map((testCase) => (
                        <div key={testCase.id} className="flex justify-between items-center p-2 bg-gray-50 rounded">
                          <div>
                            <span className="font-medium text-gray-900">{testCase.tool_name}</span>
                            <span className="text-gray-600 ml-2">→ {testCase.operation}</span>
                          </div>
                          <span className="text-xs text-gray-500">ID: {testCase.id.slice(-8)}</span>
                        </div>
                      ))}
                      {suite.tools.length > 3 && (
                        <p className="text-sm text-gray-600">
                          ... and {suite.tools.length - 3} more test cases
                        </p>
                      )}
                    </div>
                  )}
                </div>
              </div>
            </div>
          ))
        )}
      </div>
    </div>
  );
};
