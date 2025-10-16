/**
 * Impact Analysis Page
 * Standalone page for advanced regulatory impact analysis
 * Production-grade analytical tools and forecasting interface
 */

import React from 'react';
import ImpactAnalysis from '@/components/Simulator/ImpactAnalysis';

const ImpactAnalysisPage: React.FC = () => {
  return (
    <div className="min-h-screen bg-gray-50">
      <div className="py-6 px-4 sm:px-6 lg:px-8">
        <ImpactAnalysis
          scenarioId="scenario-001" // In production, this would come from URL params or context
          analysisType="baseline"
          onAnalysisComplete={(analysis) => {
            console.log('Analysis completed:', analysis);
            // Handle analysis completion
          }}
          onExport={(format) => {
            console.log('Exporting analysis as:', format);
            // Handle analysis export
          }}
          onSaveAnalysis={(analysis) => {
            console.log('Saving analysis:', analysis);
            // Handle analysis saving
          }}
        />
      </div>
    </div>
  );
};

export default ImpactAnalysisPage;
