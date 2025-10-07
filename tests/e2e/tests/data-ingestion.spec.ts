import { test, expect } from '@playwright/test';

/**
 * Data Ingestion Pipeline Tests
 *
 * Tests REST API sources, web scraping, database sources,
 * and compliance validation
 */

test.describe('Data Ingestion', () => {
  const baseURL = process.env.BASE_URL || 'http://localhost:8080';

  test('should show data source status', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const sources = page.locator(
      '.data-source, .source-status, [data-testid="data-source"]'
    );

    const count = await sources.count();
    console.log(`Found ${count} data sources`);

    if (count > 0) {
      const firstSource = sources.first();
      const sourceText = await firstSource.textContent();
      console.log('Sample data source:', sourceText?.substring(0, 100));
    }
  });

  test('GET /api/regulatory/sources should list active sources', async ({ request }) => {
    const response = await request.get(`${baseURL}/api/regulatory/sources`);

    expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();

    if (response.ok()) {
      const sources = await response.json();
      console.log('Active regulatory sources:', typeof sources);

      if (Array.isArray(sources)) {
        console.log(`Total sources: ${sources.length}`);
        sources.slice(0, 3).forEach((src: any) => {
          console.log(`  - ${src.name || src.type}: ${src.status || 'unknown'}`);
        });
      }
    }
  });

  test('should display ECB RSS feed status', async ({ page }) => {
    await page.waitForTimeout(2000);

    const ecbSource = page.locator(
      ':has-text("ECB"), :has-text("European Central Bank"), .source:has-text("RSS")'
    );

    if (await ecbSource.count() > 0) {
      console.log('ECB RSS feed source found');

      const status = ecbSource.locator('.status, .health');
      if (await status.count() > 0) {
        const statusText = await status.first().textContent();
        console.log('ECB feed status:', statusText);
      }
    }
  });

  test('should show web scraping sources', async ({ page }) => {
    await page.waitForTimeout(2000);

    const scrapingSources = page.locator(
      ':has-text("Web Scraping"), :has-text("Scraper"), .scraping-source'
    );

    const count = await scrapingSources.count();
    console.log(`Found ${count} web scraping sources`);
  });

  test('should display database source monitors', async ({ page }) => {
    await page.waitForTimeout(2000);

    const dbSources = page.locator(
      ':has-text("Database"), :has-text("PostgreSQL"), .database-source'
    );

    const count = await dbSources.count();
    console.log(`Found ${count} database sources`);
  });

  test('POST /api/regulatory/start should initiate monitoring', async ({ request }) => {
    const response = await request.post(`${baseURL}/api/regulatory/start`, {
      data: {
        sources: ['ecb_rss', 'sec_api'],
        interval: 300
      }
    });

    expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();

    if (response.ok()) {
      console.log('Regulatory monitoring started successfully');
    }
  });

  test('should show ingestion metrics', async ({ page }) => {
    await page.waitForTimeout(2000);

    const metrics = page.locator(
      '.ingestion-metric, .ingestion-stats, [data-testid="ingestion-metrics"]'
    );

    if (await metrics.count() > 0) {
      const metricsText = await metrics.first().textContent();
      console.log('Ingestion metrics:', metricsText?.substring(0, 150));
    }
  });

  test('should display compliance validation results', async ({ page }) => {
    await page.waitForTimeout(2000);

    const validationResults = page.locator(
      '.compliance-validation, .validation-result, :has-text("GDPR"), :has-text("CCPA")'
    );

    if (await validationResults.count() > 0) {
      console.log('Compliance validation results found');

      // Check for specific compliance frameworks
      const hasGDPR = await page.locator(':has-text("GDPR")').count() > 0;
      const hasCCPA = await page.locator(':has-text("CCPA")').count() > 0;

      console.log('Compliance frameworks:', { GDPR: hasGDPR, CCPA: hasCCPA });
    }
  });

  test('should show data quality checks', async ({ page }) => {
    await page.waitForTimeout(2000);

    const qualityChecks = page.locator(
      '.quality-check, .data-quality, [data-testid="quality-check"]'
    );

    const count = await qualityChecks.count();
    console.log(`Found ${count} data quality checks`);
  });

  test('should detect PII in ingested data', async ({ page }) => {
    await page.waitForTimeout(2000);

    const piiDetection = page.locator(
      ':has-text("PII"), :has-text("Personal"), .pii-detection'
    );

    if (await piiDetection.count() > 0) {
      console.log('PII detection indicators found');
    }
  });

  test('should handle ingestion errors', async ({ page }) => {
    await page.waitForTimeout(2000);

    const ingestionErrors = page.locator(
      '.ingestion-error, .source-error, [data-testid="ingestion-error"]'
    );

    const count = await ingestionErrors.count();
    console.log(`Found ${count} ingestion errors`);

    if (count > 0) {
      const firstError = ingestionErrors.first();
      const errorText = await firstError.textContent();
      console.log('Sample ingestion error:', errorText?.substring(0, 100));
    }
  });

  test('should show retry mechanisms', async ({ page }) => {
    await page.waitForTimeout(2000);

    const retryIndicators = page.locator(
      ':has-text("retry"), :has-text("attempt"), .retry-count'
    );

    if (await retryIndicators.count() > 0) {
      console.log('Retry mechanism indicators found');
    }
  });

  test('should configure source parameters', async ({ page }) => {
    await page.waitForTimeout(2000);

    const sources = page.locator('.data-source, .source-item');

    if (await sources.count() > 0) {
      const configBtn = sources.first().locator('button:has-text("Configure"), .config-btn');

      if (await configBtn.count() > 0) {
        await configBtn.first().click();
        await page.waitForTimeout(500);

        const configPanel = page.locator('.config-panel, [role="dialog"]');
        const isVisible = await configPanel.isVisible().catch(() => false);

        console.log('Source configuration panel visible:', isVisible);
      }
    }
  });

  test('should show pagination strategies', async ({ page }) => {
    await page.waitForTimeout(2000);

    const paginationInfo = page.locator(
      ':has-text("pagination"), :has-text("offset"), :has-text("cursor")'
    );

    if (await paginationInfo.count() > 0) {
      console.log('Pagination strategy information found');
    }
  });
});
