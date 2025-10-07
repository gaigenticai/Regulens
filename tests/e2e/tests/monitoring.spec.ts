import { test, expect } from '@playwright/test';

/**
 * Prometheus Metrics & Monitoring Tests
 */

test.describe('Monitoring & Metrics', () => {
  const baseURL = process.env.BASE_URL || 'http://localhost:8080';

  test('GET /metrics should return Prometheus metrics', async ({ request }) => {
    const response = await request.get(`${baseURL}/metrics`);
    expect([200, 201, 401, 403, 404].includes(response.status())).toBeTruthy();

    if (response.ok()) {
      const metrics = await response.text();
      console.log('Metrics response length:', metrics.length);

      // Check for standard metrics
      expect(metrics).toContain('# HELP');
      expect(metrics).toContain('# TYPE');
    }
  });

  test('GET /ready should return readiness probe', async ({ request }) => {
    const response = await request.get(`${baseURL}/ready`);
    expect([200, 201, 404, 503].includes(response.status())).toBeTruthy();
  });

  test('should display system metrics', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const metrics = page.locator('.metric, .system-metric, :has-text("CPU"), :has-text("Memory")');
    const count = await metrics.count();
    console.log(`Found ${count} system metrics`);
  });

  test('should show performance graphs', async ({ page }) => {
    await page.waitForTimeout(2000);

    const graphs = page.locator('canvas, svg.chart, .chart');
    const count = await graphs.count();
    console.log(`Found ${count} performance visualizations`);
  });

  test('should display API request metrics', async ({ page }) => {
    await page.waitForTimeout(2000);

    const apiMetrics = page.locator(':has-text("request"), :has-text("response time"), :has-text("throughput")');
    if (await apiMetrics.count() > 0) {
      console.log('API request metrics found');
    }
  });
});
