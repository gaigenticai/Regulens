# Violations Report

Generated: 2025-10-08T04:21:18Z
Repository: /Users/krishna/Downloads/gaigenticai/Regulens
Scan mode: Read-only recursive grep with common build/cache/binary exclusions

Excluded directories: .git, node_modules, dist, build, .next, out, target, .cache, coverage, vendor, __pycache__, .venv, .mypy_cache, .pytest_cache, .gradle, .idea
Excluded file patterns: *.min.*, *.map, *.lock, *.svg, *.png, *.jpg, *.jpeg, *.gif, *.pdf, *.zip, *.gz, *.tar, *.jar, *.bin

------------------------------------------------------------
Summary (counts by category)

- Forbidden terms (stub/stubs/placeholder/fake/dummy/mock/mocks/simulate/simulation/hardcoded/hard-coded):
  - Total matches: 517 lines across 49 files
  - In tests: 46 lines; In docs/markdown: 36 lines; Non-test/non-doc: 438 lines

- Unimplemented markers (NotImplemented, NotImplementedError, raise NotImplementedError, TODO, FIXME, TBD, WIP, HACK, "Not implemented" throws):
  - Total matches: 39 lines across 9 files (Non-test/non-doc: 30)

- Localhost/host literals (localhost, 127.0.0.1, 0.0.0.0):
  - Total matches: 101 lines across 38 files (Non-test/non-doc: 78)

- External http/https endpoints (excluding localhost):
  - Total matches: 681 lines across 40 files (Non-test/non-doc: 666)

- Likely secrets (AWS/GCP/Slack/GitHub tokens, private keys, JWTs):
  - Strong patterns: 0 lines
  - Secret-like assignments (e.g., password/token/API_KEY/SECRET_KEY assigned string): 12 lines across 5 files (Non-test/non-doc: 7)

------------------------------------------------------------
Forbidden terms — details

Terms scanned: stub, stubs, placeholder, place-holder, fake, dummy, mock, mocks, simulate, simulation, hardcoded, hard-coded

