# MiniDB

MiniDB is a C++17 relational database prototype built from scratch. It includes a SQL-like command-line interface, a parser and executor pipeline, slotted-page storage, schema persistence, and persistent integer indexing.

The project is designed as an educational database engine implementation that demonstrates how real storage systems work internally while remaining compact enough to understand end-to-end.

---

# Features

## SQL-like CLI

Supports:

* `CREATE TABLE`
* `INSERT`
* `SELECT`
* `UPDATE`
* `DELETE`
* `DESCRIBE`
* `USE`
* `COMMIT`

Interactive shell:

```text
MiniDB >
```

---

## Persistent File-Based Storage

* Single `.db` file per database
* Fixed-size page storage
* Pager abstraction for page reads/writes
* Slotted-page row layout
* Reusable deleted row slots
* Explicit commit-based persistence

Changes are persisted only after:

```sql
COMMIT;
```

or:

```text
.commit
```

---

## Schema Management

Supports schema serialization and reload across sessions.

Example:

```sql
CREATE TABLE users (
  id INT PRIMARY KEY,
  age INT,
  name TEXT
);
```

Schema metadata is stored inside the database file and automatically restored when reopening the database.

---

## Indexing

MiniDB supports:

### Primary Index

Created automatically through:

```sql
PRIMARY KEY
```

### Secondary Index

Created using:

```sql
INDEX index_name (column_name)
```

Example:

```sql
CREATE TABLE users (
  id INT PRIMARY KEY,
  age INT,
  name TEXT,
  INDEX age_idx (age)
);
```

Features:

* Persistent index pages
* Equality lookup optimization
* Index maintenance on:

  * INSERT
  * UPDATE
  * DELETE
* Index metadata shown in `DESCRIBE`

Example indexed query:

```sql
SELECT * FROM users WHERE age = 21;
```

---

# Architecture

```text
src/
  cli/          Interactive shell
  parser/       Tokenizer + parser + AST
  executor/     Query execution layer
  storage/      Pager + metadata + slotted pages
  index/        Persistent integer indexes
  common/       Shared schema/value definitions
```

Execution pipeline:

```text
Query
  ↓
Tokenizer
  ↓
Parser
  ↓
Executor
  ↓
Storage / Index
```

---

# Build Instructions

## Windows

### Recommended

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\build.ps1
```

Final executable:

```text
dist/minidb.exe
```

Run:

```powershell
.\dist\minidb.exe
```

---

### Manual Build

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

Executable:

```text
build/Debug/minidb.exe
```

---

## Linux/macOS

```bash
cmake -S . -B build
cmake --build build
```

Run:

```bash
./dist/minidb
```

---

## Install Dependencies

### Ubuntu/Debian

```bash
sudo apt update
sudo apt install cmake g++
```

### Fedora

```bash
sudo dnf install cmake gcc-c++
```

### macOS (Homebrew)

```bash
brew install cmake
```

---

# Supported Commands

## Database Selection

```sql
USE 'demo.db';
```

---

## Create Table

### Basic Table

```sql
CREATE TABLE users (
  id INT,
  name TEXT
);
```

### Table with Primary Key + Secondary Index

```sql
CREATE TABLE users (
  id INT PRIMARY KEY,
  age INT,
  name TEXT,
  INDEX age_idx (age)
);
```

---

## Insert Rows

```sql
INSERT INTO users VALUES (1, 21, 'Ada');
INSERT INTO users VALUES (2, 22, 'Grace');
```

---

## Select Queries

### Full Table Scan

```sql
SELECT * FROM users;
```

### Specific Columns

```sql
SELECT id, name FROM users;
```

### WHERE Clause

```sql
SELECT * FROM users WHERE id = 1;
SELECT * FROM users WHERE age = 21;
```

---

## Update Queries

```sql
UPDATE users SET age = 23 WHERE id = 2;
UPDATE users SET name = 'Lovelace' WHERE id = 1;
```

---

## Delete Queries

```sql
DELETE FROM users WHERE id = 1;
```

---

## Describe Table

```sql
DESCRIBE users;
DESC users;
```

Example output:

```text
id INT - PRIMARY
age INT - SECONDARY
name TEXT
```

---

## Commit Changes

```sql
COMMIT;
```

or:

```text
.commit
```

---

## Exit CLI

```text
.exit
```

---

# Full Demo Session

```sql
USE 'demo.db';

CREATE TABLE users (
  id INT PRIMARY KEY,
  age INT,
  name TEXT,
  INDEX age_idx (age)
);

INSERT INTO users VALUES (1, 21, 'Ada');
INSERT INTO users VALUES (2, 22, 'Grace');

SELECT * FROM users;
SELECT id, name FROM users WHERE age = 21;

UPDATE users SET age = 23 WHERE id = 2;
UPDATE users SET name = 'Lovelace' WHERE id = 1;

DELETE FROM users WHERE id = 1;

DESCRIBE users;

COMMIT;
.exit
```

Reopen database:

```sql
USE 'demo.db';
SELECT * FROM users;
```

---

# Quoting Rules

## INT Values

Do not use quotes:

```sql
1
42
900
```

---

## TEXT Values

Use single quotes:

```sql
'Ada'
'hello world'
```

---

## WHERE Conditions

```sql
id = 1
name = 'Ada'
```

---

# Testing Commands

## Build Verification

### Windows

```powershell
cmake -S . -B build
cmake --build build --config Debug
```

### Linux/macOS

```bash
cmake -S . -B build
cmake --build build
```

---

## Basic Functional Test

Run MiniDB:

```powershell
.\dist\minidb.exe
```

or:

```bash
./dist/minidb
```

Then execute:

```sql
USE 'test.db';

CREATE TABLE users (
  id INT PRIMARY KEY,
  age INT,
  name TEXT,
  INDEX age_idx (age)
);

INSERT INTO users VALUES (1, 21, 'Ada');
INSERT INTO users VALUES (2, 22, 'Grace');

SELECT * FROM users;
SELECT * FROM users WHERE age = 21;

UPDATE users SET age = 23 WHERE id = 2;
SELECT * FROM users WHERE age = 23;

DELETE FROM users WHERE id = 1;
SELECT * FROM users;

DESCRIBE users;

COMMIT;
.exit
```

---

## Persistence Test

Reopen MiniDB:

```sql
USE 'test.db';
SELECT * FROM users;
```

Expected:

* Updated rows should persist
* Deleted rows should remain deleted
* Index metadata should still exist

---

## Index Validation Queries

```sql
SELECT * FROM users WHERE id = 2;
SELECT * FROM users WHERE age = 23;
```

These queries validate:

* Primary index lookup
* Secondary index lookup
* Persistent index reload

---

# Current Limitations

MiniDB is intentionally scoped as a learning-focused database prototype.

Not implemented:

* JOIN operations
* GROUP BY / aggregation
* ORDER BY
* Transactions beyond explicit commit
* WAL / crash recovery
* Concurrency / locking
* Full SQL grammar
* Multi-level B+ tree internal nodes

Indexes currently support integer equality lookups.

---

# Tech Stack

* Language: C++17
* Build System: CMake
* Storage: File-based binary pages
* Indexing: Persistent integer leaf-chain index
