/**
 * API Version Selector Component
 * Allows users to select and switch between API versions
 */

import React, { useState, useEffect } from 'react';
import { Select, SelectContent, SelectItem, SelectTrigger, SelectValue } from './ui/select';
import { Badge } from './ui/badge';
import { Button } from './ui/button';
import { Alert, AlertDescription } from './ui/alert';
import { Card, CardContent, CardDescription, CardHeader, CardTitle } from './ui/card';
import { Dialog, DialogContent, DialogDescription, DialogHeader, DialogTitle, DialogTrigger } from './ui/dialog';
import {
  RefreshCw,
  AlertTriangle,
  CheckCircle,
  Info,
  Settings,
  Zap,
  Clock
} from 'lucide-react';
import { apiVersion } from '@/services/api';

interface APIVersion {
  version: string;
  status: 'current' | 'supported' | 'deprecated' | 'sunset';
  releaseDate: string;
  description: string;
  features: string[];
  breakingChanges?: string[];
  migrationGuide?: string;
  sunsetDate?: string;
}

const mockVersions: APIVersion[] = [
  {
    version: 'v1',
    status: 'current',
    releaseDate: '2024-01-15',
    description: 'Initial stable API release with core functionality',
    features: [
      'JWT Authentication',
      'Transaction Processing',
      'Fraud Detection Engine',
      'Advanced Rule Engine',
      'Policy Generation',
      'Regulatory Simulator'
    ]
  }
];

