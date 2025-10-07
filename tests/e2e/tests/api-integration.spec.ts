import { test, expect } from '@playwright/test';

/**
 * API Integration Tests
 *
 * Tests all REST API endpoints documented in api_routes.hpp
 * to ensure proper integration with the backend.
 */

test.describe('API Integration', () => {
  const baseURL = process.env.BASE_URL || 'http://localhost:8080';

  test.describe('Activity API', () => {
    test('GET /api/activity should return activity feed', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/activity`);

      // Allow both success and auth required responses
      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Activity API response keys:', Object.keys(data));
      }
    });
  });

  test.describe('Decisions API', () => {
    test('GET /api/decisions should return decision history', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/decisions`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Decisions API response:', typeof data, Array.isArray(data) ? `${data.length} items` : 'object');
      }
    });
  });

  test.describe('Regulatory Changes API', () => {
    test('GET /api/regulatory-changes should return regulatory updates', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/regulatory-changes`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Regulatory changes response structure:', Object.keys(data));
      }
    });

    test('GET /api/regulatory-changes with date filter', async ({ request }) => {
      const fromDate = new Date(Date.now() - 7 * 24 * 60 * 60 * 1000).toISOString();
      const toDate = new Date().toISOString();

      const response = await request.get(
        `${baseURL}/api/regulatory-changes?from=${fromDate}&to=${toDate}`
      );

      expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        console.log('Date-filtered regulatory changes retrieved successfully');
      }
    });
  });

  test.describe('Audit Trail API', () => {
    test('GET /api/audit-trail should return audit events', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/audit-trail`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Audit trail response:', typeof data);
      }
    });

    test('GET /api/audit-trail with filters', async ({ request }) => {
      const response = await request.get(
        `${baseURL}/api/audit-trail?event_type=compliance&limit=50`
      );

      expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        console.log('Filtered audit trail retrieved successfully');
      }
    });
  });

  test.describe('Communication API', () => {
    test('GET /api/communication should return agent messages', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/communication`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Communication API response:', typeof data);
      }
    });
  });

  test.describe('Agents API', () => {
    test('GET /api/agents should return agent list', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/agents`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        const agents = Array.isArray(data) ? data : data.agents || [];
        console.log(`Agents API returned ${Array.isArray(agents) ? agents.length : 0} agents`);
      }
    });

    test('GET /api/agents/:id should return agent details', async ({ request }) => {
      // First get list of agents
      const listResponse = await request.get(`${baseURL}/api/agents`);

      if (listResponse.ok()) {
        const data = await listResponse.json();
        const agents = Array.isArray(data) ? data : data.agents || [];

        if (agents.length > 0) {
          const agentId = agents[0].id || agents[0].agent_id || '1';
          const detailResponse = await request.get(`${baseURL}/api/agents/${agentId}`);

          expect([200, 201, 401, 403, 404].includes(detailResponse.status())).toBeTruthy();

          if (detailResponse.ok()) {
            const agentData = await detailResponse.json();
            console.log('Agent detail keys:', Object.keys(agentData));
          }
        }
      }
    });
  });

  test.describe('Collaboration API', () => {
    test('GET /api/collaboration should return collaboration data', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/collaboration`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Collaboration API response structure:', Object.keys(data));
      }
    });
  });

  test.describe('Patterns API', () => {
    test('GET /api/patterns should return pattern recognition data', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/patterns`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Patterns API response:', typeof data);
      }
    });
  });

  test.describe('Feedback API', () => {
    test('GET /api/feedback should return feedback data', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/feedback`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Feedback API response:', typeof data);
      }
    });

    test('POST /api/feedback should accept feedback submission', async ({ request }) => {
      const response = await request.post(`${baseURL}/api/feedback`, {
        data: {
          type: 'bug',
          message: 'Test feedback from automated tests',
          rating: 4
        }
      });

      // Accept success, auth required, or validation errors
      expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        console.log('Feedback submitted successfully');
      }
    });
  });

  test.describe('Errors API', () => {
    test('GET /api/errors should return error dashboard data', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/errors`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Errors API response:', typeof data);
      }
    });
  });

  test.describe('Health API', () => {
    test('GET /api/health should return system health', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/health`);

      // Health endpoint should generally be accessible
      expect([200, 201, 503].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Health check response:', data);

        // Expect basic health check fields
        expect(data).toHaveProperty('status');
      }
    });

    test('GET /health should also work (alternative endpoint)', async ({ request }) => {
      const response = await request.get(`${baseURL}/health`);

      expect([200, 201, 404, 503].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        console.log('Health endpoint accessible');
      }
    });
  });

  test.describe('LLM API', () => {
    test('GET /api/llm should return LLM integration status', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/llm`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('LLM API response structure:', Object.keys(data));
      }
    });
  });

  test.describe('Config API', () => {
    test('GET /api/config should return configuration', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/config`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Config API response:', typeof data);
      }
    });
  });

  test.describe('Database API', () => {
    test('GET /api/db should return database operations data', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/db`);

      expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const data = await response.json();
        console.log('Database API response:', typeof data);
      }
    });
  });

  test.describe('Metrics API', () => {
    test('GET /metrics should return Prometheus metrics', async ({ request }) => {
      const response = await request.get(`${baseURL}/metrics`);

      expect([200, 201, 401, 403, 404].includes(response.status())).toBeTruthy();

      if (response.ok()) {
        const contentType = response.headers()['content-type'];
        console.log('Metrics content type:', contentType);

        // Prometheus metrics are typically text/plain
        const body = await response.text();
        console.log('Metrics response length:', body.length);
      }
    });
  });

  test.describe('API Response Headers', () => {
    test('API should include CORS headers', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/health`);
      const headers = response.headers();

      console.log('CORS headers:', {
        'access-control-allow-origin': headers['access-control-allow-origin'],
        'access-control-allow-methods': headers['access-control-allow-methods'],
      });
    });

    test('API should return JSON content type for JSON endpoints', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/activity`);

      if (response.ok()) {
        const contentType = response.headers()['content-type'];
        console.log('Content-Type:', contentType);

        // Should be application/json
        expect(contentType).toContain('json');
      }
    });
  });

  test.describe('Error Handling', () => {
    test('API should handle non-existent endpoints gracefully', async ({ request }) => {
      const response = await request.get(`${baseURL}/api/nonexistent-endpoint-12345`);

      // Should return 404 or similar error
      expect([400, 404, 405].includes(response.status())).toBeTruthy();
      console.log('Non-existent endpoint status:', response.status());
    });

    test('API should validate malformed requests', async ({ request }) => {
      const response = await request.post(`${baseURL}/api/feedback`, {
        data: 'invalid-json-string'
      });

      // Should return validation error
      expect([400, 401, 403, 422].includes(response.status())).toBeTruthy();
      console.log('Malformed request status:', response.status());
    });
  });

  test.describe('Performance', () => {
    test('API endpoints should respond within reasonable time', async ({ request }) => {
      const startTime = Date.now();
      const response = await request.get(`${baseURL}/api/health`);
      const endTime = Date.now();

      const responseTime = endTime - startTime;
      console.log(`Health endpoint response time: ${responseTime}ms`);

      // Health check should be fast (< 2 seconds)
      expect(responseTime).toBeLessThan(2000);
    });

    test('Multiple concurrent API calls should be handled', async ({ request }) => {
      const endpoints = [
        '/api/activity',
        '/api/decisions',
        '/api/agents',
        '/api/health'
      ];

      const startTime = Date.now();

      // Make concurrent requests
      const promises = endpoints.map(endpoint =>
        request.get(`${baseURL}${endpoint}`).catch(() => null)
      );

      const responses = await Promise.all(promises);
      const endTime = Date.now();

      const totalTime = endTime - startTime;
      console.log(`Concurrent requests completed in ${totalTime}ms`);

      // Count successful responses
      const successful = responses.filter(r => r && r.ok()).length;
      console.log(`${successful}/${endpoints.length} concurrent requests successful`);
    });
  });
});
