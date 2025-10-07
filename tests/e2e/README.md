# Regulens E2E Testing with Playwright

Comprehensive browser-based testing suite for the Regulens AI Compliance Platform that simulates real human interactions.

## 📋 Overview

This test suite provides **COMPREHENSIVE** end-to-end testing for ALL Regulens features:

### Core Features
- **Authentication & Authorization** - Login, JWT, PBKDF2 password hashing, session management
- **Dashboard & Activity Feed** - Real-time data display, filtering, search
- **Regulatory Changes** - Change detection, ECB/SEC/FCA sources, RSS/web scraping
- **Audit Trail** - Compliance logging, event tracking, GDPR/CCPA validation
- **Agent Management** - AI agent status, communication, collaboration
- **API Integration** - All 60+ REST endpoints validation

### Advanced AI Features
- **MCDA Decision Engine** - TOPSIS, ELECTRE III, PROMETHEE II, AHP, VIKOR algorithms
- **Transaction Guardian** - Real-time fraud detection, risk assessment
- **Pattern Recognition** - Anomaly detection, time-series analysis, learning
- **LLM Integration** - OpenAI & Anthropic API, compliance analysis
- **Human-AI Collaboration** - Interactive sessions, feedback, learning

### Enterprise Features
- **Data Ingestion** - REST API sources, web scraping, database monitoring
- **Circuit Breakers** - Resilience patterns, retry mechanisms, service degradation
- **Knowledge Base** - Vector storage, case-based reasoning, regulatory knowledge
- **Visualizations** - D3.js decision trees, agent graphs, interactive charts
- **Tool Integration** - Email notifications, web search, MCP tools
- **Monitoring** - Prometheus metrics, health checks, performance tracking
- **Security** - SQL injection prevention, XSS protection, rate limiting

## 🚀 Quick Start

### Prerequisites

- Node.js 18+ and npm
- Docker and Docker Compose (for running Regulens)
- Regulens application built and running on `localhost:8080`

### Installation

```bash
# Install dependencies
npm install

# Install Playwright browsers
npx playwright install
```

### Running Tests

```bash
# Run all tests (headless mode)
npm test

# Run tests with browser visible
npm run test:headed

# Run tests in interactive UI mode
npm run test:ui

# Run tests in debug mode
npm run test:debug

# Run tests in specific browser
npm run test:chromium
npm run test:firefox
npm run test:webkit

# View test report
npm run test:report
```

## 📁 Project Structure

```
tests/e2e/
├── tests/                                  # Test specifications (18 files)
│   ├── auth.spec.ts                       # Authentication & JWT
│   ├── dashboard.spec.ts                  # Dashboard & activity feed
│   ├── regulatory-changes.spec.ts         # Regulatory monitoring
│   ├── audit-trail.spec.ts                # Audit logging & compliance
│   ├── agents.spec.ts                     # Agent management
│   ├── api-integration.spec.ts            # All 60+ API endpoints
│   ├── mcda-decision-engine.spec.ts       # TOPSIS, ELECTRE, PROMETHEE, AHP
│   ├── transaction-guardian.spec.ts       # Fraud detection
│   ├── data-ingestion.spec.ts             # Data sources & pipelines
│   ├── pattern-recognition.spec.ts        # ML patterns & anomalies
│   ├── llm-integration.spec.ts            # OpenAI & Anthropic
│   ├── collaboration.spec.ts              # Human-AI collaboration
│   ├── resilience.spec.ts                 # Circuit breakers & retry
│   ├── knowledge-base.spec.ts             # Vector storage & CBR
│   ├── visualization.spec.ts              # D3.js decision trees
│   ├── tool-integration.spec.ts           # Email, web search, MCP
│   ├── monitoring.spec.ts                 # Prometheus & health checks
│   └── security.spec.ts                   # Security & encryption
├── fixtures/                               # Test data and mocks
│   └── test-data.ts                       # Sample data, users, API endpoints
├── utils/                                  # Helper functions
│   └── helpers.ts                         # 20+ reusable utilities
├── reports/                                # Test execution reports
│   ├── html/                              # HTML report
│   ├── results.json                       # JSON results
│   └── junit.xml                          # JUnit format
└── README.md                               # This file
```

