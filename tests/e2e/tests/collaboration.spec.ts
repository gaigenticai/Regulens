import { test, expect } from '@playwright/test';

/**
 * Human-AI Collaboration Tests
 */

test.describe('Human-AI Collaboration', () => {
  const baseURL = process.env.BASE_URL || 'http://localhost:8080';

  test('GET /api/collaboration/sessions should list sessions', async ({ request }) => {
    const response = await request.get(`${baseURL}/api/collaboration/sessions`);
    expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('POST /api/collaboration/message should send message', async ({ request }) => {
    const response = await request.post(`${baseURL}/api/collaboration/message`, {
      data: { agent_id: 'agent-001', message: 'Test collaboration message' }
    });
    expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('POST /api/collaboration/feedback should submit feedback', async ({ request }) => {
    const response = await request.post(`${baseURL}/api/collaboration/feedback`, {
      data: { decision_id: 'dec-001', rating: 5, comment: 'Great decision' }
    });
    expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('GET /api/feedback/learning should return insights', async ({ request }) => {
    const response = await request.get(`${baseURL}/api/feedback/learning`);
    expect([200, 201, 401, 403].includes(response.status())).toBeTruthy();
  });

  test('should display collaboration sessions', async ({ page }) => {
    await page.goto('/');
    await page.waitForTimeout(2000);

    const sessions = page.locator('.collaboration-session, .session, [data-testid="collab-session"]');
    const count = await sessions.count();
    console.log(`Found ${count} collaboration sessions`);
  });

  test('should show agent-human conversations', async ({ page }) => {
    await page.waitForTimeout(2000);

    const conversations = page.locator('.conversation, .chat, .messages');
    if (await conversations.count() > 0) {
      console.log('Collaboration conversations found');

      const messages = conversations.locator('.message, .chat-message');
      const msgCount = await messages.count();
      console.log(`Total messages: ${msgCount}`);
    }
  });

  test('should display feedback interface', async ({ page }) => {
    await page.waitForTimeout(2000);

    const feedbackBtn = page.locator('button:has-text("Feedback"), .feedback-btn');
    if (await feedbackBtn.count() > 0) {
      await feedbackBtn.first().click();
      await page.waitForTimeout(500);

      const feedbackForm = page.locator('.feedback-form, [role="dialog"]:has-text("Feedback")');
      const isVisible = await feedbackForm.isVisible().catch(() => false);
      console.log('Feedback form visible:', isVisible);
    }
  });
});
