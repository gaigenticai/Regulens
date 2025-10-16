/**
 * Simulation Results Page
 * Standalone page for viewing comprehensive simulation results
 * Production-grade results dashboard and analysis interface
 */

import React from 'react';
import SimulationResults from '@/components/Simulator/SimulationResults';

const SimulationResultsPage: React.FC = () => {
  return (
    <div className="min-h-screen bg-gray-50">
      <div className="py-6 px-4 sm:px-6 lg:px-8">
        <SimulationResults
          onExport={(format) => {
            console.log('Exporting results as:', format);
            // Handle results export
          }}
          onShare={(resultId) => {
            console.log('Sharing result:', resultId);
            // Handle result sharing
          }}
          onCompare={(resultIds) => {
            console.log('Comparing results:', resultIds);
            // Handle results comparison
          }}
          onViewRecommendations={(resultId) => {
            console.log('Viewing recommendations for result:', resultId);
            // Handle recommendations viewing
          }}
        />
      </div>
    </div>
  );
};

export default SimulationResultsPage;
