import jwt from 'jsonwebtoken';

const SECRET = process.env.JWT_SECRET;

export function requireAuth(req, res, next) {
  const header = req.get('authorization') ?? '';
  const token = header.startsWith('Bearer ') ? header.slice(7) : null;
  if (!token) {
    return res.status(401).json({ error: 'missing_token' });
  }
  try {
    req.user = jwt.verify(token, SECRET, { algorithms: ['HS256'] });
    return next();
  } catch (err) {
    if (err instanceof jwt.TokenExpiredError) {
      return res.status(401).json({ error: 'token_expired', expiredAt: err.expiredAt });
    }
    return res.status(403).json({ error: 'invalid_token' });
  }
}
