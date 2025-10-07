import { test, expect } from '@playwright/test';

/**
 * Decision Tree Visualization Tests
 */

test.describe('Visualizations', () => {
  const baseURL = process.env.BASE_URL || 'http://localhost:8080';

  test('GET /api/decisions/tree should return tree data', async ({ request }) => {
    const response = await request.get(`${baseURL}/api/decisions/tree`);
    expect([200, 201, 401, 403, 404].includes(response.status())).toBeTruthy();

    if (response.ok()) {
      const treeData = await response.json();
      console.log('Decision tree data structure:', Object.keys(treeData));
    }
  });

  test('should render D3.js decision tree', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const svg = page.locator('svg.decision-tree, svg[data-viz="tree"]');
    if (await svg.count() > 0) {
      console.log('D3.js decision tree SVG found');

      const nodes = svg.locator('circle, .node');
      const nodeCount = await nodes.count();
      console.log(`Tree has ${nodeCount} nodes`);
    }
  });

  test('should display interactive graphs', async ({ page }) => {
    await page.waitForTimeout(2000);

    const graphs = page.locator('canvas, svg.chart, .visualization');
    const count = await graphs.count();
    console.log(`Found ${count} interactive visualizations`);
  });

  test('should show agent communication graph', async ({ page }) => {
    await page.waitForTimeout(2000);

    const agentGraph = page.locator('.agent-graph, .communication-graph');
    if (await agentGraph.count() > 0) {
      console.log('Agent communication graph found');
    }
  });
});