## 🧪 Test Suites

### Authentication Tests (`auth.spec.ts`)

Tests login functionality, JWT token handling, and session management:

- Login page loading
- Validation errors
- Invalid credentials rejection
- Successful login with valid credentials
- Token persistence across reloads
- Logout functionality
- JWT token in API requests
- Token expiration handling

### Dashboard Tests (`dashboard.spec.ts`)

Tests the main dashboard, activity feed, and real-time updates:

- Dashboard loading with all sections
- Activity feed display and updates
- Activity filtering and search
- Decision history cards
- Real-time metrics display
- Navigation between sections
- Pagination
- Manual refresh

### Regulatory Changes Tests (`regulatory-changes.spec.ts`)

Tests regulatory change detection and monitoring:

- Regulatory changes feed loading
- Source information display (ECB, RSS, etc.)
- Filtering by source type
- Change metadata display
- Change details expansion
- Search functionality
- Impact analysis indicators
- Date range filtering
- Source health status
- Notifications
- Export functionality
- External source links

### Audit Trail Tests (`audit-trail.spec.ts`)

Tests compliance audit logging and event tracking:

- Audit events loading
- Chronological event display
- Event type indicators
- Filtering by type and date
- Event details display
- Event expansion for full details
- Search functionality
- Compliance violation events
- Audit log export
- Pagination
- User activity summary
- Filtering by user/actor
- Event correlation
- Severity levels
- Compliance framework events

### Agent Management Tests (`agents.spec.ts`)

Tests AI agent management and collaboration:

- Agents list loading
- Agent cards display
- Status indicators
- Agent capabilities display
- Agent details view
- Communication messages
- Collaboration features
- Filtering by status
- Performance metrics
- Task history
- Enable/disable agents
- Agent settings/configuration
- Error states
- Search functionality
- Activity timeline
- Recommendations

### API Integration Tests (`api-integration.spec.ts`)

Direct API endpoint testing:

- `/api/activity` - Activity feed
- `/api/decisions` - Decision history
- `/api/regulatory-changes` - Regulatory updates
- `/api/audit-trail` - Audit events
- `/api/communication` - Agent messages
- `/api/agents` - Agent management
- `/api/collaboration` - Collaboration data
- `/api/patterns` - Pattern recognition
- `/api/feedback` - Feedback system
- `/api/errors` - Error dashboard
- `/api/health` - Health checks
- `/api/llm` - LLM integration
- `/api/config` - Configuration
- `/api/db` - Database operations
- `/metrics` - Prometheus metrics
- CORS headers validation
- Error handling
- Performance testing

## 🛠️ Configuration

### Environment Variables

Create a `.env` file in the project root:

```env
BASE_URL=http://localhost:8080
TEST_USERNAME=admin@regulens.com
TEST_PASSWORD=YourSecurePassword
CI=false
```

### Playwright Configuration

Main configuration in `playwright.config.ts`:

- **Browsers**: Chromium, Firefox, WebKit, Mobile Chrome, Mobile Safari
- **Base URL**: `http://localhost:8080`
- **Timeout**: 30 seconds per test
- **Retries**: 2 on CI, 0 locally
- **Screenshots**: On failure
- **Videos**: On first retry
- **Trace**: On first retry

## 📊 Test Reports

After running tests, view the HTML report:

```bash
npm run test:report
```

Reports include:
- Test execution summary
- Pass/fail rates per browser
- Screenshots of failures
- Video recordings
- Execution traces
- Network logs

## 🔧 Writing New Tests

### Example Test

```typescript
import { test, expect } from '@playwright/test';
import { login, waitForAPIResponse } from '../utils/helpers';

test.describe('My Feature', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await login(page, 'admin@regulens.com', 'password');
  });

  test('should display my feature', async ({ page }) => {
    // Wait for API response
    const data = await waitForAPIResponse(page, '/api/my-feature');
    expect(data).toBeTruthy();

    // Check UI element
    const element = page.locator('.my-feature');
    await expect(element).toBeVisible();
  });
});
```

### Using Helpers

