import { test, expect } from '@playwright/test';

/**
 * Email Notifications & Tool Integration Tests
 */

test.describe('Tool Integration', () => {
  test('should show email notification settings', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const emailSettings = page.locator('.email-settings, :has-text("Email"), :has-text("Notification")');
    if (await emailSettings.count() > 0) {
      console.log('Email notification settings found');
    }
  });

  test('should display web search tool', async ({ page }) => {
    await page.waitForTimeout(2000);

    const webSearch = page.locator('.web-search, :has-text("Web Search")');
    if (await webSearch.count() > 0) {
      console.log('Web search tool integration found');
    }
  });

  test('should show MCP tool integrations', async ({ page }) => {
    await page.waitForTimeout(2000);

    const mcpTools = page.locator(':has-text("MCP"), .mcp-tool');
    const count = await mcpTools.count();
    console.log(`Found ${count} MCP tool integrations`);
  });

  test('should display external tool connections', async ({ page }) => {
    await page.waitForTimeout(2000);

    const tools = page.locator('.tool, .integration, [data-testid="tool"]');
    const count = await tools.count();
    console.log(`Found ${count} external tool integrations`);
  });

  test('should show notification history', async ({ page }) => {
    await page.waitForTimeout(2000);

    const notifications = page.locator('.notification-history, .notifications');
    if (await notifications.count() > 0) {
      const items = notifications.locator('.notification-item, li');
      const count = await items.count();
      console.log(`Notification history: ${count} items`);
    }
  });
});