Representative matches (non-test, non-doc):
- .cursor/rules/rule.mdc: contains the policy text with multiple forbidden words (policy reference; not a code stub)
- frontend UI components: many placeholder="..." attributes (UI/UX placeholders; not code stubs)
  - frontend/src/components/LoginForm.tsx (username/password placeholders)
  - frontend/src/components/ActivityFeed/ActivityFilters.tsx (search placeholder)
  - frontend/src/components/DecisionEngine/* (name/weight/unit placeholders)
  - frontend/src/pages/* (various input placeholders)
- D3 visualization uses "simulation" as in force simulation (not mock/simulate behavior)
  - frontend/src/components/DecisionTree/DecisionTreeVisualization.tsx
  - frontend/src/pages/PatternAnalysis.tsx
- "NO MOCKS" headers in frontend code comments emphasizing non-mock implementations
  - frontend/src/App.tsx, src/types/api.ts, src/contexts/AuthContext.tsx, etc.
- Hardcoded mentions are primarily in the rule text; code paths rely on env/config in most places

Notable true placeholder use:
- shared/llm/compliance_functions.cpp — get_regulatory_updates returns objects with "effective_date": "TBD" in responses.
  - Example around line ~485:
    - {"effective_date", "TBD"}
  - This is a placeholder value in API output and violates the no-placeholder rule.

------------------------------------------------------------
Unimplemented markers — details (non-test, non-doc)

Key files with TODO/WIP/HACK/TBD/Not implemented:
- /Users/krishna/Downloads/gaigenticai/Regulens/shared/web_ui/web_ui_handlers.cpp
  - Multiple TODOs to integrate with ConfigurationManager, DataIngestionFramework, AgentOrchestrator, etc.
- Backup variants duplicating the same TODOs (should not be in repo):
  - shared/web_ui/web_ui_handlers.cpp.backup2
  - shared/web_ui/web_ui_handlers.cpp.bak
  - shared/web_ui/web_ui_handlers.cpp.bak2
- shared/llm/compliance_functions.cpp
  - (see "TBD" placeholder noted above)
- main.cpp
  - Appears in unimplemented list; review for any residual TODO/HACK markers

Sample lines (non-test, non-doc):
- shared/web_ui/web_ui_handlers.cpp: "// TODO: Integrate with ConfigurationManager to persist configuration changes"
- shared/web_ui/web_ui_handlers.cpp: "// TODO: Query DataIngestionFramework for current ingestion status"
- shared/web_ui/web_ui_handlers.cpp: "// TODO: Fetch real model metrics from LearningSystem API"

------------------------------------------------------------
Localhost/host literals — details (non-test, non-doc)

Files containing localhost/127.0.0.1/0.0.0.0:
- frontend/vite.config.ts — dev proxy targets http://localhost:8080 (dev-only)
- Dockerfile — healthcheck uses http://localhost:8080 (container-internal)
- main.cpp — logs "Web UI server started on http://localhost:8080"; server port is hardcoded to 8080
- shared/config/environment_validator.cpp — explicitly blocks localhost in production (good safeguard)
- shared/config/configuration_manager.cpp.backup — has localhost defaults (see below; should be removed)
- .env, .env.example, GitHub Actions env/test config, docker-compose.yml, package.json, playwright.config.ts (expected in dev/test tooling)

Sample lines:
- frontend/vite.config.ts: target: 'http://localhost:8080'
- Dockerfile: CMD curl -f http://localhost:8080/health || exit 1
- shared/config/environment_validator.cpp: guards against localhost values in production

------------------------------------------------------------
External http/https endpoints — details (excluding localhost)

Dominant sources:
- frontend/package-lock.json — npm registry and sponsor URLs (expected)
- infrastructure/monitoring/prometheus/*.yml — alerting/monitoring endpoints
- .env.example — reference endpoints/templates
- Various C++ sources referencing external endpoints in comments/config validation

Top files by match count:
- frontend/package-lock.json (~529 lines)
- infrastructure/monitoring/prometheus/alerting-rules.yml (~38 lines)
- .env.example (~15 lines)
- infrastructure/monitoring/prometheus/prometheus-alerting-configmap.yaml (~14 lines)

------------------------------------------------------------
Likely secrets — details

Strong patterns (AWS keys, Google API keys, Slack tokens, GitHub tokens, private keys, JWTs):
- No matches found.

Secret-like assignments (string literals to password/token/API_KEY/SECRET_KEY):
- frontend/src/components/LoginForm.tsx — validation messages (not secrets)
- shared/config/configuration_manager.hpp — constant names like SEC_EDGAR_API_KEY, ERP_SYSTEM_API_KEY, VECTOR_DB_API_KEY, LLM_OPENAI_API_KEY, LLM_ANTHROPIC_API_KEY (names only; values expected from env)

------------------------------------------------------------
Repository hygiene issues (should be addressed)

- Backup/source copies present with placeholder/hardcoded defaults:
  - shared/config/configuration_manager.cpp.backup
    - Defaults include: database.host="localhost", database.password="password", email.smtp.password="placeholder_password"
  - shared/web_ui/web_ui_handlers.cpp.backup2/.bak/.bak2
  - regulatory_monitor/regulatory_monitor_standalone_ui_demo.cpp.backup
  - These should be removed from the tracked repo to avoid accidental use.

- Hardcoded port/host in main.cpp:
  - WebUIServer(8080) and log message referencing http://localhost:8080
  - Should be parameterized via ConfigurationManager/env for production readiness.

------------------------------------------------------------
Notes on interpretation

- Many "placeholder" matches are UI input placeholders and are not code stubs; they are acceptable UX.
- "simulation" matches appear in D3 force simulations and are not mock/simulate test code.
- The clear placeholder violation found in runtime output is the "effective_date":"TBD" field in shared/llm/compliance_functions.cpp.
- Localhost references in dev tooling and container-internal healthchecks are expected, but production configs/code should not rely on localhost by default.
- No strong-signature secrets were found in the repository.
