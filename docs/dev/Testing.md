# Testing

ReqDeck has **46 CTest tests**: 44 unit test programs, one QML smoke test, and one screen
test program. They run on every push in the CI, and before every release.

## Kinds of tests

| Test | Program | What it covers |
|---|---|---|
| `<Class>.Unit` (41) | `tests/unit/tst_*.cpp` (Qt Test) | one class each: request model, variable resolver, builder, executor, cookies, OAuth client and token store, JSONPath, cURL converter, the three importers, YAML, workspace serializer and manager, secret store, inheritance, history, models for QML, help, update check, and more |
| `ReqDeck.QmlSmoke` | `tests/qml` (Qt Quick Test) | the `Base` controls, such as the menu bar, without the application |
| `ReqDeck.Screens` | `tests/screens` (Qt Quick Test, about 200 test functions) | the real screens with the real C++ services: launcher, tree, editor, response, dialogs, tabs, keyboard, focus, F1 help, themes |

`tests/bench` holds `reqdeck_bench`, a measurement program that is not part of CTest
(see [Performance](Performance)).

## No internet needed

The network tests never leave the computer. `tests/support/mockhttpserver` is a small
HTTP/1.1 server on `127.0.0.1`. It provides routes for:

- echo;
- streamed bytes;
- status codes;
- redirects and redirect loops;
- delays;
- cookies;
- repeated headers;
- body checksums;
- an OAuth token endpoint.

A self-signed test certificate in `tests/support/testcert` provides HTTPS that no system
trusts, to test the certificate warning. It protects nothing and is never part of an
application build.

## Running tests

All tests:

```powershell
ctest --preset windows-msvc2022-debug
```

Only the unit tests, or one test:

```powershell
ctest --preset windows-msvc2022-debug -R Unit
ctest --preset windows-msvc2022-debug -R JsonPath.Unit --output-on-failure
```

One test function of a unit test program, directly:

```powershell
build\windows-msvc2022-debug\Debug\tst_jsonpath.exe evaluatesRfcExamples
```

The Quick Test programs write their report to a file (`tst_qml.txt`, `tst_screens.txt` in
`build\<preset>\tests`), because they print nothing to the console there. The CI shows
the report when a screen test fails.

## Lint

```powershell
cmake --build --preset windows-msvc2022-debug --target all_qmllint
```

`qmllint` must report no warnings.

## Manual checks

Some things need a person and real servers: installing and uninstalling, SmartScreen, the
system browser for OAuth, real certificates and proxies, text rendering on a given monitor.
They are checked by hand before a release.
