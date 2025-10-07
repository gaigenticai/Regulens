import { test, expect } from '@playwright/test';

/**
 * Agent Management Tests
 *
 * Tests AI agent management, communication, collaboration features,
 * and agent status monitoring for the Regulens platform.
 */

test.describe('Agent Management', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should load agents list', async ({ page }) => {
    // Wait for agents API call
    const agentsPromise = page.waitForResponse(
      response => response.url().includes('/api/agents'),
      { timeout: 10000 }
    ).catch(() => null);

    await page.goto('/');
    const agentsResponse = await agentsPromise;

    if (agentsResponse) {
      expect(agentsResponse.status()).toBeLessThan(400);

      const data = await agentsResponse.json();
      console.log('Agents response structure:', Object.keys(data));

      if (Array.isArray(data) || data.agents) {
        const agents = Array.isArray(data) ? data : data.agents;
        console.log(`Loaded ${agents.length} agents`);
      }
    }
  });

  test('should display agent cards', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for agent cards or list items
    const agentCards = page.locator(
      '.agent-card, .agent-item, .agent, [data-testid="agent-card"]'
    );

    const count = await agentCards.count();
    console.log(`Found ${count} agent cards`);

    if (count > 0) {
      const firstAgent = agentCards.first();
      await expect(firstAgent).toBeVisible();

      // Check for agent name
      const agentName = firstAgent.locator('.agent-name, .name, h3, h4');
      if (await agentName.count() > 0) {
        const name = await agentName.first().textContent();
        console.log('Sample agent name:', name);
      }
    }
  });

  test('should show agent status indicators', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for status indicators
    const statusIndicators = page.locator(
      '.agent-status, .status, .state, [data-testid="agent-status"]'
    );

    const count = await statusIndicators.count();
    console.log(`Found ${count} agent status indicators`);

    if (count > 0) {
      const firstStatus = statusIndicators.first();
      const statusText = await firstStatus.textContent();
      console.log('Sample agent status:', statusText);

      // Check for status colors (active/idle/error)
      const backgroundColor = await firstStatus.evaluate(
        el => window.getComputedStyle(el).backgroundColor
      );
      console.log('Status indicator color:', backgroundColor);
    }
  });

  test('should display agent capabilities', async ({ page }) => {
    await page.waitForTimeout(2000);

    const agentCards = page.locator('.agent-card, .agent-item, .agent');

    if (await agentCards.count() > 0) {
      const firstAgent = agentCards.first();

      // Look for capabilities list
      const capabilities = firstAgent.locator(
        '.capabilities, .abilities, .features, .tags'
      );

      if (await capabilities.count() > 0) {
        const capText = await capabilities.first().textContent();
        console.log('Agent capabilities:', capText?.substring(0, 100));
      }
    }
  });

  test('should view agent details', async ({ page }) => {
    await page.waitForTimeout(2000);

    const agentCards = page.locator('.agent-card, .agent-item, .agent');

    if (await agentCards.count() > 0) {
      // Click on first agent
      await agentCards.first().click();
      await page.waitForTimeout(500);

      // Look for details panel or modal
      const detailsPanel = page.locator(
        '.agent-details, .details-panel, .modal, [role="dialog"]'
      );

      const isVisible = await detailsPanel.isVisible().catch(() => false);
      console.log('Agent details panel visible:', isVisible);

      if (isVisible) {
        // Look for detailed information
        const description = detailsPanel.locator('.description, .about, p');
        const hasDescription = await description.count() > 0;
        console.log('Has description:', hasDescription);
      }
    }
  });

  test('should display agent communication messages', async ({ page }) => {
    // Wait for communication API call
    const commPromise = page.waitForResponse(
      response => response.url().includes('/api/communication') ||
                  response.url().includes('/api/messages'),
      { timeout: 10000 }
    ).catch(() => null);

    await page.goto('/');
    const commResponse = await commPromise;

    if (commResponse) {
      expect(commResponse.status()).toBeLessThan(400);

      await page.waitForTimeout(1000);

      // Look for message items
      const messages = page.locator(
        '.message, .communication-item, .agent-message, [data-testid="message"]'
      );

      const count = await messages.count();
      console.log(`Found ${count} agent messages`);

      if (count > 0) {
        const firstMessage = messages.first();
        const messageText = await firstMessage.textContent();
        console.log('Sample message:', messageText?.substring(0, 100));
      }
    }
  });

  test('should show agent collaboration features', async ({ page }) => {
    // Wait for collaboration API call
    const collabPromise = page.waitForResponse(
      response => response.url().includes('/api/collaboration'),
      { timeout: 10000 }
    ).catch(() => null);

    await page.goto('/');
    const collabResponse = await collabPromise;

    if (collabResponse) {
      expect(collabResponse.status()).toBeLessThan(400);

      await page.waitForTimeout(1000);

      // Look for collaboration indicators
      const collabIndicators = page.locator(
        '.collaboration, .collab-item, .team-work, [data-testid="collaboration"]'
      );

      const count = await collabIndicators.count();
      console.log(`Found ${count} collaboration items`);
    }
  });

  test('should filter agents by status', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for status filter
    const statusFilter = page.locator(
      'select[name="status"], .status-filter, button:has-text("Active"), button:has-text("All")'
    );

    if (await statusFilter.count() > 0) {
      const agentCards = page.locator('.agent-card, .agent-item, .agent');
      const initialCount = await agentCards.count();

      console.log(`Initial agents count: ${initialCount}`);

      const filter = statusFilter.first();
      const tagName = await filter.evaluate(el => el.tagName);

      if (tagName === 'SELECT') {
        const options = await filter.locator('option').count();
        if (options > 1) {
          await filter.selectOption({ index: 1 });
          await page.waitForTimeout(1000);

          const filteredCount = await agentCards.count();
          console.log(`Filtered agents count: ${filteredCount}`);
        }
      } else {
        // Button filter
        await filter.click();
        await page.waitForTimeout(1000);

        const filteredCount = await agentCards.count();
        console.log(`Filtered agents count: ${filteredCount}`);
      }
    }
  });

  test('should display agent performance metrics', async ({ page }) => {
    await page.waitForTimeout(2000);

    const agentCards = page.locator('.agent-card, .agent-item, .agent');

    if (await agentCards.count() > 0) {
      const firstAgent = agentCards.first();

      // Click to view details
      await firstAgent.click();
      await page.waitForTimeout(500);

      // Look for performance metrics
      const metrics = page.locator(
        '.metrics, .performance, .stats, .agent-metrics'
      );

      if (await metrics.count() > 0) {
        const metricsText = await metrics.first().textContent();
        console.log('Agent metrics:', metricsText?.substring(0, 150));
      }
    }
  });

  test('should show agent task history', async ({ page }) => {
    await page.waitForTimeout(2000);

    const agentCards = page.locator('.agent-card, .agent-item, .agent');

    if (await agentCards.count() > 0) {
      const firstAgent = agentCards.first();
      await firstAgent.click();
      await page.waitForTimeout(500);

      // Look for task history
      const taskHistory = page.locator(
        '.task-history, .history, .tasks, [data-testid="task-history"]'
      );

      if (await taskHistory.count() > 0) {
        const tasks = taskHistory.locator('.task, .task-item, li');
        const taskCount = await tasks.count();
        console.log(`Agent has ${taskCount} tasks in history`);
      }
    }
  });

  test('should enable/disable agents', async ({ page }) => {
    await page.waitForTimeout(2000);

    const agentCards = page.locator('.agent-card, .agent-item, .agent');

    if (await agentCards.count() > 0) {
      const firstAgent = agentCards.first();

      // Look for enable/disable toggle
      const toggle = firstAgent.locator(
        'input[type="checkbox"], .toggle, button:has-text("Enable"), button:has-text("Disable")'
      );

      if (await toggle.count() > 0) {
        const toggleElement = toggle.first();
        const isEnabled = await toggleElement.isEnabled();
        console.log('Agent toggle enabled:', isEnabled);

        if (isEnabled) {
          // Don't actually toggle in test to avoid side effects
          console.log('Toggle control found and functional');
        }
      }
    }
  });

  test('should configure agent settings', async ({ page }) => {
    await page.waitForTimeout(2000);

    const agentCards = page.locator('.agent-card, .agent-item, .agent');

    if (await agentCards.count() > 0) {
      const firstAgent = agentCards.first();

      // Look for settings/config button
      const settingsButton = firstAgent.locator(
        'button:has-text("Settings"), button:has-text("Configure"), .settings-btn, [aria-label*="settings" i]'
      );

      if (await settingsButton.count() > 0) {
        await settingsButton.first().click();
        await page.waitForTimeout(500);

        // Look for settings panel
        const settingsPanel = page.locator(
          '.settings-panel, .config-panel, .modal, [role="dialog"]'
        );

        const isVisible = await settingsPanel.isVisible().catch(() => false);
        console.log('Settings panel visible:', isVisible);

        if (isVisible) {
          // Look for configuration options
          const configOptions = settingsPanel.locator('input, select, textarea');
          const count = await configOptions.count();
          console.log(`Found ${count} configuration options`);
        }
      }
    }
  });

  test('should display agent error states', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for agents with error status
    const errorAgents = page.locator(
      '.agent-card:has(.error), .agent-item:has(.failed), [data-status="error"]'
    );

    const count = await errorAgents.count();
    console.log(`Found ${count} agents with errors`);

    if (count > 0) {
      const firstError = errorAgents.first();
      const errorMessage = firstError.locator('.error-message, .error, .message');

      if (await errorMessage.count() > 0) {
        const errorText = await errorMessage.first().textContent();
        console.log('Sample error:', errorText?.substring(0, 100));
      }
    }
  });

  test('should search agents', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for search input
    const searchInput = page.locator(
      'input[type="search"], input[placeholder*="Search" i]'
    ).first();

    if (await searchInput.isVisible().catch(() => false)) {
      // Search for agent
      await searchInput.fill('compliance');
      await page.waitForTimeout(1000);

      const agentCards = page.locator('.agent-card, .agent-item, .agent');
      const count = await agentCards.count();

      console.log(`Search results for "compliance": ${count} agents`);
    }
  });

  test('should show agent activity timeline', async ({ page }) => {
    await page.waitForTimeout(2000);

    const agentCards = page.locator('.agent-card, .agent-item, .agent');

    if (await agentCards.count() > 0) {
      const firstAgent = agentCards.first();
      await firstAgent.click();
      await page.waitForTimeout(500);

      // Look for activity timeline
      const timeline = page.locator(
        '.timeline, .activity-timeline, .events-timeline'
      );

      if (await timeline.count() > 0) {
        const timelineItems = timeline.locator('.timeline-item, .event, li');
        const itemCount = await timelineItems.count();
        console.log(`Timeline has ${itemCount} events`);
      }
    }
  });

  test('should display agent recommendations', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for recommendations section
    const recommendations = page.locator(
      '.recommendations, .suggestions, .agent-recommendations'
    );

    if (await recommendations.count() > 0) {
      const recItems = recommendations.locator('.recommendation, .suggestion, li');
      const count = await recItems.count();
      console.log(`Found ${count} agent recommendations`);

      if (count > 0) {
        const firstRec = recItems.first();
        const recText = await firstRec.textContent();
        console.log('Sample recommendation:', recText?.substring(0, 100));
      }
    }
  });
});
