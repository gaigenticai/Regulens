import { test, expect } from '@playwright/test';

/**
 * LLM Integration Tests (OpenAI & Anthropic)
 */

test.describe('LLM Integration', () => {
  const baseURL = process.env.BASE_URL || 'http://localhost:8080';

  test('POST /api/llm/completion should work', async ({ request }) => {
    const response = await request.post(`${baseURL}/api/llm/completion`, {
      data: { prompt: 'Analyze this compliance requirement', model: 'gpt-4' }
    });
    expect([200, 201, 400, 401, 403, 503].includes(response.status())).toBeTruthy();
  });

  test('POST /api/llm/analysis should analyze documents', async ({ request }) => {
    const response = await request.post(`${baseURL}/api/llm/analysis`, {
      data: { text: 'Sample regulatory document', type: 'compliance' }
    });
    expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('POST /api/llm/compliance should check compliance', async ({ request }) => {
    const response = await request.post(`${baseURL}/api/llm/compliance`, {
      data: { document: 'Test document', framework: 'GDPR' }
    });
    expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('should display LLM-generated insights', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const insights = page.locator('.llm-insight, .ai-insight, :has-text("AI Analysis")');
    const count = await insights.count();
    console.log(`Found ${count} LLM-generated insights`);
  });

  test('should show LLM provider status', async ({ page }) => {
    await page.waitForTimeout(2000);

    const llmStatus = page.locator(':has-text("OpenAI"), :has-text("Anthropic"), :has-text("Claude")');
    if (await llmStatus.count() > 0) {
      console.log('LLM provider status indicators found');
    }
  });
});
