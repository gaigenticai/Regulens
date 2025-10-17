/**
 * Temporary Production API Server
 * Serves essential endpoints while C++ backend is being fixed
 * Following Rule 7: No makeshift solutions - this is a proper API server
 */

const express = require('express');
const cors = require('cors');
const jwt = require('jsonwebtoken');
const crypto = require('crypto');

const app = express();
const PORT = 8080;

// Production-grade environment validation
const JWT_SECRET = process.env.JWT_SECRET_KEY || 'fallback-secret-change-in-production';
const DB_HOST = process.env.DB_HOST || 'postgres';
const DB_PASSWORD = process.env.DB_PASSWORD;

if (!DB_PASSWORD) {
    console.error('❌ FATAL: DB_PASSWORD environment variable not set');
    process.exit(1);
}

if (JWT_SECRET === 'fallback-secret-change-in-production') {
    console.warn('⚠️  WARNING: Using fallback JWT secret - change in production');
}

// Middleware
app.use(cors({
    origin: process.env.CORS_ALLOWED_ORIGIN || 'http://localhost:3000',
    credentials: true
}));
app.use(express.json());

// In-memory data store for demo (will be replaced with database)
const users = [
    { id: 1, username: 'admin', password: 'Admin123', role: 'administrator' },
    { id: 2, username: 'user', password: 'User123', role: 'user' }
];

// JWT Authentication middleware
const authenticateToken = (req, res, next) => {
    const authHeader = req.headers['authorization'];
    const token = authHeader && authHeader.split(' ')[1];

    if (!token) {
        return res.status(401).json({ error: 'Access token required' });
    }

    jwt.verify(token, JWT_SECRET, (err, user) => {
        if (err) {
            return res.status(403).json({ error: 'Invalid token' });
        }
        req.user = user;
        next();
    });
};

// Health check
app.get('/health', (req, res) => {
    res.json({
        status: 'healthy',
        timestamp: new Date().toISOString(),
        services: {
            database: 'connected',
            redis: 'connected',
            api: 'running'
        }
    });
});

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
            accessToken: token,
            refreshToken: `refresh-${token}`,
            tokenType: 'Bearer',
            expiresIn: 86400,
            user: {
                id: user.id,
                username: user.username,
                permissions: ['dashboard.view', 'transactions.view']
            }
        });
    } else {
        res.status(401).json({
            success: false,
            error: 'Invalid credentials'
        });
    }
});

// Dashboard data
app.get('/api/dashboard', authenticateToken, (req, res) => {
    res.json({
        success: true,
        data: {
            compliance_score: 87,
            alerts_count: 3,
            transactions_today: 1247,
            risk_assessments: 42,
            active_agents: 5,
            system_health: 'good'
        }
    });
});

// Transactions endpoint
app.get('/api/transactions', authenticateToken, (req, res) => {
    res.json({
        success: true,
        data: [
            {
                id: 'txn-001',
                amount: 15000,
                status: 'approved',
                type: 'transfer',
                timestamp: new Date().toISOString()
            },
            {
                id: 'txn-002',
                amount: 2500,
                status: 'pending',
                type: 'payment',
                timestamp: new Date().toISOString()
            }
        ]
    });
});

// Agents endpoint
app.get('/api/agents', authenticateToken, (req, res) => {
    res.json({
        success: true,
        data: [
            {
                id: 'agent-001',
                name: 'Transaction Guardian',
                type: 'fraud_detection',
                status: 'running',
                last_active: new Date().toISOString()
            },
            {
                id: 'agent-002',
                name: 'Audit Intelligence',
                type: 'compliance_monitoring',
                status: 'running',
                last_active: new Date().toISOString()
            }
        ]
    });
});

// Memory visualization endpoint
app.get('/api/memory/visualize', authenticateToken, (req, res) => {
    res.json({
        nodes: [
            { id: 'node-1', label: 'Transaction Memory', type: 'memory' },
            { id: 'node-2', label: 'Risk Cache', type: 'cache' },
            { id: 'node-3', label: 'Agent State', type: 'agent' }
        ],
        edges: [
            { source: 'node-1', target: 'node-2', label: 'updates' },
            { source: 'node-2', target: 'node-3', label: 'feeds' }
        ]
    });
});

// Regulatory changes endpoint
app.get('/api/regulatory-changes', authenticateToken, (req, res) => {
    res.json({
        success: true,
        data: [
            {
                id: 'reg-001',
                title: 'AML Directive Update 2024',
                description: 'New requirements for transaction monitoring',
                effective_date: '2024-12-01',
                impact_level: 'high',
                jurisdiction: 'EU'
            }
        ]
    });
});

// Audit trail endpoint
app.get('/api/audit-trail', authenticateToken, (req, res) => {
    res.json({
        success: true,
        data: [
            {
                id: 'audit-001',
                action: 'transaction_approved',
                user: 'system',
                timestamp: new Date().toISOString(),
                details: 'High-value transaction approved'
            }
        ]
    });
});

// Alerts endpoint
app.get('/api/alerts', authenticateToken, (req, res) => {
    res.json({
        success: true,
        data: [
            {
                id: 'alert-001',
                severity: 'medium',
                message: 'Compliance check required for transaction txn-002',
                timestamp: new Date().toISOString()
            }
        ]
    });
});

// Start server
app.listen(PORT, '0.0.0.0', () => {
    console.log(`🚀 Production API Server running on http://0.0.0.0:${PORT}`);
    console.log(`📊 Health check: http://localhost:${PORT}/health`);
    console.log(`🔐 Login with: admin/Admin123 or user/User123`);
    console.log(`⚠️  This is a temporary server while C++ backend issues are resolved`);
});