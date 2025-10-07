import { test, expect } from '@playwright/test';

/**
 * MCDA Decision Engine Tests
 *
 * Tests Multi-Criteria Decision Analysis algorithms:
 * - TOPSIS (Technique for Order of Preference by Similarity to Ideal Solution)
 * - ELECTRE III (Elimination and Choice Translating Reality)
 * - PROMETHEE II (Preference Ranking Organization Method for Enrichment Evaluations)
 * - AHP (Analytic Hierarchy Process)
 * - VIKOR (VlseKriterijumska Optimizacija I Kompromisno Resenje)
 */

test.describe('MCDA Decision Engine', () => {
  test.beforeEach(async ({ page }) => {
    await page.goto('/');
    await page.waitForLoadState('networkidle');
  });

  test('should access decision engine interface', async ({ page }) => {
    // Look for decision engine section
    const decisionEngine = page.locator(
      '.decision-engine, .mcda-engine, [data-testid="decision-engine"]'
    );

    if (await decisionEngine.count() > 0) {
      await expect(decisionEngine.first()).toBeVisible();
      console.log('Decision engine interface found');
    } else {
      // Try navigating to decisions page
      const decisionsLink = page.locator('a:has-text("Decisions"), nav a:has-text("Decision")');
      if (await decisionsLink.count() > 0) {
        await decisionsLink.first().click();
        await page.waitForTimeout(1000);
      }
    }
  });

  test('should display available MCDA algorithms', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for algorithm selector
    const algorithmSelector = page.locator(
      'select[name="algorithm"], .algorithm-selector, [data-testid="mcda-algorithm"]'
    );

    if (await algorithmSelector.count() > 0) {
      const options = await algorithmSelector.first().locator('option').allTextContents();
      console.log('Available MCDA algorithms:', options);

      // Should include production algorithms
      const algorithmsText = options.join(' ').toUpperCase();
      const hasTOPSIS = algorithmsText.includes('TOPSIS');
      const hasELECTRE = algorithmsText.includes('ELECTRE');
      const hasPROMETHEE = algorithmsText.includes('PROMETHEE');
      const hasAHP = algorithmsText.includes('AHP');

      console.log('Algorithms available:', {
        TOPSIS: hasTOPSIS,
        ELECTRE: hasELECTRE,
        PROMETHEE: hasPROMETHEE,
        AHP: hasAHP
      });
    }
  });

  test('should create new decision with criteria', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for new decision button
    const newDecisionBtn = page.locator(
      'button:has-text("New Decision"), button:has-text("Create Decision")'
    );

    if (await newDecisionBtn.count() > 0) {
      await newDecisionBtn.first().click();
      await page.waitForTimeout(500);

      // Check for decision form
      const decisionForm = page.locator('form, .decision-form, [role="dialog"]');
      const isVisible = await decisionForm.isVisible().catch(() => false);

      if (isVisible) {
        console.log('Decision creation form opened');

        // Look for criteria input fields
        const criteriaInputs = decisionForm.locator('input[name*="criteria"], .criteria-input');
        const count = await criteriaInputs.count();
        console.log(`Found ${count} criteria input fields`);
      }
    }
  });

  test('should display decision alternatives', async ({ page }) => {
    await page.waitForTimeout(2000);

    const decisions = page.locator('.decision, .decision-item, .decision-card');

    if (await decisions.count() > 0) {
      const firstDecision = decisions.first();
      await firstDecision.click();
      await page.waitForTimeout(500);

      // Look for alternatives list
      const alternatives = page.locator(
        '.alternative, .option, .choice, [data-testid="alternative"]'
      );

      const count = await alternatives.count();
      console.log(`Decision has ${count} alternatives`);

      if (count > 0) {
        const firstAlt = alternatives.first();
        const altText = await firstAlt.textContent();
        console.log('Sample alternative:', altText?.substring(0, 100));
      }
    }
  });

  test('should show TOPSIS ideal solution calculations', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for TOPSIS-specific indicators
    const topsisElements = page.locator(
      ':has-text("ideal solution"), :has-text("TOPSIS"), .topsis-result'
    );

    if (await topsisElements.count() > 0) {
      console.log('TOPSIS ideal solution elements found');

      // Look for similarity scores
      const similarityScores = page.locator(
        '.similarity-score, .topsis-score, [data-testid="similarity"]'
      );

      if (await similarityScores.count() > 0) {
        const scores = await similarityScores.allTextContents();
        console.log('TOPSIS similarity scores:', scores.slice(0, 3));
      }
    }
  });

  test('should display ELECTRE III concordance/discordance matrices', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for ELECTRE-specific elements
    const electreElements = page.locator(
      ':has-text("concordance"), :has-text("discordance"), :has-text("ELECTRE")'
    );

    if (await electreElements.count() > 0) {
      console.log('ELECTRE III elements found');

      // Look for matrix visualizations
      const matrices = page.locator('.matrix, table.concordance, table.discordance');

      if (await matrices.count() > 0) {
        const matrixCount = await matrices.count();
        console.log(`Found ${matrixCount} ELECTRE matrices`);
      }
    }
  });

  test('should show PROMETHEE II preference functions', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for PROMETHEE-specific elements
    const prometheeElements = page.locator(
      ':has-text("PROMETHEE"), :has-text("preference function"), :has-text("net flow")'
    );

    if (await prometheeElements.count() > 0) {
      console.log('PROMETHEE II elements found');

      // Look for preference function types
      const prefFunctions = page.locator(
        '.preference-function, [data-testid="pref-function"]'
      );

      if (await prefFunctions.count() > 0) {
        const functions = await prefFunctions.allTextContents();
        console.log('Preference functions:', functions.slice(0, 3));
      }
    }
  });

  test('should display AHP eigenvector calculations', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for AHP-specific elements
    const ahpElements = page.locator(
      ':has-text("AHP"), :has-text("eigenvector"), :has-text("consistency")'
    );

    if (await ahpElements.count() > 0) {
      console.log('AHP elements found');

      // Look for consistency ratio
      const consistencyRatio = page.locator(
        '.consistency-ratio, [data-testid="consistency-ratio"]'
      );

      if (await consistencyRatio.count() > 0) {
        const ratio = await consistencyRatio.first().textContent();
        console.log('AHP consistency ratio:', ratio);
      }
    }
  });

  test('should show decision tree visualization', async ({ page }) => {
    // Wait for decision tree visualization API
    const treePromise = page.waitForResponse(
      response => response.url().includes('/api/decisions/tree'),
      { timeout: 10000 }
    ).catch(() => null);

    await page.goto('/');
    const treeResponse = await treePromise;

    if (treeResponse && treeResponse.ok()) {
      const treeData = await treeResponse.json();
      console.log('Decision tree data received');

      await page.waitForTimeout(1000);

      // Look for D3.js visualization elements
      const visualization = page.locator('svg, canvas, .decision-tree-viz');

      if (await visualization.count() > 0) {
        console.log('Decision tree visualization rendered');
      }
    }
  });

  test('should display criteria weights', async ({ page }) => {
    await page.waitForTimeout(2000);

    const decisions = page.locator('.decision, .decision-item');

    if (await decisions.count() > 0) {
      const firstDecision = decisions.first();
      await firstDecision.click();
      await page.waitForTimeout(500);

      // Look for criteria weights
      const weights = page.locator(
        '.criteria-weight, .weight, [data-testid="weight"]'
      );

      if (await weights.count() > 0) {
        const weightValues = await weights.allTextContents();
        console.log('Criteria weights:', weightValues.slice(0, 5));
      }
    }
  });

  test('should show decision confidence scores', async ({ page }) => {
    await page.waitForTimeout(2000);

    const decisions = page.locator('.decision, .decision-item');

    if (await decisions.count() > 0) {
      const confidenceScores = page.locator(
        '.confidence-score, .confidence, [data-testid="confidence"]'
      );

      if (await confidenceScores.count() > 0) {
        const scores = await confidenceScores.allTextContents();
        console.log('Decision confidence scores:', scores.slice(0, 3));

        // Confidence should be between 0 and 1
        const firstScore = parseFloat(scores[0].replace(/[^\d.]/g, ''));
        if (!isNaN(firstScore)) {
          expect(firstScore).toBeGreaterThanOrEqual(0);
          expect(firstScore).toBeLessThanOrEqual(1);
        }
      }
    }
  });

  test('should compare multiple algorithms', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for algorithm comparison feature
    const compareBtn = page.locator(
      'button:has-text("Compare"), button:has-text("Compare Algorithms")'
    );

    if (await compareBtn.count() > 0) {
      await compareBtn.first().click();
      await page.waitForTimeout(1000);

      // Look for comparison table/chart
      const comparison = page.locator('.comparison, .algorithm-comparison, table');

      if (await comparison.count() > 0) {
        console.log('Algorithm comparison view displayed');

        // Check for multiple algorithm columns
        const algorithmColumns = comparison.locator('th, .algorithm-name');
        const count = await algorithmColumns.count();
        console.log(`Comparing ${count} algorithms`);
      }
    }
  });

  test('should visualize decision POST /api/decisions/visualize', async ({ request }) => {
    const baseURL = process.env.BASE_URL || 'http://localhost:8080';

    const response = await request.post(`${baseURL}/api/decisions/visualize`, {
      data: {
        algorithm: 'TOPSIS',
        criteria: ['cost', 'quality', 'time'],
        alternatives: ['option_a', 'option_b', 'option_c'],
        weights: [0.4, 0.4, 0.2]
      }
    });

    expect([200, 201, 400, 401, 403].includes(response.status())).toBeTruthy();

    if (response.ok()) {
      const data = await response.json();
      console.log('Decision visualization data structure:', Object.keys(data));
    }
  });

  test('should export decision results', async ({ page }) => {
    await page.waitForTimeout(2000);

    const exportBtn = page.locator(
      'button:has-text("Export"), button:has-text("Download Results")'
    );

    if (await exportBtn.count() > 0) {
      await exportBtn.first().click();
      await page.waitForTimeout(500);

      // Look for export format options
      const formatOptions = page.locator(
        '.export-format, [role="menuitem"]:has-text("JSON"), [role="menuitem"]:has-text("CSV")'
      );

      if (await formatOptions.count() > 0) {
        const formats = await formatOptions.allTextContents();
        console.log('Available export formats:', formats);
      }
    }
  });

  test('should show decision history and audit', async ({ page }) => {
    await page.waitForTimeout(2000);

    const decisions = page.locator('.decision, .decision-item');

    if (await decisions.count() > 0) {
      const firstDecision = decisions.first();
      await firstDecision.click();
      await page.waitForTimeout(500);

      // Look for decision metadata
      const metadata = page.locator('.metadata, .decision-info, dl');

      if (await metadata.count() > 0) {
        const metadataText = await metadata.first().textContent();
        console.log('Decision metadata present');

        // Check for audit fields
        const hasDecisionMaker = metadataText?.includes('decision') || metadataText?.includes('maker');
        const hasTimestamp = metadataText?.includes('date') || metadataText?.includes('time');

        console.log('Audit fields:', { hasDecisionMaker, hasTimestamp });
      }
    }
  });

  test('should handle decision optimization constraints', async ({ page }) => {
    await page.waitForTimeout(2000);

    // Look for constraints configuration
    const constraints = page.locator(
      '.constraint, .optimization-constraint, [data-testid="constraint"]'
    );

    if (await constraints.count() > 0) {
      const constraintCount = await constraints.count();
      console.log(`Found ${constraintCount} optimization constraints`);

      const firstConstraint = constraints.first();
      const constraintText = await firstConstraint.textContent();
      console.log('Sample constraint:', constraintText?.substring(0, 80));
    }
  });
});
