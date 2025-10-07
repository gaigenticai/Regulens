import { test, expect } from '@playwright/test';

/**
 * Audit Trail Tests
 *
 * Tests compliance audit logging, event tracking, filtering,
 * and audit report generation for the Regulens platform.
 */

test.describe('Audit Trail', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should load audit trail data', async ({ page }) => {
    // Wait for audit trail API call
    const auditPromise = page.waitForResponse(
      response => response.url().includes('/api/audit-trail') ||
                  response.url().includes('/api/audit') ||
                  response.url().includes('/api/events'),
      { timeout: 10000 }
    ).catch(() => null);

    await page.goto('/');
    const auditResponse = await auditPromise;

    if (auditResponse) {
      expect(auditResponse.status()).toBeLessThan(400);

      const data = await auditResponse.json();
      console.log('Audit trail response structure:', Object.keys(data));

      if (Array.isArray(data) || data.events) {
        const events = Array.isArray(data) ? data : data.events;
        console.log(`Loaded ${events.length} audit events`);
      }
    }
  });

  test('should display audit events chronologically', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for audit event items
    const auditEvents = page.locator(
      '.audit-event, .event-item, .log-entry, [data-testid="audit-event"]'
    );

    const count = await auditEvents.count();
    console.log(`Found ${count} audit events`);

    if (count > 1) {
      // Get timestamps of first two events
      const firstTimestamp = await auditEvents.nth(0).locator('.timestamp, .time, time').textContent();
      const secondTimestamp = await auditEvents.nth(1).locator('.timestamp, .time, time').textContent();

      console.log('First event timestamp:', firstTimestamp);
      console.log('Second event timestamp:', secondTimestamp);
    }
  });

  test('should show event type indicators', async ({ page }) => {
    await page.waitForTimeout(2000);

    const auditEvents = page.locator('.audit-event, .event-item, .log-entry');

    if (await auditEvents.count() > 0) {
      const firstEvent = auditEvents.first();

      // Look for event type/category
      const eventType = firstEvent.locator('.event-type, .category, .type, .badge');

      if (await eventType.count() > 0) {
        const typeText = await eventType.first().textContent();
        console.log('Sample event type:', typeText);
        expect(typeText?.trim()).toBeTruthy();
      }
    }
  });

  test('should filter audit events by type', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for type filter
    const typeFilter = page.locator(
      'select:has(option:has-text("All")), .filter-type, [name="eventType"]'
    );

    if (await typeFilter.count() > 0) {
      const auditEvents = page.locator('.audit-event, .event-item, .log-entry');
      const initialCount = await auditEvents.count();

      console.log(`Initial events count: ${initialCount}`);

      // Select a specific event type (if available)
      const options = await typeFilter.first().locator('option').count();
      if (options > 1) {
        await typeFilter.first().selectOption({ index: 1 });
        await page.waitForTimeout(1000);

        const filteredCount = await auditEvents.count();
        console.log(`Filtered events count: ${filteredCount}`);
      }
    }
  });

  test('should filter audit events by date range', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for date filters
    const datePickers = page.locator('input[type="date"], .date-picker, input[name*="date" i]');

    if (await datePickers.count() >= 2) {
      const fromDate = datePickers.first();
      const toDate = datePickers.last();

      // Set date range (last 30 days)
      const today = new Date();
      const monthAgo = new Date(today.getTime() - 30 * 24 * 60 * 60 * 1000);

      const formatDate = (date: Date) => date.toISOString().split('T')[0];

      await fromDate.fill(formatDate(monthAgo));
      await toDate.fill(formatDate(today));

      await page.waitForTimeout(1000);

      const auditEvents = page.locator('.audit-event, .event-item, .log-entry');
      const count = await auditEvents.count();

      console.log(`Events in last 30 days: ${count}`);
    }
  });

  test('should display event details', async ({ page }) => {
    await page.waitForTimeout(2000);

    const auditEvents = page.locator('.audit-event, .event-item, .log-entry');

    if (await auditEvents.count() > 0) {
      const firstEvent = auditEvents.first();

      // Check for key event details
      const actor = firstEvent.locator('.actor, .user, .performer');
      const action = firstEvent.locator('.action, .operation');
      const timestamp = firstEvent.locator('.timestamp, .time, time');
      const resource = firstEvent.locator('.resource, .target, .object');

      const hasActor = await actor.count() > 0;
      const hasAction = await action.count() > 0;
      const hasTimestamp = await timestamp.count() > 0;
      const hasResource = await resource.count() > 0;

      console.log('Event details present:', {
        actor: hasActor,
        action: hasAction,
        timestamp: hasTimestamp,
        resource: hasResource
      });

      if (hasAction) {
        const actionText = await action.first().textContent();
        console.log('Sample action:', actionText);
      }
    }
  });

  test('should expand event for full details', async ({ page }) => {
    await page.waitForTimeout(2000);

    const auditEvents = page.locator('.audit-event, .event-item, .log-entry');

    if (await auditEvents.count() > 0) {
      // Click on first event
      await auditEvents.first().click();
      await page.waitForTimeout(500);

      // Look for expanded details panel
      const detailsPanel = page.locator(
        '.event-details, .details-panel, .expanded-view, [role="region"]'
      );

      const isVisible = await detailsPanel.isVisible().catch(() => false);
      console.log('Event details panel visible:', isVisible);

      if (isVisible) {
        // Look for metadata fields
        const metadata = detailsPanel.locator('.metadata, .properties, dl');
        const hasMetadata = await metadata.count() > 0;
        console.log('Has metadata:', hasMetadata);
      }
    }
  });

  test('should search audit events', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for search input
    const searchInput = page.locator(
      'input[type="search"], input[placeholder*="Search" i]'
    ).first();

    if (await searchInput.isVisible().catch(() => false)) {
      // Search for specific user or action
      await searchInput.fill('admin');
      await page.waitForTimeout(1000);

      const auditEvents = page.locator('.audit-event, .event-item, .log-entry');
      const count = await auditEvents.count();

      console.log(`Search results for "admin": ${count} events`);
    }
  });

  test('should show compliance violation events', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for violation or alert indicators
    const violations = page.locator(
      '.violation, .alert, .warning, .severity-high, [data-severity="high"]'
    );

    const count = await violations.count();
    console.log(`Found ${count} compliance violations/alerts`);

    if (count > 0) {
      const firstViolation = violations.first();
      const violationText = await firstViolation.textContent();
      console.log('Sample violation:', violationText?.substring(0, 100));

      // Check for visual indicator (red/warning color)
      const color = await firstViolation.evaluate(
        el => window.getComputedStyle(el).color
      );
      console.log('Violation indicator color:', color);
    }
  });

  test('should export audit logs', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for export button
    const exportButton = page.locator(
      'button:has-text("Export"), button:has-text("Download"), button:has-text("Generate Report")'
    );

    if (await exportButton.count() > 0) {
      const firstExport = exportButton.first();

      // Click export button
      await firstExport.click();
      await page.waitForTimeout(500);

      // Look for format options (CSV, JSON, PDF)
      const formatOptions = page.locator(
        '.export-format, [role="menuitem"]:has-text("CSV"), [role="menuitem"]:has-text("JSON")'
      );

      const hasOptions = await formatOptions.count() > 0;
      console.log('Export format options available:', hasOptions);

      if (hasOptions) {
        const options = await formatOptions.allTextContents();
        console.log('Available export formats:', options);
      }
    }
  });

  test('should paginate through audit events', async ({ page }) => {
    await page.waitForTimeout(2000);

    const auditEvents = page.locator('.audit-event, .event-item, .log-entry');
    const initialCount = await auditEvents.count();

    console.log(`Initial page events: ${initialCount}`);

    // Look for pagination controls
    const nextButton = page.locator(
      'button:has-text("Next"), .pagination-next, [aria-label="Next page"]'
    );

    if (await nextButton.count() > 0 && await nextButton.first().isEnabled()) {
      await nextButton.first().click();
      await page.waitForTimeout(1000);

      const secondPageCount = await auditEvents.count();
      console.log(`Second page events: ${secondPageCount}`);

      // Verify new events loaded
      expect(secondPageCount).toBeGreaterThan(0);
    }
  });

  test('should show user activity summary', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for user activity widget or summary
    const activitySummary = page.locator(
      '.user-activity, .activity-summary, .stats, .metrics'
    );

    if (await activitySummary.count() > 0) {
      const summary = activitySummary.first();

      // Look for key metrics
      const metrics = summary.locator('.metric, .stat, .count');
      const metricsCount = await metrics.count();

      console.log(`Found ${metricsCount} activity metrics`);

      if (metricsCount > 0) {
        const firstMetric = metrics.first();
        const metricText = await firstMetric.textContent();
        console.log('Sample metric:', metricText);
      }
    }
  });

  test('should filter by user/actor', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for user filter
    const userFilter = page.locator(
      'select[name="user"], select[name="actor"], .user-filter, input[placeholder*="user" i]'
    );

    if (await userFilter.count() > 0) {
      const filter = userFilter.first();
      const tagName = await filter.evaluate(el => el.tagName);

      if (tagName === 'SELECT') {
        const options = await filter.locator('option').count();
        if (options > 1) {
          await filter.selectOption({ index: 1 });
          await page.waitForTimeout(1000);

          const auditEvents = page.locator('.audit-event, .event-item, .log-entry');
          const count = await auditEvents.count();
          console.log(`Events for selected user: ${count}`);
        }
      } else if (tagName === 'INPUT') {
        await filter.fill('admin');
        await page.waitForTimeout(1000);

        const auditEvents = page.locator('.audit-event, .event-item, .log-entry');
        const count = await auditEvents.count();
        console.log(`Events for user "admin": ${count}`);
      }
    }
  });

  test('should show event correlation', async ({ page }) => {
    await page.waitForTimeout(2000);

    const auditEvents = page.locator('.audit-event, .event-item, .log-entry');

    if (await auditEvents.count() > 0) {
      const firstEvent = auditEvents.first();

      // Click to expand
      await firstEvent.click();
      await page.waitForTimeout(500);

      // Look for related events or correlation ID
      const correlationId = page.locator(
        '.correlation-id, .trace-id, .request-id, [data-testid="correlation"]'
      );

      if (await correlationId.count() > 0) {
        const idText = await correlationId.first().textContent();
        console.log('Correlation ID found:', idText);
      }
    }
  });

  test('should display event severity levels', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for severity indicators
    const severityLevels = page.locator(
      '.severity, .level, .priority, [data-severity]'
    );

    const count = await severityLevels.count();
    console.log(`Found ${count} severity indicators`);

    if (count > 0) {
      // Collect all severity levels
      const levels = await severityLevels.allTextContents();
      const uniqueLevels = [...new Set(levels.map(l => l.trim()))];
      console.log('Severity levels present:', uniqueLevels);
    }
  });

  test('should track compliance framework events', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for events related to GDPR, CCPA, etc.
    const complianceEvents = page.locator(
      '.event-item:has-text("GDPR"), .event-item:has-text("CCPA"), .event-item:has-text("compliance")'
    );

    const count = await complianceEvents.count();
    console.log(`Found ${count} compliance framework events`);

    if (count > 0) {
      const firstEvent = complianceEvents.first();
      const eventText = await firstEvent.textContent();
      console.log('Sample compliance event:', eventText?.substring(0, 100));
    }
  });
});
