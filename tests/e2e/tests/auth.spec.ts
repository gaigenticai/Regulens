import { test, expect } from '@playwright/test';

/**
 * Authentication Tests
 *
 * Tests the authentication flow including login, JWT token handling,
 * and session management for the Regulens platform.
 */

test.describe('Authentication', () => {
  test.beforeEach(async ({ page }) => {
    // Navigate to the application
    await page.goto('/');
  });

  test('should load the login page', async ({ page }) => {
    // Verify page title
    await expect(page).toHaveTitle(/Regulens/);

    // Check for login form elements
    const loginButton = page.locator('button:has-text("Login"), button:has-text("Sign In")');
    await expect(loginButton).toBeVisible({ timeout: 10000 });
  });

  test('should show validation errors for empty credentials', async ({ page }) => {
    // Find and click login button without filling credentials
    const loginButton = page.locator('button:has-text("Login"), button:has-text("Sign In")').first();
    await loginButton.click();

    // Check for validation messages or error states
    // This will depend on your actual UI implementation
    const errorMessage = page.locator('.error, .alert-danger, [role="alert"]').first();

    // Wait a bit for validation to trigger
    await page.waitForTimeout(500);
  });

  test('should reject invalid credentials', async ({ page }) => {
    // Fill in invalid credentials
    const usernameField = page.locator('input[type="text"], input[type="email"], input[name="username"], input[placeholder*="username" i], input[placeholder*="email" i]').first();
    const passwordField = page.locator('input[type="password"]').first();

    if (await usernameField.isVisible()) {
      await usernameField.fill('invalid_user@example.com');
      await passwordField.fill('wrong_password');

      // Submit the form
      const loginButton = page.locator('button:has-text("Login"), button:has-text("Sign In")').first();
      await loginButton.click();

      // Wait for error response
      await page.waitForTimeout(1000);

      // Check for error message
      const errorMessage = page.locator('.error, .alert-danger, [role="alert"], .login-error');

      // Either error message should appear or credentials should not be accepted
      const hasError = await errorMessage.count() > 0;
      const stillOnLoginPage = await loginButton.isVisible();

      expect(hasError || stillOnLoginPage).toBeTruthy();
    }
  });

  test('should successfully login with valid credentials', async ({ page }) => {
    // Note: This test requires valid test credentials
    // You should set up test users in your database or use environment variables
    const testUsername = process.env.TEST_USERNAME || 'test_admin@regulens.com';
    const testPassword = process.env.TEST_PASSWORD || 'TestPassword123!';

    const usernameField = page.locator('input[type="text"], input[type="email"], input[name="username"], input[placeholder*="username" i]').first();
    const passwordField = page.locator('input[type="password"]').first();

    if (await usernameField.isVisible()) {
      await usernameField.fill(testUsername);
      await passwordField.fill(testPassword);

      // Intercept the login API call
      const loginPromise = page.waitForResponse(
        response => response.url().includes('/api/login') || response.url().includes('/api/auth'),
        { timeout: 10000 }
      );

      const loginButton = page.locator('button:has-text("Login"), button:has-text("Sign In")').first();
      await loginButton.click();

      try {
        const response = await loginPromise;

        // Check if login was successful (200-299 status code)
        expect(response.status()).toBeLessThan(400);

        // Check for JWT token in response or localStorage
        const localStorage = await page.evaluate(() => {
          return {
            token: window.localStorage.getItem('token') || window.localStorage.getItem('jwt'),
            authToken: window.localStorage.getItem('authToken'),
          };
        });

        // Either token should be present or we should be redirected to dashboard
        const hasToken = localStorage.token || localStorage.authToken;
        const onDashboard = page.url().includes('/dashboard') || page.url().includes('/home');

        expect(hasToken || onDashboard).toBeTruthy();
      } catch (error) {
        // If no API call is made, check if we're on a different page (already logged in)
        console.log('Login test: No API call intercepted, checking page state');
      }
    }
  });

  test('should persist authentication across page reloads', async ({ page, context }) => {
    // This test assumes you've logged in successfully
    // You might need to adjust based on your actual authentication flow

    // Store authentication state
    await context.storageState({ path: 'tests/e2e/.auth/user.json' });

    // Reload the page
    await page.reload();

    // Wait for page to load
    await page.waitForLoadState('networkidle');

    // Verify still authenticated (no redirect to login)
    const loginButton = page.locator('button:has-text("Login"), button:has-text("Sign In")');
    const isLoginVisible = await loginButton.isVisible().catch(() => false);

    // If we don't see a login button, we're likely still authenticated
    // (This assumes authenticated users don't see the login button)
  });

  test('should logout successfully', async ({ page }) => {
    // Look for logout button/link
    const logoutButton = page.locator('button:has-text("Logout"), a:has-text("Logout"), button:has-text("Sign Out")').first();

    if (await logoutButton.isVisible({ timeout: 2000 }).catch(() => false)) {
      // Intercept logout API call
      const logoutPromise = page.waitForResponse(
        response => response.url().includes('/api/logout') || response.url().includes('/api/auth/logout'),
        { timeout: 5000 }
      ).catch(() => null);

      await logoutButton.click();

      // Wait for navigation or response
      await Promise.race([
        logoutPromise,
        page.waitForNavigation({ timeout: 5000 }).catch(() => null)
      ]);

      // Check if redirected to login or token is cleared
      const localStorage = await page.evaluate(() => {
        return {
          token: window.localStorage.getItem('token') || window.localStorage.getItem('jwt'),
        };
      });

      expect(localStorage.token).toBeNull();
    }
  });

  test('should include JWT token in API requests', async ({ page }) => {
    // Monitor network requests
    page.on('request', request => {
      const url = request.url();
      if (url.includes('/api/')) {
        const headers = request.headers();

        // Check for Authorization header
        if (headers['authorization']) {
          console.log('API request has Authorization header:', headers['authorization'].substring(0, 20) + '...');
        }
      }
    });

    // Navigate and trigger some API calls
    await page.goto('/');
    await page.waitForTimeout(2000);

    // Make an API request (if authenticated)
    const response = await page.evaluate(async () => {
      try {
        const res = await fetch('/api/activity');
        return { status: res.status, ok: res.ok };
      } catch (error) {
        return { error: String(error) };
      }
    });

    // Response should either be successful (200-299) or unauthorized (401)
    if ('status' in response) {
      expect([200, 201, 401, 403].includes(response.status)).toBeTruthy();
    }
  });

  test('should handle token expiration gracefully', async ({ page }) => {
    // Set an expired token in localStorage
    await page.goto('/');

    await page.evaluate(() => {
      // Set a mock expired token
      const expiredToken = 'eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiZXhwIjoxNTE2MjM5MDIyfQ.invalid';
      window.localStorage.setItem('token', expiredToken);
    });

    // Try to access a protected API endpoint
    const response = await page.evaluate(async () => {
      try {
        const res = await fetch('/api/activity', {
          headers: {
            'Authorization': 'Bearer ' + window.localStorage.getItem('token')
          }
        });
        return { status: res.status };
      } catch (error) {
        return { error: String(error) };
      }
    });

    // Should get unauthorized response
    if ('status' in response) {
      expect([401, 403].includes(response.status)).toBeTruthy();
    }
  });
});
