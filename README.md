# pg_fuzzyscore 🔍
**A tiny fuzzy search for PostgreSQL, ported to C from the amazing fuzzysort javascript library**

## Features:
* **IDE-Style Matching:** Implements the strict backtracking algorithm to reward word-boundary matches (e.g., `fs` matches **F**uzzy**S**ort).
* **Multi-token Support:** Handles spaces naturally by splitting search strings into tokens (up to 16).
* **Normalization:** Accent removal and lowercasing.
* **Prepared rows:** Stores the prepared target string in a BYTEA column and uses it for the matching algorithm.


## Usage:
1. Prepare your data.
```
ALTER TABLE items ADD COLUMN prepared bytea;
UPDATE items SET prepared = fuzzyprepare(name);
```
2. Search using the prepared column.
```
SELECT * FROM items 
WHERE fuzzyscore(prepared, 'some item') > 0.5
ORDER BY fuzzyscore(prepared, 'some item') DESC;
```

## Credits:
This project is a C port of the excellent fuzzysort library created by farzher.
The core scoring logic and backtracking algorithm are based on his original work, i just tried my best to try and understand his code and port it, it is not a 1:1 port.