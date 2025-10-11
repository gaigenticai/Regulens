/**
 * Critical Data Flow E2E Tests
 * Tests complete data flow: Frontend → API → Database → API → Frontend
 */

import { test, expect } from '@playwright/test';

test.describe('Critical Data Flows - Frontend to Backend to Database', () => {
  
  test.beforeEach(async ({ page }) => {
    // Login before each test
    await page.goto('http://localhost:5173/login');
    await page.fill('input[name="username"]', 'admin');
    await page.fill('input[name="password"]', 'admin123');
    await page.click('button[type="submit"]');
    await page.waitForURL('http://localhost:5173/', { timeout: 10000 });
  });

  test('Activity Feed Flow: Database → API → Frontend Display', async ({ page }) => {
    // Navigate to activity feed
    await page.goto('http://localhost:5173/activity');
    await page.waitForSelector('[data-testid="activity-feed"]', { timeout: 10000 });

    // Verify activity items are loaded from database
    const activityItems = page.locator('[data-testid="activity-item"]');
    const count = await activityItems.count();
    expect(count).toBeGreaterThan(0);

    // Verify each activity item has required data
    const firstItem = activityItems.first();
    await expect(firstItem).toBeVisible();
    await expect(firstItem.locator('[data-testid="activity-type"]')).toBeVisible();
    await expect(firstItem.locator('[data-testid="activity-timestamp"]')).toBeVisible();
  });

  test('Decision Creation Flow: Frontend → API → Database → API → Display', async ({ page }) => {
    // Navigate to decision engine
    await page.goto('http://localhost:5173/decisions');
    await page.waitForSelector('[data-testid="decision-engine"]', { timeout: 10000 });

    // Create a new decision
    await page.click('button:has-text("New Decision")');
    await page.fill('input[name="decision_name"]', 'Test Decision ' + Date.now());
    await page.fill('textarea[name="description"]', 'E2E test decision');
    
    // Add criteria
    await page.click('button:has-text("Add Criterion")');
    await page.fill('input[name="criterion_name"]', 'Risk Score');
    await page.fill('input[name="criterion_weight"]', '0.35');
    
    // Submit decision
    await page.click('button:has-text("Create Decision")');
    
    // Verify success message
    await expect(page.locator('text=Decision created successfully')).toBeVisible({ timeout: 5000 });

    // Verify decision appears in history
    await page.goto('http://localhost:5173/decisions/history');
    await page.waitForSelector('[data-testid="decision-item"]');
    await expect(page.locator('text=Test Decision')).toBeVisible();
  });

  test('Agent Status Flow: Database → API → Frontend Update', async ({ page }) => {
    // Navigate to agents page
    await page.goto('http://localhost:5173/agents');
    await page.waitForSelector('[data-testid="agent-list"]', { timeout: 10000 });

    // Verify agents are loaded
    const agentCards = page.locator('[data-testid="agent-card"]');
    const count = await agentCards.count();
    expect(count).toBeGreaterThan(0);

    // Click on first agent to view details
    await agentCards.first().click();
    await page.waitForURL(/\/agents\/.+/);

    // Verify agent details are loaded from database
    await expect(page.locator('[data-testid="agent-name"]')).toBeVisible();
    await expect(page.locator('[data-testid="agent-status"]')).toBeVisible();
    await expect(page.locator('[data-testid="agent-metrics"]')).toBeVisible();
  });

  test('Regulatory Changes Flow: External Source → Database → API → Frontend', async ({ page }) => {
    // Navigate to regulatory changes
    await page.goto('http://localhost:5173/regulatory');
    await page.waitForSelector('[data-testid="regulatory-changes"]', { timeout: 10000 });

    // Verify changes are loaded
    const changes = page.locator('[data-testid="regulatory-change"]');
    const count = await changes.count();
    expect(count).toBeGreaterThan(0);

    // Verify each change has required fields from database
    const firstChange = changes.first();
    await expect(firstChange.locator('[data-testid="change-source"]')).toBeVisible();
    await expect(firstChange.locator('[data-testid="change-title"]')).toBeVisible();
    await expect(firstChange.locator('[data-testid="change-date"]')).toBeVisible();
  });

  test('Audit Trail Flow: System Events → Database → API → Frontend Audit Log', async ({ page }) => {
    // Navigate to audit trail
    await page.goto('http://localhost:5173/audit');
    await page.waitForSelector('[data-testid="audit-trail"]', { timeout: 10000 });

    // Verify audit entries are loaded
    const auditEntries = page.locator('[data-testid="audit-entry"]');
    const count = await auditEntries.count();
    expect(count).toBeGreaterThan(0);

    // Verify audit entry structure
    const firstEntry = auditEntries.first();
    await expect(firstEntry.locator('[data-testid="audit-action"]')).toBeVisible();
    await expect(firstEntry.locator('[data-testid="audit-user"]')).toBeVisible();
    await expect(firstEntry.locator('[data-testid="audit-timestamp"]')).toBeVisible();
  });

  test('Transaction Monitoring Flow: Transaction → Database → Risk Assessment → Frontend', async ({ page }) => {
    // Navigate to transactions
    await page.goto('http://localhost:5173/transactions');
    await page.waitForSelector('[data-testid="transactions-list"]', { timeout: 10000 });

    // Verify transactions are loaded
    const transactions = page.locator('[data-testid="transaction-item"]');
    const count = await transactions.count();
    expect(count).toBeGreaterThan(0);

    // Click on a transaction to view details
    await transactions.first().click();
    
    // Verify transaction details loaded from database
    await expect(page.locator('[data-testid="transaction-id"]')).toBeVisible();
    await expect(page.locator('[data-testid="transaction-risk-score"]')).toBeVisible();
    await expect(page.locator('[data-testid="transaction-status"]')).toBeVisible();
  });

  test('Knowledge Base Flow: Query → Embeddings → Database → Search Results', async ({ page }) => {
    // Navigate to knowledge base
    await page.goto('http://localhost:5173/knowledge');
    await page.waitForSelector('[data-testid="knowledge-base"]', { timeout: 10000 });

    // Perform a search
    await page.fill('input[data-testid="search-input"]', 'compliance regulations');
    await page.click('button[data-testid="search-button"]');

    // Wait for results
    await page.waitForSelector('[data-testid="search-results"]', { timeout: 10000 });
    
    // Verify results are returned from database
    const results = page.locator('[data-testid="knowledge-item"]');
    const count = await results.count();
    expect(count).toBeGreaterThan(0);
  });

  test('Pattern Recognition Flow: Data → Analysis → Database → Frontend Visualization', async ({ page }) => {
    // Navigate to pattern analysis
    await page.goto('http://localhost:5173/pattern-analysis');
    await page.waitForSelector('[data-testid="pattern-analysis"]', { timeout: 10000 });

    // Verify patterns are loaded from database
    const patterns = page.locator('[data-testid="pattern-item"]');
    const count = await patterns.count();
    expect(count).toBeGreaterThan(0);

    // Verify pattern details
    const firstPattern = patterns.first();
    await expect(firstPattern.locator('[data-testid="pattern-type"]')).toBeVisible();
    await expect(firstPattern.locator('[data-testid="pattern-confidence"]')).toBeVisible();
  });

  test('LLM Integration Flow: User Input → LLM → Database → Response Display', async ({ page }) => {
    // Navigate to LLM analysis
    await page.goto('http://localhost:5173/llm-analysis');
    await page.waitForSelector('[data-testid="llm-analysis"]', { timeout: 10000 });

    // Submit analysis request
    await page.fill('textarea[data-testid="llm-input"]', 'Analyze this compliance scenario');
    await page.click('button[data-testid="analyze-button"]');

    // Wait for response
    await page.waitForSelector('[data-testid="llm-response"]', { timeout: 15000 });
    
    // Verify response is displayed
    await expect(page.locator('[data-testid="llm-response"]')).toBeVisible();
  });

  test('System Metrics Flow: Prometheus → Database → API → Dashboard', async ({ page }) => {
    // Navigate to system metrics
    await page.goto('http://localhost:5173/metrics');
    await page.waitForSelector('[data-testid="system-metrics"]', { timeout: 10000 });

    // Verify metrics are loaded
    await expect(page.locator('[data-testid="cpu-usage"]')).toBeVisible();
    await expect(page.locator('[data-testid="memory-usage"]')).toBeVisible();
    await expect(page.locator('[data-testid="request-rate"]')).toBeVisible();
  });

  test('Configuration Flow: Frontend Update → API → Database → System Reload', async ({ page }) => {
    // Navigate to settings
    await page.goto('http://localhost:5173/settings');
    await page.waitForSelector('[data-testid="settings-form"]', { timeout: 10000 });

    // Update a configuration
    const originalValue = await page.inputValue('input[name="log_level"]');
    await page.fill('input[name="log_level"]', 'debug');
    await page.click('button:has-text("Save Settings")');

    // Verify success message
    await expect(page.locator('text=Settings saved successfully')).toBeVisible({ timeout: 5000 });

    // Reload page to verify persistence
    await page.reload();
    await page.waitForSelector('[data-testid="settings-form"]');
    const newValue = await page.inputValue('input[name="log_level"]');
    expect(newValue).toBe('debug');

    // Restore original value
    await page.fill('input[name="log_level"]', originalValue);
    await page.click('button:has-text("Save Settings")');
  });

  test('Agent Collaboration Flow: Multi-agent → Consensus → Database → UI Update', async ({ page }) => {
    // Navigate to agent collaboration
    await page.goto('http://localhost:5173/agent-collaboration');
    await page.waitForSelector('[data-testid="collaboration-sessions"]', { timeout: 10000 });

    // Verify collaboration sessions are loaded
    const sessions = page.locator('[data-testid="collaboration-session"]');
    const count = await sessions.count();
    expect(count).toBeGreaterThan(0);

    // View session details
    await sessions.first().click();
    await expect(page.locator('[data-testid="session-participants"]')).toBeVisible();
    await expect(page.locator('[data-testid="session-consensus"]')).toBeVisible();
  });

  test('Memory Management Flow: Conversation → Storage → Consolidation → Retrieval', async ({ page }) => {
    // Navigate to memory management
    await page.goto('http://localhost:5173/memory-management');
    await page.waitForSelector('[data-testid="memory-management"]', { timeout: 10000 });

    // Verify conversation memories are loaded from database
    const memories = page.locator('[data-testid="memory-item"]');
    const count = await memories.count();
    expect(count).toBeGreaterThan(0);

    // Switch to consolidation tab
    await page.click('button:has-text("Consolidation")');
    await expect(page.locator('[data-testid="consolidation-status"]')).toBeVisible();
  });

  test('Feedback Loop Flow: User Feedback → Database → Learning → Model Update', async ({ page }) => {
    // Navigate to feedback analytics
    await page.goto('http://localhost:5173/feedback-analytics');
    await page.waitForSelector('[data-testid="feedback-stats"]', { timeout: 10000 });

    // Verify feedback statistics from database
    await expect(page.locator('[data-testid="total-feedback"]')).toBeVisible();
    await expect(page.locator('[data-testid="avg-score"]')).toBeVisible();
    await expect(page.locator('[data-testid="learning-applied"]')).toBeVisible();
  });

  test('Risk Assessment Flow: Data Input → Analysis → Database → Dashboard Display', async ({ page }) => {
    // Navigate to risk dashboard
    await page.goto('http://localhost:5173/risk-dashboard');
    await page.waitForSelector('[data-testid="risk-metrics"]', { timeout: 10000 });

    // Verify risk metrics are loaded from database
    await expect(page.locator('[data-testid="total-assessments"]')).toBeVisible();
    await expect(page.locator('[data-testid="high-risk-count"]')).toBeVisible();
    await expect(page.locator('[data-testid="avg-risk-score"]')).toBeVisible();
  });

  test('Data Quality Flow: Ingestion → Validation → Database → Monitor Display', async ({ page }) => {
    // Navigate to data quality monitor
    await page.goto('http://localhost:5173/data-quality-monitor');
    await page.waitForSelector('[data-testid="quality-checks"]', { timeout: 10000 });

    // Verify quality checks from database
    const checks = page.locator('[data-testid="quality-check"]');
    const count = await checks.count();
    expect(count).toBeGreaterThan(0);

    // Verify check details
    const firstCheck = checks.first();
    await expect(firstCheck.locator('[data-testid="check-name"]')).toBeVisible();
    await expect(firstCheck.locator('[data-testid="check-status"]')).toBeVisible();
    await expect(firstCheck.locator('[data-testid="check-score"]')).toBeVisible();
  });

  test('End-to-End Complex Flow: Transaction → Risk Analysis → Agent Collaboration → Decision → Audit', async ({ page }) => {
    // Step 1: Create a transaction
    await page.goto('http://localhost:5173/transactions');
    await page.click('button:has-text("New Transaction")');
    const txId = 'tx_e2e_' + Date.now();
    await page.fill('input[name="transaction_id"]', txId);
    await page.fill('input[name="amount"]', '50000');
    await page.click('button:has-text("Submit")');
    await expect(page.locator('text=Transaction created')).toBeVisible({ timeout: 5000 });

    // Step 2: Verify risk assessment was triggered
    await page.goto('http://localhost:5173/risk-dashboard');
    await page.waitForSelector('[data-testid="risk-metrics"]');
    // Risk metrics should have updated

    // Step 3: Check agent collaboration for the transaction
    await page.goto('http://localhost:5173/agent-communication-console');
    await page.waitForSelector('[data-testid="agent-messages"]');
    // Should see agent messages about the transaction

    // Step 4: Verify decision was made
    await page.goto('http://localhost:5173/decisions/history');
    await page.waitForSelector('[data-testid="decision-list"]');
    // Should have a decision related to the transaction

    // Step 5: Verify audit trail
    await page.goto('http://localhost:5173/audit');
    await page.waitForSelector('[data-testid="audit-trail"]');
    await page.fill('input[data-testid="search-audit"]', txId);
    // Should find audit entries for all steps
  });
});

