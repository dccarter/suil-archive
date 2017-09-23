/**
 * Create database for suil admin interface
 */
 
---
--- suil.users
---
CREATE TABLE users (
    id INTEGER AUTO INCREMENT,
    username VARCHAR(64) PRIMARY KEY UNIQUE,
    email TEXT NOT NULL,
    fullname VARCHAR(64) NOT NULL,
    salt VARCHAR(20) NOT NULL,
    passwd VARCHAR(60) NOT NULL
);