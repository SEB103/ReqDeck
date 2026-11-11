# yaml-cpp (vendored)

- **Version:** 0.9.0 (tag `yaml-cpp-0.9.0`, released 2026-02-04)
- **Source:** <https://github.com/jbeder/yaml-cpp/releases/tag/yaml-cpp-0.9.0>, the
  release source archive `yaml-cpp-yaml-cpp-0.9.0.tar.gz`
  (SHA-256 `25cb043240f828a8c51beb830569634bc7ac603978e0f69d6b63558dadefd49a`)
- **License:** MIT (`LICENSE`, `LICENSES/MIT.txt`). `src/contrib/dragonbox.h` is
  offered by its author under several licenses, MIT among them, which is the one
  used here.

## What is kept

`include/`, `src/`, `LICENSE`, and `README.md`, unmodified. The graph builder
sources of `src/contrib/` and the natvis files are left out, since ReqDeck does
not use them; the headers of `include/yaml-cpp/contrib/` remain but are not used.
Tests, documentation, and the upstream build files are left out.

ReqDeck builds the library with its own target in `third_party/CMakeLists.txt`,
as a static library linked into the core library. Nothing of it is installed
apart from the license texts.

## Updating

Replace `include/` and `src/` with those of the new release, drop the same
`src/contrib/` files, update this file and `THIRD_PARTY_NOTICES.md`, and run the
YAML tests (`YamlJson.Unit`).
