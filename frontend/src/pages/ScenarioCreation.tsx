/**
 * Scenario Creation Page
 * Standalone page for creating regulatory simulation scenarios
 * Production-grade scenario creation interface
 */

import React from 'react';
import ScenarioCreation from '@/components/Simulator/ScenarioCreation';

const ScenarioCreationPage: React.FC = () => {
  return (
    <div className="min-h-screen bg-gray-50">
      <div className="py-6 px-4 sm:px-6 lg:px-8">
        <ScenarioCreation
          onSave={async (scenario) => {
            console.log('Saving scenario:', scenario);
            // Handle scenario saving
            // In production, this would make an API call to save the scenario
            alert('Scenario saved successfully!');
          }}
          onTest={async (scenario) => {
            console.log('Testing scenario:', scenario);
            // Handle scenario testing
            // In production, this would make an API call to test the scenario
            await new Promise(resolve => setTimeout(resolve, 2000)); // Simulate test delay
            return { success: true, message: 'Test completed successfully' };
          }}
          onPreview={(scenario) => {
            console.log('Previewing scenario:', scenario);
            // Handle scenario preview
            // Could open a modal or navigate to preview page
          }}
          onCancel={() => {
            console.log('Cancelling scenario creation');
            // Handle cancellation - could navigate back or show confirmation
            if (window.confirm('Are you sure you want to cancel? All unsaved changes will be lost.')) {
              window.history.back();
            }
          }}
        />
      </div>
    </div>
  );
};

export default ScenarioCreationPage;
