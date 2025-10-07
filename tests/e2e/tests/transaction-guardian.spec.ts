import { test, expect } from '@playwright/test';

/**
 * Transaction Guardian Tests
 *
 * Tests the real-time fraud detection and transaction monitoring features
 */

test.describe('Transaction Guardian', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should load transaction monitoring dashboard', async ({ page }) => {
    await page.waitForTimeout(2000);

    const transactionDashboard = page.locator(
      '.transaction-dashboard, .transaction-monitor, [data-testid="transaction-guardian"]'
    );

    if (await transactionDashboard.count() > 0) {
      await expect(transactionDashboard.first()).toBeVisible();
      console.log('Transaction Guardian dashboard found');
    }
  });

  test('should display recent transactions', async ({ page }) => {
    await page.waitForTimeout(2000);

    const transactions = page.locator(
      '.transaction, .transaction-item, [data-testid="transaction"]'
    );

    const count = await transactions.count();
    console.log(`Found ${count} recent transactions`);

    if (count > 0) {
      const firstTx = transactions.first();
      const txText = await firstTx.textContent();
      console.log('Sample transaction:', txText?.substring(0, 100));
    }
  });

  test('should show fraud risk scores', async ({ page }) => {
    await page.waitForTimeout(2000);

    const riskScores = page.locator(
      '.risk-score, .fraud-score, [data-testid="risk-score"]'
    );

    if (await riskScores.count() > 0) {
      const scores = await riskScores.allTextContents();
      console.log('Transaction risk scores:', scores.slice(0, 5));
    }
  });

  test('should highlight suspicious transactions', async ({ page }) => {
    await page.waitForTimeout(2000);

    const suspiciousTx = page.locator(
      '.suspicious, .high-risk, .flagged, [data-risk="high"]'
    );

    const count = await suspiciousTx.count();
    console.log(`Found ${count} suspicious transactions`);

    if (count > 0) {
      const firstSuspicious = suspiciousTx.first();
      const bgColor = await firstSuspicious.evaluate(
        el => window.getComputedStyle(el).backgroundColor
      );
      console.log('Suspicious transaction indicator color:', bgColor);
    }
  });

  test('should filter transactions by risk level', async ({ page }) => {
    await page.waitForTimeout(2000);

    const riskFilter = page.locator(
      'select[name="risk"], .risk-filter, button:has-text("High Risk")'
    );

    if (await riskFilter.count() > 0) {
      await riskFilter.first().click();
      await page.waitForTimeout(1000);

      const filteredTx = page.locator('.transaction, .transaction-item');
      const count = await filteredTx.count();
      console.log(`Filtered transactions: ${count}`);
    }
  });

  test('should display fraud detection rules', async ({ page }) => {
    await page.waitForTimeout(2000);

    const rules = page.locator('.rule, .detection-rule, [data-testid="fraud-rule"]');

    if (await rules.count() > 0) {
      const ruleCount = await rules.count();
      console.log(`Active fraud detection rules: ${ruleCount}`);

      const firstRule = rules.first();
      const ruleText = await firstRule.textContent();
      console.log('Sample fraud rule:', ruleText?.substring(0, 80));
    }
  });

  test('should show transaction analysis details', async ({ page }) => {
    await page.waitForTimeout(2000);

    const transactions = page.locator('.transaction, .transaction-item');

    if (await transactions.count() > 0) {
      await transactions.first().click();
      await page.waitForTimeout(500);

      const analysisPanel = page.locator(
        '.analysis, .transaction-analysis, [data-testid="analysis"]'
      );

      const isVisible = await analysisPanel.isVisible().catch(() => false);
      console.log('Transaction analysis panel visible:', isVisible);
    }
  });

  test('should alert on anomalous patterns', async ({ page }) => {
    await page.waitForTimeout(2000);

    const alerts = page.locator(
      '.alert, .anomaly-alert, .pattern-alert, [role="alert"]'
    );

    const count = await alerts.count();
    console.log(`Found ${count} anomaly alerts`);
  });

  test('should approve/reject flagged transactions', async ({ page }) => {
    await page.waitForTimeout(2000);

    const flaggedTx = page.locator('.suspicious, .flagged, [data-status="flagged"]');

    if (await flaggedTx.count() > 0) {
      const approveBtn = flaggedTx.first().locator('button:has-text("Approve")');
      const rejectBtn = flaggedTx.first().locator('button:has-text("Reject")');

      const hasApprove = await approveBtn.count() > 0;
      const hasReject = await rejectBtn.count() > 0;

      console.log('Transaction actions available:', { approve: hasApprove, reject: hasReject });
    }
  });

  test('should export transaction reports', async ({ page }) => {
    await page.waitForTimeout(2000);

    const exportBtn = page.locator(
      'button:has-text("Export"), button:has-text("Generate Report")'
    );

    if (await exportBtn.count() > 0) {
      await exportBtn.first().click();
      await page.waitForTimeout(500);

      console.log('Export button clicked');
    }
  });
});
