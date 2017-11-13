/**
 * Initialize database for suil admin interface
 */

CREATE SCHEMA IF NOT EXISTS suildb;

---
--- suil.users
---
CREATE TABLE IF NOT EXISTS suildb.users (
    id SERIAL,
    username VARCHAR(64) PRIMARY KEY UNIQUE,
    email TEXT UNIQUE NOT NULL,
    fullname VARCHAR(64) NOT NULL,
    salt VARCHAR(20) NOT NULL,
    passwd VARCHAR(60) NOT NULL,
    roles TEXT[]
);