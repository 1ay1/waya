# The `waya` CLI

waya ships a single launcher — like `npm` or `cargo` — so you never have to
remember the underlying CMake invocations. It's a small, dependency-free script
at the repo root (`./waya`), with a `waya.cmd` sibling for native Windows
`cmd.exe` / PowerShell.

```sh
./waya <command> [target] [options]
```

Put it on your `PATH` once and drop the `./`:

```sh
ln -s "$PWD/waya" ~/.local/bin/waya
waya run aurora
```

It runs everywhere: Linux, macOS, the BSDs, and Windows under
Git-Bash / MSYS2 / Cygwin / WSL (native cmd/PowerShell via `waya.cmd`).

## Commands

| Command | What it does |
|---------|--------------|
| `new <name> [dir]` | Scaffold a fresh, self-contained app (fetches waya via CMake). |
| `dev [target]` | Watch the source, rebuild on save, and live-reload the browser. No target opens an arrow-key picker. |
| `build [target]` | Configure (if needed) and build one target — no run. |
| `run [target]` | Build, then run the server. Alias: **`serve`**. |
| `list` | List the example targets you can `dev`/`run`/`build`. |
| `clean` | Remove the build directory. |
| `doctor` | Check your toolchain (cmake, C++26 compiler, file watcher). |
| `help` | Show usage. |

The `target` is an example name (`aurora`, `pulse`, `showcase`, …) or any
CMake target in your project. `waya list` prints the built-in examples.

## Options

These apply to `dev`, `build`, and `run`:

| Flag | Effect | Env equivalent |
|------|--------|----------------|
| `-d, --build-dir <dir>` | Build directory (default: `build`). | — |
| `-p, --port <n>` | Server port. | `WAYA_PORT` |
| `--host <addr>` | Bind address. | `WAYA_HOST` |
| `--no-open` | Don't auto-open the browser. | `WAYA_NO_OPEN` |
| `-j, --jobs <n>` | Parallel build jobs. | `JOBS` |

The environment variables work with a raw binary too, so
`WAYA_PORT=9000 ./build/aurora` behaves the same as `waya run aurora -p 9000`.

## Examples

```sh
# scaffold and run a brand-new app
waya new my-app && cd my-app && waya run

# live-reload development on a bundled example
waya dev showcase

# serve on a custom port without opening a browser
waya run aurora --port 9000 --no-open

# see what you can run, and check your toolchain
waya list
waya doctor
```

## Without the CLI

Everything the CLI does maps to plain CMake, if you prefer:

```sh
cmake -S . -B build
cmake --build build -j
./build/counter          # then open http://localhost:8080
```

The CLI just saves you from typing that — and adds the file-watching
live-reload loop (`waya dev`) on top.
