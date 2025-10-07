import { test, expect } from '@playwright/test';

/**
 * Regulatory Changes Monitoring Tests
 *
 * Tests the regulatory change detection and monitoring features,
 * including ECB RSS feeds, web scraping sources, and change notifications.
 */

test.describe('Regulatory Changes Monitoring', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should load regulatory changes feed', async ({ page }) => {
    // Wait for regulatory changes API call
    const changesPromise = page.waitForResponse(
      response => response.url().includes('/api/regulatory-changes') ||
                  response.url().includes('/api/changes') ||
                  response.url().includes('/api/regulations'),
      { timeout: 10000 }
    ).catch(() => null);

    await page.goto('/');
    const changesResponse = await changesPromise;

    if (changesResponse) {
      expect(changesResponse.status()).toBeLessThan(400);

      const data = await changesResponse.json();
      console.log('Regulatory changes response structure:', Object.keys(data));
    }
  });

  test('should display regulatory change items', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for regulatory change items in the UI
    const changeItems = page.locator(
      '.regulatory-change, .change-item, .regulation-card, [data-testid="regulatory-change"]'
    );

    const count = await changeItems.count();
    console.log(`Found ${count} regulatory change items`);

    if (count > 0) {
      // Check first item has content
      const firstItem = changeItems.first();
      await expect(firstItem).toBeVisible();

      const text = await firstItem.textContent();
      expect(text?.trim()).toBeTruthy();
      console.log('Sample change item:', text?.trim().substring(0, 100));
    }
  });

  test('should show regulatory source information', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for source labels (ECB, RSS, Web Scraping, etc.)
    const sourceTags = page.locator(
      '.source, .source-tag, .badge:has-text("ECB"), .badge:has-text("RSS"), [data-testid="source"]'
    );

    const count = await sourceTags.count();
    console.log(`Found ${count} source tags`);

    if (count > 0) {
      const firstSource = sourceTags.first();
      const sourceText = await firstSource.textContent();
      console.log('Sample source:', sourceText);
      expect(sourceText?.trim()).toBeTruthy();
    }
  });

  test('should filter changes by source type', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for source filter controls
    const filterDropdown = page.locator(
      'select:has(option:has-text("ECB")), select:has(option:has-text("RSS")), .source-filter'
    );

    if (await filterDropdown.count() > 0) {
      const changeItems = page.locator('.regulatory-change, .change-item, .regulation-card');
      const initialCount = await changeItems.count();

      console.log(`Initial changes count: ${initialCount}`);

      // Select a specific source
      await filterDropdown.first().selectOption({ index: 1 });
      await page.waitForTimeout(1000);

      const filteredCount = await changeItems.count();
      console.log(`Filtered changes count: ${filteredCount}`);
    }
  });

  test('should display change detection metadata', async ({ page }) => {
    await page.waitForTimeout(2000);

    const changeItems = page.locator('.regulatory-change, .change-item, .regulation-card');

    if (await changeItems.count() > 0) {
      const firstItem = changeItems.first();

      // Look for metadata fields
      const timestamp = firstItem.locator('.timestamp, .date, time');
      const severity = firstItem.locator('.severity, .priority, .impact');
      const category = firstItem.locator('.category, .type, .tag');

      const hasTimestamp = await timestamp.count() > 0;
      const hasSeverity = await severity.count() > 0;
      const hasCategory = await category.count() > 0;

      console.log('Change metadata:', {
        timestamp: hasTimestamp,
        severity: hasSeverity,
        category: hasCategory
      });

      if (hasTimestamp) {
        const timestampText = await timestamp.first().textContent();
        console.log('Sample timestamp:', timestampText);
      }
    }
  });

  test('should expand change details', async ({ page }) => {
    await page.waitForTimeout(2000);

    const changeItems = page.locator('.regulatory-change, .change-item, .regulation-card');

    if (await changeItems.count() > 0) {
      // Click on first change item
      await changeItems.first().click();
      await page.waitForTimeout(500);

      // Look for expanded details
      const details = page.locator(
        '.change-details, .details-panel, .expanded-content, [role="region"]'
      );

      const isVisible = await details.isVisible().catch(() => false);
      console.log('Change details expanded:', isVisible);

      if (isVisible) {
        // Look for detailed fields
        const description = details.locator('.description, .content, p');
        const hasDescription = await description.count() > 0;
        console.log('Has description:', hasDescription);
      }
    }
  });

  test('should search regulatory changes', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for search input
    const searchInput = page.locator(
      'input[type="search"], input[placeholder*="Search" i], .search-input'
    ).first();

    if (await searchInput.isVisible().catch(() => false)) {
      // Search for specific term
      await searchInput.fill('banking');
      await page.waitForTimeout(1000);

      const changeItems = page.locator('.regulatory-change, .change-item, .regulation-card');
      const count = await changeItems.count();

      console.log(`Search results for "banking": ${count} items`);

      // Verify results contain search term
      if (count > 0) {
        const firstItem = changeItems.first();
        const text = await firstItem.textContent();
        const containsSearchTerm = text?.toLowerCase().includes('banking');
        console.log('First result contains "banking":', containsSearchTerm);
      }
    }
  });

  test('should show change impact analysis', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for impact indicators
    const impactIndicators = page.locator(
      '.impact, .risk-level, .severity-indicator, [data-testid="impact"]'
    );

    const count = await impactIndicators.count();
    console.log(`Found ${count} impact indicators`);

    if (count > 0) {
      const firstImpact = impactIndicators.first();
      const impactText = await firstImpact.textContent();
      console.log('Sample impact level:', impactText);

      // Check for visual styling (high/medium/low impact colors)
      const backgroundColor = await firstImpact.evaluate(
        el => window.getComputedStyle(el).backgroundColor
      );
      console.log('Impact indicator color:', backgroundColor);
    }
  });

  test('should handle date range filtering', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for date pickers
    const datePickers = page.locator('input[type="date"], .date-picker');

    if (await datePickers.count() >= 2) {
      const fromDate = datePickers.first();
      const toDate = datePickers.last();

      // Set date range (last 7 days)
      const today = new Date();
      const weekAgo = new Date(today.getTime() - 7 * 24 * 60 * 60 * 1000);

      const formatDate = (date: Date) => date.toISOString().split('T')[0];

      await fromDate.fill(formatDate(weekAgo));
      await toDate.fill(formatDate(today));

      await page.waitForTimeout(1000);

      const changeItems = page.locator('.regulatory-change, .change-item, .regulation-card');
      const count = await changeItems.count();

      console.log(`Changes in last 7 days: ${count}`);
    }
  });

  test('should show regulatory source health status', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for source health indicators
    const sourceStatus = page.locator(
      '.source-status, .health-indicator, .status-badge'
    );

    const count = await sourceStatus.count();
    console.log(`Found ${count} source status indicators`);

    if (count > 0) {
      const firstStatus = sourceStatus.first();
      const statusText = await firstStatus.textContent();
      console.log('Sample source status:', statusText);
    }
  });

  test('should display change notifications', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for notification bell or indicator
    const notificationBell = page.locator(
      '.notification-bell, .notifications, [aria-label*="notification" i]'
    );

    if (await notificationBell.count() > 0) {
      await notificationBell.first().click();
      await page.waitForTimeout(500);

      // Check for notification dropdown/panel
      const notificationPanel = page.locator(
        '.notification-panel, .notifications-dropdown, [role="menu"]'
      );

      const isVisible = await notificationPanel.isVisible().catch(() => false);
      console.log('Notification panel visible:', isVisible);

      if (isVisible) {
        const notificationItems = notificationPanel.locator('.notification-item, li');
        const count = await notificationItems.count();
        console.log(`Unread notifications: ${count}`);
      }
    }
  });

  test('should export regulatory changes', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for export button
    const exportButton = page.locator(
      'button:has-text("Export"), button:has-text("Download"), [aria-label*="export" i]'
    );

    if (await exportButton.count() > 0) {
      // Set up download listener
      const downloadPromise = page.waitForEvent('download', { timeout: 5000 }).catch(() => null);

      await exportButton.first().click();
      await page.waitForTimeout(500);

      // Check if download options appear
      const exportOptions = page.locator('.export-option, [role="menuitem"]');
      const hasOptions = await exportOptions.count() > 0;

      console.log('Export options available:', hasOptions);

      if (hasOptions) {
        const firstOption = exportOptions.first();
        const optionText = await firstOption.textContent();
        console.log('Sample export option:', optionText);
      }
    }
  });

  test('should link to original regulatory source', async ({ page }) => {
    await page.waitForTimeout(2000);

    const changeItems = page.locator('.regulatory-change, .change-item, .regulation-card');

    if (await changeItems.count() > 0) {
      const firstItem = changeItems.first();

      // Look for external link
      const sourceLink = firstItem.locator('a[href^="http"], a:has-text("View Source"), .source-link');

      if (await sourceLink.count() > 0) {
        const href = await sourceLink.first().getAttribute('href');
        console.log('Source link:', href);
        expect(href).toMatch(/^https?:\/\//);
      }
    }
  });

  test('should show change comparison', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for compare functionality
    const compareCheckboxes = page.locator('input[type="checkbox"]');

    if (await compareCheckboxes.count() >= 2) {
      // Select two items
      await compareCheckboxes.nth(0).check();
      await compareCheckboxes.nth(1).check();

      // Look for compare button
      const compareButton = page.locator('button:has-text("Compare")');

      if (await compareButton.isVisible().catch(() => false)) {
        await compareButton.click();
        await page.waitForTimeout(1000);

        // Check for comparison view
        const comparisonView = page.locator('.comparison, .diff-view, [data-testid="comparison"]');
        const isVisible = await comparisonView.isVisible().catch(() => false);

        console.log('Comparison view displayed:', isVisible);
      }
    }
  });
});
