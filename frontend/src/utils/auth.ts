// JWT parsing and authentication utilities

interface JWTPayload {
  user_id?: string;
  sub?: string;
  username?: string;
  email?: string;
  roles?: string[];
  exp?: number;
  iat?: number;
}

// Parse JWT token and return payload
export const parseJwt = (token: string): JWTPayload | null => {
  try {
    const base64Url = token.split('.')[1];
    const base64 = base64Url.replace(/-/g, '+').replace(/_/g, '/');
    const jsonPayload = decodeURIComponent(atob(base64).split('').map(function(c) {
      return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2);
    }).join(''));

    return JSON.parse(jsonPayload);
  } catch (error) {
    console.error('Failed to parse JWT token:', error);
    return null;
  }
};

// Get user ID from JWT token
export const getUserIdFromToken = (): string => {
  const token = localStorage.getItem('token');
  if (!token || token === 'demo-token') {
    return 'anonymous_user'; // Fallback for demo/development
  }

  const decoded = parseJwt(token);
  return decoded?.user_id || decoded?.sub || 'anonymous_user';
};

// Get username from JWT token
export const getUsernameFromToken = (): string => {
  const token = localStorage.getItem('token');
  if (!token || token === 'demo-token') {
    return 'Anonymous User'; // Fallback for demo/development
  }

  const decoded = parseJwt(token);
  return decoded?.username || 'Anonymous User';
};

// Get user roles from JWT token
export const getUserRolesFromToken = (): string[] => {
  const token = localStorage.getItem('token');
  if (!token || token === 'demo-token') {
    return ['user']; // Default role for demo/development
  }

  const decoded = parseJwt(token);
  return decoded?.roles || ['user'];
};

// Check if user has specific role
export const hasRole = (role: string): boolean => {
  const roles = getUserRolesFromToken();
  return roles.includes(role);
};

// Check if JWT token is expired
export const isTokenExpired = (): boolean => {
  const token = localStorage.getItem('token');
  if (!token || token === 'demo-token') {
    return true; // Consider demo token as expired
  }

  const decoded = parseJwt(token);
  if (!decoded?.exp) {
    return true; // No expiration claim
  }

  const currentTime = Math.floor(Date.now() / 1000);
  return decoded.exp < currentTime;
};