const VersionSelector: React.FC = () => {
  const [selectedVersion, setSelectedVersion] = useState(apiVersion.current);
  const [versions] = useState<APIVersion[]>(mockVersions);
  const [isChanging, setIsChanging] = useState(false);
  const [changeError, setChangeError] = useState<string | null>(null);
  const [showDetails, setShowDetails] = useState(false);

  const currentVersionData = versions.find(v => v.version === selectedVersion);

  const getStatusColor = (status: string) => {
    switch (status) {
      case 'current': return 'bg-green-100 text-green-800';
      case 'supported': return 'bg-blue-100 text-blue-800';
      case 'deprecated': return 'bg-yellow-100 text-yellow-800';
      case 'sunset': return 'bg-red-100 text-red-800';
      default: return 'bg-gray-100 text-gray-800';
    }
  };

  const getStatusIcon = (status: string) => {
    switch (status) {
      case 'current': return <CheckCircle className="h-3 w-3" />;
      case 'supported': return <Info className="h-3 w-3" />;
      case 'deprecated': return <AlertTriangle className="h-3 w-3" />;
      case 'sunset': return <AlertTriangle className="h-3 w-3" />;
      default: return <Clock className="h-3 w-3" />;
    }
  };

  const handleVersionChange = async (newVersion: string) => {
    if (newVersion === selectedVersion) return;

    setIsChanging(true);
    setChangeError(null);

    try {
      // In production, this would validate compatibility and test the version
      await new Promise(resolve => setTimeout(resolve, 1000)); // Simulate API check

      apiVersion.set(newVersion);
      setSelectedVersion(newVersion);

      // Show success message or notification
      console.log(`Successfully switched to API version ${newVersion}`);

    } catch (error) {
      setChangeError(`Failed to switch to version ${newVersion}. Please try again.`);
    } finally {
      setIsChanging(false);
    }
  };

  const getVersionDescription = (version: APIVersion) => {
    const statusText = {
      current: 'Latest stable version',
      supported: 'Still supported',
      deprecated: 'Deprecated - consider upgrading',
      sunset: 'Will be removed soon'
    };

    return `${statusText[version.status]} - Released ${new Date(version.releaseDate).toLocaleDateString()}`;
  };

  return (
    <div className="space-y-4">
      {/* Version Selector */}
      <div className="flex items-center gap-4">
        <div className="flex items-center gap-2">
          <Settings className="h-4 w-4 text-gray-600" />
          <span className="text-sm font-medium">API Version:</span>
        </div>

        <Select value={selectedVersion} onValueChange={handleVersionChange} disabled={isChanging}>
          <SelectTrigger className="w-32">
            <SelectValue />
          </SelectTrigger>
          <SelectContent>
            {apiVersion.versions.map((version) => {
              const versionData = versions.find(v => v.version === version);
              return (
                <SelectItem key={version} value={version}>
                  <div className="flex items-center gap-2">
                    {version}
                    {versionData && (
                      <Badge className={getStatusColor(versionData.status)}>
                        {versionData.status}
                      </Badge>
                    )}
                  </div>
                </SelectItem>
              );
            })}
          </SelectContent>
        </Select>

        {isChanging && <RefreshCw className="h-4 w-4 animate-spin text-blue-600" />}

        <Dialog open={showDetails} onOpenChange={setShowDetails}>
          <DialogTrigger asChild>
            <Button variant="outline" size="sm">
              <Info className="h-4 w-4 mr-2" />
              Details
            </Button>
          </DialogTrigger>
          <DialogContent className="max-w-2xl max-h-[80vh] overflow-y-auto">
            <DialogHeader>
              <DialogTitle>API Version Information</DialogTitle>
              <DialogDescription>
                Detailed information about available API versions
              </DialogDescription>
            </DialogHeader>

            <div className="space-y-4">
              {versions.map((version) => (
                <Card key={version.version}>
                  <CardHeader className="pb-3">
                    <div className="flex items-center justify-between">
                      <div className="flex items-center gap-3">
                        <CardTitle className="text-lg">{version.version}</CardTitle>
                        <Badge className={getStatusColor(version.status)}>
                          <div className="flex items-center gap-1">
                            {getStatusIcon(version.status)}
                            {version.status}
                          </div>
                        </Badge>
                        {version.version === selectedVersion && (
                          <Badge className="bg-blue-100 text-blue-800">
                            <Zap className="h-3 w-3 mr-1" />
                            Active
                          </Badge>
                        )}
                      </div>
                    </div>
                    <CardDescription>{getVersionDescription(version)}</CardDescription>
                  </CardHeader>

                  <CardContent className="space-y-4">
                    <div>
                      <h4 className="font-medium mb-2">Features</h4>
                      <ul className="list-disc list-inside text-sm text-gray-600 space-y-1">
                        {version.features.map((feature, index) => (
                          <li key={index}>{feature}</li>
                        ))}
                      </ul>
                    </div>

                    {version.breakingChanges && version.breakingChanges.length > 0 && (
                      <div>
                        <h4 className="font-medium mb-2 text-orange-700">Breaking Changes</h4>
                        <ul className="list-disc list-inside text-sm text-orange-600 space-y-1">
                          {version.breakingChanges.map((change, index) => (
                            <li key={index}>{change}</li>
                          ))}
                        </ul>
                      </div>
                    )}

                    {version.migrationGuide && (
                      <div>
                        <h4 className="font-medium mb-2">Migration Guide</h4>
                        <p className="text-sm text-gray-600">{version.migrationGuide}</p>
                      </div>
                    )}

                    {version.sunsetDate && (
                      <Alert>
                        <AlertTriangle className="h-4 w-4" />
                        <AlertDescription>
                          This version will be sunset on {new Date(version.sunsetDate).toLocaleDateString()}.
                          Please migrate to a newer version.
                        </AlertDescription>
                      </Alert>
                    )}

                    {version.version !== selectedVersion && (
                      <Button
                        variant="outline"
                        size="sm"
                        onClick={() => {
                          handleVersionChange(version.version);
                          setShowDetails(false);
                        }}
                        disabled={isChanging}
                      >
                        Switch to {version.version}
                      </Button>
                    )}
                  </CardContent>
                </Card>
              ))}
            </div>
          </DialogContent>
        </Dialog>
      </div>

      {/* Current Version Info */}
      {currentVersionData && (
        <div className="flex items-center gap-4 p-3 bg-gray-50 rounded-lg">
          <div className="flex items-center gap-2">
            {getStatusIcon(currentVersionData.status)}
            <span className="text-sm font-medium">
              Version {currentVersionData.version}
            </span>
            <Badge className={getStatusColor(currentVersionData.status)}>
              {currentVersionData.status}
            </Badge>
          </div>

          <div className="text-xs text-gray-600">
            {currentVersionData.features.length} features • Released {new Date(currentVersionData.releaseDate).toLocaleDateString()}
          </div>
        </div>
      )}

      {/* Error Display */}
      {changeError && (
        <Alert variant="destructive">
          <AlertTriangle className="h-4 w-4" />
          <AlertDescription>{changeError}</AlertDescription>
        </Alert>
      )}

      {/* Version Compatibility Warning */}
      {currentVersionData?.status === 'deprecated' && (
        <Alert>
          <AlertTriangle className="h-4 w-4" />
          <AlertDescription>
            You are using a deprecated API version. Consider upgrading to the latest version for new features and security updates.
          </AlertDescription>
        </Alert>
      )}
    </div>
  );
};

export default VersionSelector;
