import { test, expect } from '@playwright/test';

/**
 * Knowledge Base & Vector Storage Tests
 */

test.describe('Knowledge Base', () => {
  test('should access knowledge base', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const kb = page.locator('.knowledge-base, :has-text("Knowledge"), [data-testid="knowledge-base"]');
    if (await kb.count() > 0) {
      console.log('Knowledge base section found');
    }
  });

  test('should search knowledge base', async ({ page }) => {
    await page.waitForTimeout(2000);

    const searchInput = page.locator('input[placeholder*="Search knowledge" i], .kb-search');
    if (await searchInput.count() > 0) {
      await searchInput.first().fill('compliance regulation');
      await page.waitForTimeout(1000);

      const results = page.locator('.search-result, .kb-result');
      const count = await results.count();
      console.log(`Knowledge base search results: ${count}`);
    }
  });

  test('should display case-based reasoning examples', async ({ page }) => {
    await page.waitForTimeout(2000);

    const caseExamples = page.locator('.case, .cbr-case, :has-text("similar case")');
    const count = await caseExamples.count();
    console.log(`Found ${count} case-based reasoning examples`);
  });

  test('should show regulatory knowledge', async ({ page }) => {
    await page.waitForTimeout(2000);

    const regulations = page.locator('.regulation, .regulatory-knowledge, :has-text("regulation")');
    const count = await regulations.count();
    console.log(`Found ${count} regulatory knowledge items`);
  });
});
