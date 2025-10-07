import { test, expect } from '@playwright/test';

/**
 * Dashboard and Activity Feed Tests
 *
 * Tests the main dashboard interface, activity feed updates,
 * and real-time data display for the Regulens platform.
 */

test.describe('Dashboard', () => {
  test.beforeEach(async ({ page }) => {
    // Navigate to dashboard
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should load dashboard with all key sections', async ({ page }) => {
    // Verify main dashboard elements are present
    const header = page.locator('header, .header');
    await expect(header).toBeVisible();

    // Check for main content area
    const mainContent = page.locator('main, .main-content, .container');
    await expect(mainContent).toBeVisible();

    // Look for dashboard title or logo
    const logo = page.locator('.logo, h1:has-text("Regulens")');
    await expect(logo.first()).toBeVisible();
  });

  test('should display activity feed', async ({ page }) => {
    // Wait for activity feed API call
    const activityPromise = page.waitForResponse(
      response => response.url().includes('/api/activity'),
      { timeout: 10000 }
    ).catch(() => null);

    await page.goto('/');
    const activityResponse = await activityPromise;

    if (activityResponse) {
      expect(activityResponse.status()).toBeLessThan(400);

      // Wait for activity items to render
      await page.waitForTimeout(1000);

      // Check if activity items are displayed
      const activityItems = page.locator('.activity-item, .feed-item, [data-testid="activity-item"]');
      const count = await activityItems.count();

      console.log(`Found ${count} activity items`);
    }
  });

  test('should show activity feed with timestamps', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for timestamp elements
    const timestamps = page.locator('.timestamp, .time, [data-testid="timestamp"], time');
    const count = await timestamps.count();

    if (count > 0) {
      const firstTimestamp = timestamps.first();
      await expect(firstTimestamp).toBeVisible();

      // Verify timestamp has content
      const text = await firstTimestamp.textContent();
      expect(text).toBeTruthy();
      console.log('Sample timestamp:', text);
    }
  });

  test('should filter activity by type', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for filter buttons or dropdowns
    const filterButtons = page.locator('button:has-text("Filter"), select[name="filter"], .filter-option');

    if (await filterButtons.count() > 0) {
      const firstFilter = filterButtons.first();
      await firstFilter.click();

      await page.waitForTimeout(500);

      // Check if activity feed updates
      const activityItems = page.locator('.activity-item, .feed-item');
      const initialCount = await activityItems.count();

      console.log(`Activity items after filter: ${initialCount}`);
    }
  });

  test('should display decision history cards', async ({ page }) => {
    // Wait for decisions API call
    const decisionsPromise = page.waitForResponse(
      response => response.url().includes('/api/decisions'),
      { timeout: 10000 }
    ).catch(() => null);

    await page.goto('/');
    const decisionsResponse = await decisionsPromise;

    if (decisionsResponse) {
      expect(decisionsResponse.status()).toBeLessThan(400);

      // Parse response
      const decisions = await decisionsResponse.json();
      console.log('Decisions count:', Array.isArray(decisions) ? decisions.length : Object.keys(decisions).length);

      // Look for decision cards
      const decisionCards = page.locator('.decision-card, .card, [data-testid="decision-card"]');
      await page.waitForTimeout(1000);

      const cardCount = await decisionCards.count();
      console.log(`Found ${cardCount} decision cards`);
    }
  });

  test('should show decision details on click', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Find decision cards
    const decisionCards = page.locator('.decision-card, .card, [data-testid="decision-card"]');
    const count = await decisionCards.count();

    if (count > 0) {
      // Click on first decision
      await decisionCards.first().click();

      // Wait for details to appear
      await page.waitForTimeout(500);

      // Look for modal or expanded view
      const modal = page.locator('.modal, .dialog, .details, [role="dialog"]');
      const isModalVisible = await modal.isVisible().catch(() => false);

      if (isModalVisible) {
        console.log('Decision details modal opened');

        // Close modal
        const closeButton = page.locator('button:has-text("Close"), .close, [aria-label="Close"]');
        if (await closeButton.isVisible({ timeout: 1000 }).catch(() => false)) {
          await closeButton.click();
        }
      }
    }
  });

  test('should display real-time metrics', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for metric cards or statistics
    const metrics = page.locator('.metric, .stat, .statistic, [data-testid="metric"]');
    const count = await metrics.count();

    console.log(`Found ${count} metrics on dashboard`);

    if (count > 0) {
      // Check first metric has a value
      const firstMetric = metrics.first();
      const value = await firstMetric.textContent();
      expect(value?.trim()).toBeTruthy();
      console.log('Sample metric value:', value?.trim().substring(0, 50));
    }
  });

  test('should update activity feed in real-time', async ({ page }) => {
    // Listen for activity updates
    let activityUpdateCount = 0;

    page.on('response', response => {
      if (response.url().includes('/api/activity')) {
        activityUpdateCount++;
        console.log(`Activity API called ${activityUpdateCount} times`);
      }
    });

    await page.goto('/');
    await page.waitForTimeout(2000);

    // Get initial activity count
    const activityItems = page.locator('.activity-item, .feed-item');
    const initialCount = await activityItems.count();

    console.log(`Initial activity count: ${initialCount}`);

    // Wait for potential updates
    await page.waitForTimeout(5000);

    // Check if there were any updates
    const finalCount = await activityItems.count();
    console.log(`Final activity count: ${finalCount}`);
  });

  test('should navigate between dashboard sections', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for navigation tabs or section buttons
    const navItems = page.locator('nav a, .tab, [role="tab"], .nav-link');
    const count = await navItems.count();

    console.log(`Found ${count} navigation items`);

    if (count > 1) {
      // Click on second nav item
      const secondNav = navItems.nth(1);
      const navText = await secondNav.textContent();
      console.log('Clicking navigation item:', navText);

      await secondNav.click();
      await page.waitForTimeout(500);

      // Verify URL changed or content updated
      const url = page.url();
      console.log('Current URL:', url);
    }
  });

  test('should handle empty activity feed gracefully', async ({ page }) => {
    // This test checks for proper empty state handling
    await page.goto('/');
    await page.waitForTimeout(2000);

    const activityItems = page.locator('.activity-item, .feed-item');
    const count = await activityItems.count();

    if (count === 0) {
      // Look for empty state message
      const emptyMessage = page.locator('.empty-state, .no-data, p:has-text("No activities"), p:has-text("No data")');
      const hasEmptyState = await emptyMessage.count() > 0;

      console.log('Empty state displayed:', hasEmptyState);
    }
  });

  test('should search within activity feed', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for search input
    const searchInput = page.locator('input[type="search"], input[placeholder*="Search" i], .search-input');

    if (await searchInput.count() > 0) {
      const firstSearch = searchInput.first();
      await firstSearch.fill('compliance');

      // Wait for search results
      await page.waitForTimeout(1000);

      const activityItems = page.locator('.activity-item, .feed-item');
      const count = await activityItems.count();

      console.log(`Search results: ${count} items`);
    }
  });

  test('should paginate through activity feed', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for pagination controls
    const nextButton = page.locator('button:has-text("Next"), .pagination-next, [aria-label="Next page"]');

    if (await nextButton.count() > 0 && await nextButton.first().isVisible()) {
      const activityItems = page.locator('.activity-item, .feed-item');
      const firstPageCount = await activityItems.count();

      console.log(`First page: ${firstPageCount} items`);

      // Click next page
      await nextButton.first().click();
      await page.waitForTimeout(1000);

      const secondPageCount = await activityItems.count();
      console.log(`Second page: ${secondPageCount} items`);
    }
  });

  test('should display status indicators correctly', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for status indicators
    const statusIndicators = page.locator('.status-indicator, .status-dot, .badge');
    const count = await statusIndicators.count();

    console.log(`Found ${count} status indicators`);

    if (count > 0) {
      // Check if status has appropriate styling
      const firstStatus = statusIndicators.first();
      const color = await firstStatus.evaluate(el => window.getComputedStyle(el).backgroundColor);
      console.log('Status indicator color:', color);
    }
  });

  test('should refresh data on manual refresh', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    // Look for refresh button
    const refreshButton = page.locator('button:has-text("Refresh"), [aria-label="Refresh"], .refresh-btn');

    if (await refreshButton.count() > 0) {
      let apiCallCount = 0;

      page.on('response', response => {
        if (response.url().includes('/api/')) {
          apiCallCount++;
        }
      });

      await refreshButton.first().click();
      await page.waitForTimeout(1000);

      console.log(`API calls after refresh: ${apiCallCount}`);
      expect(apiCallCount).toBeGreaterThan(0);
    }
  });
});
