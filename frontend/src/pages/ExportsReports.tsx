import React, { useState } from 'react';
import { Download, FileText, Plus } from 'lucide-react';
import { useExportRequests, useCreateExportRequest, useExportTemplates } from '@/hooks/useExports';
import { LoadingSpinner } from '@/components/LoadingSpinner';

const ExportsReports: React.FC = () => {
  const [showCreateModal, setShowCreateModal] = useState(false);
  const { data: requests = [], isLoading } = useExportRequests();
  const { data: templates = [] } = useExportTemplates();
  const createExport = useCreateExportRequest();

  const handleCreateExport = async (e: React.FormEvent<HTMLFormElement>) => {
    e.preventDefault();
    const formData = new FormData(e.currentTarget);
    
    await createExport.mutateAsync({
      export_type: formData.get('export_type') as string,
      requested_by: 'current_user',
    });
    
    setShowCreateModal(false);
    e.currentTarget.reset();
  };

  if (isLoading) {
    return <LoadingSpinner fullScreen message="Loading exports..." />;
  }

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'completed': return 'text-green-600 bg-green-100';
      case 'pending': return 'text-yellow-600 bg-yellow-100';
      case 'processing': return 'text-blue-600 bg-blue-100';
      case 'failed': return 'text-red-600 bg-red-100';
      default: return 'text-gray-600 bg-gray-100';
    }
  };

  return (
    <div className="space-y-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-2xl font-bold text-gray-900">Exports & Reports</h1>
          <p className="text-gray-600 mt-1">Generate PDF and Excel reports</p>
        </div>
        <button
          onClick={() => setShowCreateModal(true)}
          className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
        >
          <Plus className="w-5 h-5" />
          New Export
        </button>
      </div>

      <div className="bg-white rounded-lg shadow-sm border border-gray-200">
        <div className="p-6 border-b border-gray-200">
          <h2 className="text-lg font-semibold text-gray-900">Export History</h2>
        </div>
        <div className="divide-y divide-gray-200">
          {requests.length === 0 ? (
            <div className="p-12 text-center">
              <FileText className="w-12 h-12 text-gray-400 mx-auto mb-4" />
              <h3 className="text-lg font-medium text-gray-900 mb-2">No exports yet</h3>
              <p className="text-gray-600 mb-4">Create your first export to generate reports</p>
            </div>
          ) : (
            requests.map((req) => (
              <div key={req.export_id} className="p-6 hover:bg-gray-50">
                <div className="flex items-center justify-between">
                  <div className="flex-1">
                    <div className="flex items-center gap-3 mb-2">
                      <h3 className="text-lg font-semibold text-gray-900">{req.export_type}</h3>
                      <span className={`px-2 py-1 text-xs font-medium rounded-full ${getStatusColor(req.status)}`}>
                        {req.status}
                      </span>
                    </div>
                    <p className="text-sm text-gray-500">
                      Created {new Date(req.created_at).toLocaleString()}
                      {req.file_size_bytes && ` • ${(req.file_size_bytes / 1024 / 1024).toFixed(2)} MB`}
                      {req.download_count > 0 && ` • Downloaded ${req.download_count} times`}
                    </p>
                  </div>
                  {req.status === 'completed' && req.download_url && (
                    <a
                      href={req.download_url}
                      className="flex items-center gap-2 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700"
                    >
                      <Download className="w-4 h-4" />
                      Download
                    </a>
                  )}
                </div>
              </div>
            ))
          )}
        </div>
      </div>

      {showCreateModal && (
        <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50">
          <div className="bg-white rounded-lg shadow-xl max-w-md w-full mx-4">
            <div className="p-6 border-b border-gray-200">
              <h2 className="text-xl font-semibold text-gray-900">Create Export</h2>
            </div>
            <form onSubmit={handleCreateExport} className="p-6 space-y-4">
              <div>
                <label className="block text-sm font-medium text-gray-700 mb-1">Export Type *</label>
                <select
                  name="export_type"
                  required
                  className="w-full px-3 py-2 border border-gray-300 rounded-lg focus:ring-2 focus:ring-blue-500"
                >
                  <option value="">Select type...</option>
                  <option value="pdf_audit_trail">PDF Audit Trail</option>
                  <option value="excel_transactions">Excel Transactions</option>
                  <option value="pdf_compliance_report">PDF Compliance Report</option>
                  <option value="excel_regulatory_changes">Excel Regulatory Changes</option>
                  <option value="csv_data_export">CSV Data Export</option>
                </select>
              </div>
              <div className="flex gap-3 pt-4">
                <button
                  type="button"
                  onClick={() => setShowCreateModal(false)}
                  className="flex-1 px-4 py-2 border border-gray-300 rounded-lg hover:bg-gray-50"
                >
                  Cancel
                </button>
                <button
                  type="submit"
                  disabled={createExport.isPending}
                  className="flex-1 px-4 py-2 bg-blue-600 text-white rounded-lg hover:bg-blue-700 disabled:opacity-50"
                >
                  {createExport.isPending ? 'Creating...' : 'Create Export'}
                </button>
              </div>
            </form>
          </div>
        </div>
      )}
    </div>
  );
};

export default ExportsReports;

