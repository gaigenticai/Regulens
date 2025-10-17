/**
 * Mock Backend Server
 * Simple Node.js server to mock authentication and basic API endpoints
 * Allows frontend to work while backend build issues are resolved
 */

const express = require('express');
const cors = require('cors');
const jwt = require('jsonwebtoken');

const app = express();
const PORT = 8080;

// Mock JWT secret (in production this would be from environment)
const JWT_SECRET = 'mock_jwt_secret_for_development_only_not_for_production';

// Middleware
app.use(cors({
  origin: 'http://localhost:3002',
  credentials: true
}));
app.use(express.json());

// Mock user database
const users = [
  { id: 1, username: 'admin', password: 'admin', role: 'administrator' },
  { id: 2, username: 'user', password: 'user', role: 'user' }
];

// Authentication endpoints
app.post('/api/auth/login', (req, res) => {
  const { username, password } = req.body;

  const user = users.find(u => u.username === username && u.password === password);

  if (user) {
    const token = jwt.sign(
      { userId: user.id, username: user.username, role: user.role },
      JWT_SECRET,
      { expiresIn: '24h' }
    );

    res.json({
      success: true,
      token,
      user: {
        id: user.id,
        username: user.username,
        role: user.role
      }
    });
  } else {
    res.status(401).json({
      success: false,
      error: 'Invalid credentials'
    });
  }
});

app.post('/api/auth/register', (req, res) => {
  const { username, password } = req.body;

  // Check if user exists
  if (users.find(u => u.username === username)) {
    return res.status(400).json({
      success: false,
      error: 'User already exists'
    });
  }

  const newUser = {
    id: users.length + 1,
    username,
    password,
    role: 'user'
  };

  users.push(newUser);

  const token = jwt.sign(
    { userId: newUser.id, username: newUser.username, role: newUser.role },
    JWT_SECRET,
    { expiresIn: '24h' }
  );

  res.json({
    success: true,
    token,
    user: {
      id: newUser.id,
      username: newUser.username,
      role: newUser.role
    }
  });
});

// Health check
app.get('/health', (req, res) => {
  res.json({
    status: 'healthy',
    timestamp: new Date().toISOString(),
    services: {
      database: 'mock',
      redis: 'mock',
      api: 'running'
    }
  });
});

// Mock API endpoints
app.get('/api/dashboard', (req, res) => {
  res.json({
    success: true,
    data: {
      compliance_score: 85,
      alerts_count: 3,
      transactions_today: 1250,
      risk_assessments: 45
    }
  });
});

app.get('/api/transactions', (req, res) => {
  res.json({
    success: true,
    data: [
      { id: 1, amount: 1000, status: 'approved', timestamp: new Date().toISOString() },
      { id: 2, amount: 2500, status: 'pending', timestamp: new Date().toISOString() }
    ]
  });
});

// Middleware to verify JWT
const authenticateToken = (req, res, next) => {
  const authHeader = req.headers['authorization'];
  const token = authHeader && authHeader.split(' ')[1];

  if (!token) {
    return res.status(401).json({ success: false, error: 'Access token required' });
  }

  jwt.verify(token, JWT_SECRET, (err, user) => {
    if (err) {
      return res.status(403).json({ success: false, error: 'Invalid token' });
    }
    req.user = user;
    next();
  });
};

// Protected route example
app.get('/api/user/profile', authenticateToken, (req, res) => {
  res.json({
    success: true,
    user: req.user
  });
});

app.listen(PORT, () => {
  console.log(`🚀 Mock Backend Server running on http://localhost:${PORT}`);
  console.log(`📊 Health check: http://localhost:${PORT}/health`);
  console.log(`🔐 Login with: username: admin, password: admin`);
  console.log(`🔐 Or register a new user`);
});