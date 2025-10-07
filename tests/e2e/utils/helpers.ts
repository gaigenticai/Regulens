import { Page, expect } from '@playwright/test';

/**
 * Test Helper Functions
 *
 * Reusable utilities for Playwright tests
 */

/**
 * Wait for an element to be visible and return it
 */
export async function waitForElement(page: Page, selector: string, timeout = 10000) {
  try {
    await page.waitForSelector(selector, { state: 'visible', timeout });
    return page.locator(selector).first();
  } catch (error) {
    console.log(`Element not found: ${selector}`);
    return null;
  }
}

/**
 * Check if API endpoint is responding
 */
export async function checkAPIHealth(page: Page, endpoint: string) {
  const response = await page.evaluate(async (url) => {
    try {
      const res = await fetch(url);
      return { status: res.status, ok: res.ok };
    } catch (error) {
      return { error: String(error) };
    }
  }, endpoint);

  return response;
}

/**
 * Login helper - attempts to log in with provided credentials
 */
export async function login(page: Page, username: string, password: string) {
  // Navigate to home/login page
  await page.goto('/');
  await page.waitForLoadState('networkidle');

  // Find username/email field
  const usernameField = page.locator(
    'input[type="text"], input[type="email"], input[name="username"], input[placeholder*="username" i]'
  ).first();

  const passwordField = page.locator('input[type="password"]').first();

  if (await usernameField.isVisible({ timeout: 5000 }).catch(() => false)) {
    await usernameField.fill(username);
    await passwordField.fill(password);

    // Submit login
    const loginButton = page.locator('button:has-text("Login"), button:has-text("Sign In")').first();
    await loginButton.click();

    // Wait for navigation or response
    await page.waitForTimeout(2000);

    return true;
  }

  return false;
}

/**
 * Logout helper
 */
export async function logout(page: Page) {
  const logoutButton = page.locator(
    'button:has-text("Logout"), a:has-text("Logout"), button:has-text("Sign Out")'
  ).first();

  if (await logoutButton.isVisible({ timeout: 2000 }).catch(() => false)) {
    await logoutButton.click();
    await page.waitForTimeout(1000);
    return true;
  }

  return false;
}

/**
 * Get authentication token from localStorage
 */
export async function getAuthToken(page: Page): Promise<string | null> {
  return await page.evaluate(() => {
    return window.localStorage.getItem('token') ||
           window.localStorage.getItem('jwt') ||
           window.localStorage.getItem('authToken');
  });
}

/**
 * Set authentication token in localStorage
 */
export async function setAuthToken(page: Page, token: string) {
  await page.evaluate((t) => {
    window.localStorage.setItem('token', t);
  }, token);
}

/**
 * Clear all localStorage items
 */
export async function clearStorage(page: Page) {
  await page.evaluate(() => {
    window.localStorage.clear();
    window.sessionStorage.clear();
  });
}

/**
 * Wait for API response and return data
 */
export async function waitForAPIResponse(page: Page, urlPattern: string, timeout = 10000) {
  try {
    const response = await page.waitForResponse(
      (response) => response.url().includes(urlPattern),
      { timeout }
    );

    if (response.ok()) {
      return await response.json();
    }

    return null;
  } catch (error) {
    console.log(`API response not received: ${urlPattern}`);
    return null;
  }
}

/**
 * Take a screenshot with a descriptive name
 */
export async function takeScreenshot(page: Page, name: string) {
  const timestamp = new Date().toISOString().replace(/[:.]/g, '-');
  const filename = `screenshot-${name}-${timestamp}.png`;
  await page.screenshot({ path: `tests/e2e/screenshots/${filename}`, fullPage: true });
  console.log(`Screenshot saved: ${filename}`);
}

/**
 * Scroll to bottom of page to trigger lazy loading
 */
export async function scrollToBottom(page: Page) {
  await page.evaluate(() => {
    window.scrollTo(0, document.body.scrollHeight);
  });
  await page.waitForTimeout(500);
}

/**
 * Get all text content from matching elements
 */
export async function getAllTextContent(page: Page, selector: string): Promise<string[]> {
  const elements = page.locator(selector);
  const count = await elements.count();
  const texts: string[] = [];

  for (let i = 0; i < count; i++) {
    const text = await elements.nth(i).textContent();
    if (text) {
      texts.push(text.trim());
    }
  }

  return texts;
}

/**
 * Check if element exists (without throwing error)
 */
export async function elementExists(page: Page, selector: string): Promise<boolean> {
  return await page.locator(selector).count() > 0;
}

/**
 * Wait for page to be fully loaded (no network activity)
 */
export async function waitForPageLoad(page: Page) {
  await page.waitForLoadState('networkidle');
  await page.waitForLoadState('domcontentloaded');
}

/**
 * Fill form with multiple fields
 */
export async function fillForm(page: Page, formData: Record<string, string>) {
  for (const [selector, value] of Object.entries(formData)) {
    const field = page.locator(selector);
    if (await field.isVisible({ timeout: 2000 }).catch(() => false)) {
      await field.fill(value);
    }
  }
}

/**
 * Get current page URL
 */
export async function getCurrentURL(page: Page): Promise<string> {
  return page.url();
}

/**
 * Navigate and wait for load
 */
export async function navigateTo(page: Page, path: string) {
  await page.goto(path);
  await waitForPageLoad(page);
}

/**
 * Retry an action multiple times
 */
export async function retry<T>(
  action: () => Promise<T>,
  maxAttempts = 3,
  delayMs = 1000
): Promise<T | null> {
  for (let attempt = 1; attempt <= maxAttempts; attempt++) {
    try {
      return await action();
    } catch (error) {
      if (attempt === maxAttempts) {
        console.log(`Action failed after ${maxAttempts} attempts:`, error);
        return null;
      }
      await new Promise(resolve => setTimeout(resolve, delayMs));
    }
  }
  return null;
}

/**
 * Check if running in CI environment
 */
export function isCI(): boolean {
  return !!process.env.CI;
}

/**
 * Get environment variable with fallback
 */
export function getEnv(key: string, fallback: string = ''): string {
  return process.env[key] || fallback;
}

/**
 * Format date for input fields (YYYY-MM-DD)
 */
export function formatDateForInput(date: Date): string {
  return date.toISOString().split('T')[0];
}

/**
 * Generate random test data
 */
export function generateTestData() {
  const timestamp = Date.now();
  return {
    email: `test-${timestamp}@regulens.test`,
    username: `testuser_${timestamp}`,
    password: 'TestPassword123!',
    timestamp,
  };
}

/**
 * Intercept network requests matching pattern
 */
export async function interceptRequests(
  page: Page,
  urlPattern: string,
  callback: (url: string, method: string) => void
) {
  page.on('request', (request) => {
    if (request.url().includes(urlPattern)) {
      callback(request.url(), request.method());
    }
  });
}

/**
 * Mock API response
 */
export async function mockAPIResponse(
  page: Page,
  urlPattern: string,
  mockData: any,
  status = 200
) {
  await page.route(`**${urlPattern}**`, (route) => {
    route.fulfill({
      status,
      contentType: 'application/json',
      body: JSON.stringify(mockData),
    });
  });
}
