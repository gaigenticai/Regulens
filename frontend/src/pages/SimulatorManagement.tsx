/**
 * Simulator Management Page
 * Advanced regulatory simulator management interface
 * Production-grade simulation lifecycle management
 */

import React from 'react';
import SimulatorManagement from '@/components/Simulator/SimulatorManagement';

const SimulatorManagementPage: React.FC = () => {
  return (
    <div className="min-h-screen bg-gray-50">
      <div className="max-w-7xl mx-auto py-6 px-4 sm:px-6 lg:px-8">
        <SimulatorManagement
          onScenarioCreate={(scenario) => {
            console.log('Create scenario:', scenario);
            // Handle scenario creation
          }}
          onScenarioUpdate={(scenarioId, updates) => {
            console.log('Update scenario:', scenarioId, updates);
            // Handle scenario updates
          }}
          onScenarioDelete={(scenarioId) => {
            console.log('Delete scenario:', scenarioId);
            // Handle scenario deletion
          }}
          onBulkOperation={(operation, scenarioIds) => {
            console.log('Bulk operation:', operation, 'on scenarios:', scenarioIds);
            // Handle bulk operations
          }}
          onWorkflowExecute={(workflowId) => {
            console.log('Execute workflow:', workflowId);
            // Handle workflow execution
          }}
          onScheduleCreate={(schedule) => {
            console.log('Create schedule:', schedule);
            // Handle schedule creation
          }}
        />
      </div>
    </div>
  );
};

export default SimulatorManagementPage;
