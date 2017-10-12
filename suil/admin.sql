/**
 * Server administration module database
 */

---
--- admin.apps
---
CREATE TABLE apps (
    id      INTEGER AUTO INCREMENT,
    owner  INTEGER NOT NULL,
    tag    VARCHAR(32) PRIMARY KEY UNIQUE,
    name   VARCHAR(20) UNIQUE NOT NULL,
    version INTEGER NOT NULL,
    description TEXT NOT NULL
);

---
--- admin.app_config
---
CREATE TABLE app_config (
    id     INTEGER AUTO INCREMENT,
    name   VARCHAR(32) PRIMARY KEY,
    value  TEXT NOT NULL,
    app_id INTEGER NOT NULL,
    data   INTEGER NOT NULL
);