/**
 * RBAC Management - Phase 7B.1
 * Role hierarchy, user assignments, permission matrix
 */

import React, { useState, useEffect } from 'react';
import { securityAPI } from '../services/api';

interface Role {
  role_id: string;
  role_name: string;
  description: string;
  hierarchy_level: number;
  is_builtin: boolean;
}

interface Permission {
  resource_type: string;
  action: string;
  level: 'READ' | 'WRITE' | 'APPROVE' | 'ADMIN';
}

interface UserRole {
  assignment_id: string;
  user_id: string;
  role_id: string;
  assigned_at: string;
  expires_at?: string;
}

export const RBACManagement: React.FC = () => {
  const [activeTab, setActiveTab] = useState<'roles' | 'assignments' | 'permissions'>('roles');
  const [roles, setRoles] = useState<Role[]>([]);
  const [userRoles, setUserRoles] = useState<UserRole[]>([]);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const [showCreateRole, setShowCreateRole] = useState(false);
  const [showAssignUser, setShowAssignUser] = useState(false);
  const [newRole, setNewRole] = useState({ name: '', description: '', level: 1 });
  const [selectedUserId, setSelectedUserId] = useState('');
  const [selectedRoleId, setSelectedRoleId] = useState('');

  useEffect(() => {
    loadRBACData();
  }, [activeTab]);

  const loadRBACData = async () => {
    setLoading(true);
    setError(null);
    try {
      const [rolesList, statsData] = await Promise.all([
        securityAPI.getAllRoles(),
        securityAPI.getRBACStats(),
      ]);
      setRoles(rolesList || []);
    } catch (err) {
      setError('Failed to load RBAC data');
      console.error(err);
    } finally {
      setLoading(false);
    }
  };

  const handleCreateRole = async () => {
    if (!newRole.name.trim()) return;
    try {
      await securityAPI.createRole({
        role_name: newRole.name,
        description: newRole.description,
        hierarchy_level: newRole.level,
      });
      setNewRole({ name: '', description: '', level: 1 });
      setShowCreateRole(false);
      loadRBACData();
    } catch (err) {
      setError('Failed to create role');
    }
  };

  const handleDeleteRole = async (roleId: string) => {
    if (!confirm('Delete this role?')) return;
    try {
      await securityAPI.deleteRole(roleId);
      loadRBACData();
    } catch (err) {
      setError('Failed to delete role');
    }
  };

  const handleAssignUserRole = async () => {
    if (!selectedUserId || !selectedRoleId) return;
    try {
      await securityAPI.assignUserRole(selectedUserId, selectedRoleId, {
        assigned_at: new Date().toISOString(),
      });
      setSelectedUserId('');
      setSelectedRoleId('');
      setShowAssignUser(false);
      loadRBACData();
    } catch (err) {
      setError('Failed to assign role');
    }
  };

  const handleRevokeUserRole = async (userId: string, roleId: string) => {
    if (!confirm('Revoke this role?')) return;
    try {
      await securityAPI.revokeUserRole(userId, roleId);
      loadRBACData();
    } catch (err) {
      setError('Failed to revoke role');
    }
  };

  const renderRolesTab = () => (
    <div className="space-y-6">
      <div className="flex justify-between items-center">
        <h3 className="text-lg font-semibold">Role Hierarchy</h3>
        <button
          onClick={() => setShowCreateRole(!showCreateRole)}
          className="bg-blue-600 text-white px-4 py-2 rounded hover:bg-blue-700"
        >
          + Create Role
        </button>
      </div>

      {showCreateRole && (
        <div className="bg-blue-50 p-4 rounded-lg border border-blue-200">
          <div className="space-y-3">
            <input
              type="text"
              placeholder="Role name"
              value={newRole.name}
              onChange={(e) => setNewRole({ ...newRole, name: e.target.value })}
              className="w-full px-3 py-2 border rounded"
            />
            <textarea
              placeholder="Description"
              value={newRole.description}
              onChange={(e) => setNewRole({ ...newRole, description: e.target.value })}
              className="w-full px-3 py-2 border rounded"
              rows={2}
            />
            <select
              value={newRole.level}
              onChange={(e) => setNewRole({ ...newRole, level: parseInt(e.target.value) })}
              className="w-full px-3 py-2 border rounded"
            >
              {[1, 2, 3, 4, 5, 6, 7, 8, 9, 10].map((level) => (
                <option key={level} value={level}>
                  Hierarchy Level {level}
                </option>
              ))}
            </select>
            <div className="flex gap-2">
              <button
                onClick={handleCreateRole}
                className="bg-green-600 text-white px-4 py-2 rounded hover:bg-green-700"
              >
                Create
              </button>
              <button
                onClick={() => setShowCreateRole(false)}
                className="bg-gray-400 text-white px-4 py-2 rounded hover:bg-gray-500"
              >
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}

      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {roles.map((role) => (
          <div key={role.role_id} className="bg-white p-4 rounded-lg border border-gray-200">
            <div className="flex justify-between items-start mb-2">
              <div>
                <h4 className="font-semibold text-gray-900">{role.role_name}</h4>
                <p className="text-sm text-gray-600 mt-1">{role.description}</p>
              </div>
              {!role.is_builtin && (
                <button
                  onClick={() => handleDeleteRole(role.role_id)}
                  className="text-red-600 hover:text-red-800"
                >
                  ✕
                </button>
              )}
            </div>
            <div className="flex gap-2 mt-3">
              <span className="inline-block px-2 py-1 text-xs bg-blue-100 text-blue-800 rounded">
                Level {role.hierarchy_level}
              </span>
              {role.is_builtin && (
                <span className="inline-block px-2 py-1 text-xs bg-gray-100 text-gray-800 rounded">
                  Built-in
                </span>
              )}
            </div>
          </div>
        ))}
      </div>
    </div>
  );

  const renderAssignmentsTab = () => (
    <div className="space-y-6">
      <div className="flex justify-between items-center">
        <h3 className="text-lg font-semibold">User Role Assignments</h3>
        <button
          onClick={() => setShowAssignUser(!showAssignUser)}
          className="bg-blue-600 text-white px-4 py-2 rounded hover:bg-blue-700"
        >
          + Assign Role
        </button>
      </div>

      {showAssignUser && (
        <div className="bg-blue-50 p-4 rounded-lg border border-blue-200">
          <div className="space-y-3">
            <input
              type="text"
              placeholder="User ID"
              value={selectedUserId}
              onChange={(e) => setSelectedUserId(e.target.value)}
              className="w-full px-3 py-2 border rounded"
            />
            <select
              value={selectedRoleId}
              onChange={(e) => setSelectedRoleId(e.target.value)}
              className="w-full px-3 py-2 border rounded"
            >
              <option value="">Select Role</option>
              {roles.map((role) => (
                <option key={role.role_id} value={role.role_id}>
                  {role.role_name}
                </option>
              ))}
            </select>
            <div className="flex gap-2">
              <button
                onClick={handleAssignUserRole}
                className="bg-green-600 text-white px-4 py-2 rounded hover:bg-green-700"
              >
                Assign
              </button>
              <button
                onClick={() => setShowAssignUser(false)}
                className="bg-gray-400 text-white px-4 py-2 rounded hover:bg-gray-500"
              >
                Cancel
              </button>
            </div>
          </div>
        </div>
      )}

      <div className="bg-white rounded-lg border border-gray-200 overflow-hidden">
        <table className="w-full">
          <thead className="bg-gray-50">
            <tr>
              <th className="px-4 py-3 text-left text-sm font-semibold">User ID</th>
              <th className="px-4 py-3 text-left text-sm font-semibold">Role</th>
              <th className="px-4 py-3 text-left text-sm font-semibold">Assigned</th>
              <th className="px-4 py-3 text-left text-sm font-semibold">Actions</th>
            </tr>
          </thead>
          <tbody className="divide-y">
            {userRoles.map((assignment) => (
              <tr key={assignment.assignment_id} className="hover:bg-gray-50">
                <td className="px-4 py-3 text-sm">{assignment.user_id}</td>
                <td className="px-4 py-3 text-sm">{assignment.role_id}</td>
                <td className="px-4 py-3 text-sm">{new Date(assignment.assigned_at).toLocaleDateString()}</td>
                <td className="px-4 py-3 text-sm">
                  <button
                    onClick={() => handleRevokeUserRole(assignment.user_id, assignment.role_id)}
                    className="text-red-600 hover:text-red-800"
                  >
                    Revoke
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );

  const renderPermissionsTab = () => (
    <div className="space-y-6">
      <h3 className="text-lg font-semibold">Permission Matrix</h3>
      <div className="bg-white rounded-lg border border-gray-200 overflow-x-auto">
        <table className="w-full text-sm">
          <thead className="bg-gray-50">
            <tr>
              <th className="px-4 py-3 text-left font-semibold">Role</th>
              <th className="px-4 py-3 text-center font-semibold">READ</th>
              <th className="px-4 py-3 text-center font-semibold">WRITE</th>
              <th className="px-4 py-3 text-center font-semibold">APPROVE</th>
              <th className="px-4 py-3 text-center font-semibold">ADMIN</th>
            </tr>
          </thead>
          <tbody className="divide-y">
            {roles.map((role) => (
              <tr key={role.role_id} className="hover:bg-gray-50">
                <td className="px-4 py-3 font-medium">{role.role_name}</td>
                <td className="px-4 py-3 text-center">
                  <input type="checkbox" defaultChecked className="w-4 h-4" />
                </td>
                <td className="px-4 py-3 text-center">
                  <input type="checkbox" defaultChecked={role.hierarchy_level > 3} className="w-4 h-4" />
                </td>
                <td className="px-4 py-3 text-center">
                  <input type="checkbox" defaultChecked={role.hierarchy_level > 6} className="w-4 h-4" />
                </td>
                <td className="px-4 py-3 text-center">
                  <input type="checkbox" defaultChecked={role.hierarchy_level === 10} className="w-4 h-4" />
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </div>
  );

  return (
    <div className="min-h-screen bg-gray-100 p-6">
      <div className="max-w-6xl mx-auto">
        {/* Header */}
        <div className="mb-8">
          <h1 className="text-3xl font-bold text-gray-900">RBAC Management</h1>
          <p className="text-gray-600 mt-2">Role hierarchy, permissions, and user assignments</p>
        </div>

        {/* Tabs */}
        <div className="bg-white rounded-lg shadow mb-6 border-b">
          <div className="flex">
            {[
              { id: 'roles', label: 'Roles', icon: '👥' },
              { id: 'assignments', label: 'Assignments', icon: '🔗' },
              { id: 'permissions', label: 'Permissions', icon: '🔐' },
            ].map((tab) => (
              <button
                key={tab.id}
                onClick={() => setActiveTab(tab.id as any)}
                className={`flex-1 px-6 py-4 font-medium text-center transition-colors ${
                  activeTab === tab.id
                    ? 'border-b-2 border-blue-600 text-blue-600'
                    : 'text-gray-600 hover:text-gray-900'
                }`}
              >
                <span className="mr-2">{tab.icon}</span>
                {tab.label}
              </button>
            ))}
          </div>
        </div>

        {/* Content */}
        <div className="bg-white rounded-lg shadow p-6">
          {loading ? (
            <div className="flex justify-center items-center h-96">
              <div className="text-center">
                <div className="w-12 h-12 bg-blue-200 rounded-full animate-spin mx-auto mb-4"></div>
                <p className="text-gray-600">Loading...</p>
              </div>
            </div>
          ) : error ? (
            <div className="text-red-600 p-4 bg-red-50 rounded">{error}</div>
          ) : (
            <>
              {activeTab === 'roles' && renderRolesTab()}
              {activeTab === 'assignments' && renderAssignmentsTab()}
              {activeTab === 'permissions' && renderPermissionsTab()}
            </>
          )}
        </div>
      </div>
    </div>
  );
};

export default RBACManagement;
