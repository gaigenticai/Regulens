/**
 * Simulation Execution Page
 * Standalone page for real-time simulation execution monitoring
 * Production-grade execution interface
 */

import React from 'react';
import SimulationExecution from '@/components/Simulator/SimulationExecution';

const SimulationExecutionPage: React.FC = () => {
  return (
    <div className="min-h-screen bg-gray-50">
      <div className="py-6 px-4 sm:px-6 lg:px-8">
        <SimulationExecution
          onExecutionStart={(executionId) => {
            console.log('Execution started:', executionId);
            // Handle execution start notifications
          }}
          onExecutionComplete={(execution) => {
            console.log('Execution completed:', execution);
            // Handle execution completion
          }}
          onExecutionCancel={(executionId) => {
            console.log('Execution cancelled:', executionId);
            // Handle execution cancellation
          }}
          onViewResults={(executionId) => {
            console.log('View results for execution:', executionId);
            // Navigate to results page or open modal
          }}
          onExportResults={(executionId, format) => {
            console.log('Export results for execution:', executionId, 'as', format);
            // Handle results export
          }}
        />
      </div>
    </div>
  );
};

export default SimulationExecutionPage;
