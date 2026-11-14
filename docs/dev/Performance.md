# Performance

ReqDeck publishes speed and memory figures together with how, where, and when they were
measured. Every number below can be reproduced with the scripts in the repository.

## Release 1.0.0

Measured 2026-11-13 on an Intel Core i5-10600 (3.3 GHz) with 32 GB RAM, running
Windows 11 Pro, from the release deployment `release/ReqDeck-1.0.0`.

| Measurement | Value |
|---|---|
| Cold start to main window (best of 5) | 944 ms |
| Private memory, idle after start | 155.9 MB |
| Workspace of 3,000 requests: save / load | 50 ms / 26 ms |
| Start with the 3,000-request workspace open | 924 ms, 167.3 MB |
| 20 MB JSON response: show / JSONPath filter `$[*].id` | 14 ms / 610 ms |
| 0.9 MB JSON response: pretty-print | 36 ms |
| 100 requests to a local server | 118 ms, memory growth −1.4 MB |

Package sizes: installer 90.8 MB, portable ZIP 72.0 MB.

## How it is measured

`packaging/measure.ps1` measures a deployment the way a user meets it:

1. It copies the deployment into a temporary folder and adds `portable.ini`, so that no
   data goes elsewhere.
2. **Cold start** is the time until the main window exists. The script takes the best of
   five starts, after one warm-up start.
3. **Idle memory** is the private memory five seconds after start.
4. `reqdeck_bench`, built with the Release configuration, measures the rest without a
   window:
   - saving and loading a workspace of 3,000 requests;
   - showing and filtering a 20 MB JSON response;
   - pretty-printing a response near the 1 MB display limit;
   - 100 requests to a local server.

The script removes its temporary folder and uploads nothing.

```powershell
powershell -NoProfile -File packaging/measure.ps1 -AppDir release/ReqDeck-1.0.0
```

## Design choices behind the numbers

- **Bodies larger than the display limit** are shown unformatted, so a huge response does
  not freeze the window.
- **Large responses** are kept in a temporary file instead of in memory, and saving writes
  the whole file.
- **Uploads are streamed** from disk.
- **JSONPath filtering** is linear in the size of the body, and its output is capped for
  display.

No other HTTP client has been measured on the same machine yet. A fair comparison would
need the same steps on the same hardware.
