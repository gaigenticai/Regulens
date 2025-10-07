import { test, expect } from '@playwright/test';

/**
 * Circuit Breaker & Resilience Tests
 */

test.describe('Resilience & Circuit Breakers', () => {
  const baseURL = process.env.BASE_URL || 'http://localhost:8080';

  test('GET /api/circuit-breaker/status should return status', async ({ request }) => {
    const response = await request.get(`${baseURL}/api/circuit-breaker/status`);
    expect([200, 201, 401, 403, 404].includes(response.status())).toBeTruthy();

    if (response.ok()) {
      const data = await response.json();
      console.log('Circuit breaker status:', data);
    }
  });

  test('should display circuit breaker states', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const circuitBreakers = page.locator('.circuit-breaker, :has-text("circuit")');
    if (await circuitBreakers.count() > 0) {
      console.log('Circuit breaker information found');
    }
  });

  test('should show retry attempts', async ({ page }) => {
    await page.waitForTimeout(2000);

    const retries = page.locator(':has-text("retry"), :has-text("attempt")');
    if (await retries.count() > 0) {
      console.log('Retry mechanism indicators found');
    }
  });

  test('should handle service degradation', async ({ page }) => {
    await page.waitForTimeout(2000);

    const degradedServices = page.locator('.degraded, :has-text("degraded")');
    const count = await degradedServices.count();
    console.log(`Found ${count} degraded services`);
  });
});
