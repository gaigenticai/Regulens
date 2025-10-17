#!/usr/bin/env node

/**
 * Temporary Node.js API Bridge for Regulens
 * This serves as a development proxy while the C++ backend compilation is being fixed
 * IMPORTANT: This is a temporary development server only - NOT for production
 */

const express = require('express');
const cors = require('cors');
const pg = require('pg');
const redis = require('redis');
const dotenv = require('dotenv');

dotenv.config();

console.log('Environment variables:');
console.log('DB_USER:', process.env.DB_USER);
console.log('DB_PASSWORD:', process.env.DB_PASSWORD ? '***' : 'NOT SET');
console.log('DB_NAME:', process.env.DB_NAME);

const app = express();

// Middleware
app.use(cors({
  origin: process.env.CORS_ALLOWED_ORIGIN || 'http://localhost:5173',
  credentials: true
}));
app.use(express.json());

// Database connection pool
const pool = new pg.Pool({
  user: process.env.DB_USER || 'regulens_user',
  password: process.env.DB_PASSWORD || 'test_password_123',
  host: process.env.DB_HOST || 'localhost',
  port: process.env.DB_PORT || 5432,
  database: process.env.DB_NAME || 'regulens_compliance',
});

// Redis client
const redisClient = redis.createClient({
  host: process.env.REDIS_HOST || 'localhost',
  port: process.env.REDIS_PORT || 6379,
  password: process.env.REDIS_PASSWORD || 'test_redis_password',
});

// Health check
app.get('/health', (req, res) => {
  res.json({ status: 'OK', timestamp: new Date().toISOString() });
});

// Auth endpoints
app.post('/api/auth/login', async (req, res) => {
  try {
    const { username, password } = req.body;

    if (!username) {
      return res.status(400).json({ error: 'Username is required' });
    }

    // For demo purposes, accept admin/Admin123
    if (username === 'admin' && password === 'Admin123') {
      const token = `jwt-admin-${Date.now()}`;

      res.json({
        accessToken: token,
        refreshToken: `refresh-${token}`,
        tokenType: 'Bearer',
        expiresIn: 86400,
        user: {
          id: 'admin-user-id',
          username: 'admin',
          permissions: ['dashboard.view', 'transactions.view', 'transactions.manage']
        }
      });
      return;
    }

    // For other users, try database lookup
    try {
      const userResult = await pool.query(
        'SELECT user_id, username FROM user_authentication WHERE username = $1 AND is_active = true',
        [username]
      );

      if (userResult.rows.length > 0) {
        const user = userResult.rows[0];
        const token = `jwt-${user.user_id}-${Date.now()}`;

        res.json({
          accessToken: token,
          refreshToken: `refresh-${token}`,
          tokenType: 'Bearer',
          expiresIn: 86400,
          user: {
            id: user.user_id,
            username: user.username,
            permissions: ['dashboard.view']
          }
        });
        return;
      }
    } catch (dbErr) {
      console.log('Database not available, using mock auth');
    }

    res.status(401).json({ error: 'Invalid username or password' });

  } catch (err) {
    console.error('Login error:', err.message);
    res.status(500).json({ error: 'Internal server error' });
  }
});

app.post('/api/auth/logout', (req, res) => {
  res.json({ success: true });
});

// Transactions endpoints (stub)
app.get('/api/transactions', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM transactions LIMIT 10');
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

app.post('/api/transactions', async (req, res) => {
  try {
    const { transaction_id, amount, type, customer_id } = req.body;
    const result = await pool.query(
      'INSERT INTO transactions (transaction_id, amount, type, customer_id, created_at) VALUES ($1, $2, $3, $4, NOW()) RETURNING *',
      [transaction_id, amount, type, customer_id]
    );
    res.json(result.rows[0]);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Audit endpoints (stub)
app.get('/api/audit-trail', async (req, res) => {
  try {
    const result = await pool.query('SELECT * FROM audit_logs ORDER BY timestamp DESC LIMIT 20');
    res.json(result.rows);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

// Alerts endpoints (stub)
app.get('/api/alerts', async (req, res) => {
  res.json([
    { id: '1', severity: 'high', message: 'High-risk transaction detected', timestamp: new Date() },
    { id: '2', severity: 'medium', message: 'Compliance check required', timestamp: new Date() },
  ]);
});

// Decisions endpoints (stub)
app.get('/api/decisions', async (req, res) => {
  res.json([
    { id: 'dec-1', type: 'APPROVE', confidence: 0.95, timestamp: new Date() },
    { id: 'dec-2', type: 'REVIEW', confidence: 0.67, timestamp: new Date() },
  ]);
});

// Regulatory endpoints (stub)
app.get('/api/regulatory-changes', async (req, res) => {
  res.json([
    { id: 'reg-1', title: 'AML Directive Update', date: new Date(), jurisdiction: 'EU' },
  ]);
});

// Memory endpoint (stub)
app.get('/api/memory/visualize', (req, res) => {
  res.json({
    nodes: [
      { id: 'node-1', label: 'Transaction Memory', type: 'memory' },
      { id: 'node-2', label: 'Risk Cache', type: 'cache' },
    ],
    edges: [
      { source: 'node-1', target: 'node-2', label: 'updates' },
    ],
  });
});

// Error handling middleware
app.use((err, req, res, next) => {
  console.error(err);
  res.status(500).json({ error: 'Internal server error', message: err.message });
});

const PORT = process.env.WEB_SERVER_PORT || 8080;
app.listen(PORT, '0.0.0.0', () => {
  console.log(`\n✅ Regulens Development API Bridge running on port ${PORT}`);
  console.log(`📝 WARNING: This is a temporary development server`);
  console.log(`🔄 Production C++ backend compilation in progress...\n`);
});