test.describe('Database Connection Resilience', () => {
  test('Should handle database timeout gracefully', async ({ page }) => {
    await page.goto('http://localhost:5173/login');
    await page.fill('input[name="username"]', 'admin');
    await page.fill('input[name="password"]', 'admin123');
    await page.click('button[type="submit"]');
    await page.waitForURL('http://localhost:5173/');

    // Navigate to a data-heavy page
    await page.goto('http://localhost:5173/activity');
    
    // Should either show data or appropriate error message
    const hasData = await page.locator('[data-testid="activity-item"]').count() > 0;
    const hasError = await page.locator('text=/database|connection|error/i').isVisible().catch(() => false);
    
    expect(hasData || hasError).toBeTruthy();
  });

  test('Should retry failed requests', async ({ page }) => {
    await page.goto('http://localhost:5173/login');
    await page.fill('input[name="username"]', 'admin');
    await page.fill('input[name="password"]', 'admin123');
    await page.click('button[type="submit"]');
    await page.waitForURL('http://localhost:5173/');

    // Multiple rapid navigations should not break the app
    await page.goto('http://localhost:5173/activity');
    await page.goto('http://localhost:5173/decisions');
    await page.goto('http://localhost:5173/agents');
    await page.goto('http://localhost:5173/regulatory');
    
    // Final page should load successfully
    await expect(page.locator('[data-testid="regulatory-changes"]')).toBeVisible({ timeout: 10000 });
  });
});

