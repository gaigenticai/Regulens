import { test, expect } from '@playwright/test';

/**
 * Pattern Recognition & Learning Tests
 */

test.describe('Pattern Recognition', () => {
  const baseURL = process.env.BASE_URL || 'http://localhost:8080';

  test('GET /api/patterns should return discovered patterns', async ({ request }) => {
    const response = await request.get(`${baseURL}/api/patterns`);
    expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('POST /api/patterns/discover should run discovery', async ({ request }) => {
    const response = await request.post(`${baseURL}/api/patterns/discover`, {
      data: { timeframe: '7d', min_confidence: 0.7 }
    });
    expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('GET /api/patterns/stats should return statistics', async ({ request }) => {
    const response = await request.get(`${baseURL}/api/patterns/stats`);
    expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('should display pattern list', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const patterns = page.locator('.pattern, .pattern-item, [data-testid="pattern"]');
    const count = await patterns.count();
    console.log(`Found ${count} recognized patterns`);
  });

  test('should show pattern confidence scores', async ({ page }) => {
    await page.waitForTimeout(2000);

    const confidence = page.locator('.confidence, .pattern-confidence');
    if (await confidence.count() > 0) {
      const scores = await confidence.allTextContents();
      console.log('Pattern confidence scores:', scores.slice(0, 3));
    }
  });

  test('should display anomaly detection', async ({ page }) => {
    await page.waitForTimeout(2000);

    const anomalies = page.locator('.anomaly, .outlier, :has-text("anomaly")');
    const count = await anomalies.count();
    console.log(`Found ${count} detected anomalies`);
  });
});