```typescript
import {
  login,
  logout,
  waitForAPIResponse,
  getAuthToken,
  takeScreenshot,
  scrollToBottom,
  mockAPIResponse
} from '../utils/helpers';

// Login
await login(page, username, password);

// Get current auth token
const token = await getAuthToken(page);

// Wait for API call
const data = await waitForAPIResponse(page, '/api/endpoint');

// Mock API for testing
await mockAPIResponse(page, '/api/test', { status: 'ok' });

// Take debug screenshot
await takeScreenshot(page, 'debug-view');
```

### Using Test Data

```typescript
import {
  mockRegulatoryChanges,
  mockAgents,
  testUsers,
  apiEndpoints
} from '../fixtures/test-data';

// Use test user
const { username, password } = testUsers.admin;

// Use mock data for assertions
expect(data).toMatchObject(mockRegulatoryChanges[0]);
```

## 🐛 Debugging Tests

### Interactive UI Mode

```bash
npm run test:ui
```

This opens Playwright's interactive test runner where you can:
- Run individual tests
- See test execution step-by-step
- Inspect page state at each step
- View network requests
- Debug with browser DevTools

### Debug Mode

```bash
npm run test:debug
```

Opens tests in headed mode with Playwright Inspector for step-by-step debugging.

### Record New Tests

```bash
npm run test:codegen
```

Opens a browser where your actions are recorded as Playwright test code.

## 🎯 Best Practices

1. **Use Specific Selectors**: Prefer `data-testid` attributes over CSS classes
2. **Wait for Elements**: Use Playwright's auto-waiting or explicit `waitForSelector`
3. **Mock External APIs**: When testing UI, mock external dependencies
4. **Test Real API Calls**: For integration tests, use real backend
5. **Keep Tests Independent**: Each test should set up its own state
6. **Use Fixtures**: Share common test data via fixtures
7. **Handle Flakiness**: Use retries and proper waits, not arbitrary timeouts
8. **Clean Up**: Reset state between tests

## 🔄 CI/CD Integration

Tests run automatically on:
- Push to `main` or `develop` branches
- Pull requests
- Manual workflow dispatch

GitHub Actions workflow includes:
- PostgreSQL and Redis services
- Building Regulens application
- Running all tests across browsers
- Uploading test reports and screenshots
- Publishing results summary

## 📈 Performance

Current test suite metrics:
- **Total Test Files**: 18 comprehensive test suites
- **Total Tests**: **200+ individual test cases**
- **Execution Time**: ~15-20 minutes (all browsers, parallel)
- **Coverage**:
  - **60+ API endpoints** tested
  - **20+ UI flows** validated
  - **5 MCDA algorithms** tested (TOPSIS, ELECTRE III, PROMETHEE II, AHP, VIKOR)
  - **Security**, **fraud detection**, **compliance validation**
  - **LLM integration**, **pattern recognition**, **collaboration**
- **Browsers**: 5 configurations (Chrome, Firefox, Safari, Mobile Chrome, Mobile Safari)

## 🤝 Contributing

When adding new features to Regulens:

1. Add corresponding test file in `tests/e2e/tests/`
2. Update fixtures in `test-data.ts` if needed
3. Add helper functions to `helpers.ts` for reusability
4. Document new test commands in this README
5. Ensure tests pass locally before committing
6. Tests must pass in CI before merging

## 📚 Resources

- [Playwright Documentation](https://playwright.dev)
- [Playwright Best Practices](https://playwright.dev/docs/best-practices)
- [Playwright API Reference](https://playwright.dev/docs/api/class-playwright)
- [Regulens API Documentation](../../docs/API.md)

## 🆘 Troubleshooting

### Tests timing out

- Increase timeout in `playwright.config.ts`
- Check if Regulens is running on correct port
- Verify database and Redis are accessible

### Authentication failures

- Check test credentials in `.env`
- Verify JWT secret is configured
- Ensure user exists in test database

### Flaky tests

- Add explicit waits for dynamic content
- Use `waitForAPIResponse` helper
- Check network conditions
- Review test isolation

### Browser installation issues

```bash
# Reinstall browsers
npx playwright install --force
```

## 📝 License

Part of the Regulens AI Compliance Platform.
Copyright © 2025 Gaigenticai
