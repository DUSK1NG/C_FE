# Task 1 Report

## Fix Round 1

### Fix Applied

`web/app.js` now wraps command arguments with shell-safe single-quote escaping and keeps embedded single quotes safe by closing/reopening the quoted segment.

## Fix Round 2

### Fix Applied

`web/app.js` now uses PowerShell single-quote escaping for command arguments:

- wrap the argument in outer single quotes
- double any embedded single quote characters

## Fix Round 3

### Fix Applied

`web/app.js` continues to use PowerShell-style shell quoting:

- outer single quotes around the argument
- embedded single quotes doubled to `''`
