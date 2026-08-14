# `os` plugin

An optional, real-I/O plugin library built from two primitives:

| Name | Token in | Token out | Does |
|------|----------|-----------|------|
| `os.print` | anything | same, unchanged | Writes the token to stdout |
| `os.exec` | a shell command | the command's stdout | Runs it via `sh -c`, captures output |
| `http.get` | a URL | the response body | `os.exec` with `curl` / `wget` / `python3` tried in order |
| `webhook` | a port number (default `8080`) | the raw request received | `os.exec` with `nc` / `python3` tried in order, blocks for one request |

`http.get` and `webhook` are not separate implementations — they are
`os.exec` given a generated command line, with a fallback list so the plugin
works with whatever happens to be installed. This is deliberately the
smallest thing that can be called "an HTTP client" and "a webhook listener":
shell out to whatever tool is already on the machine, in a sensible order,
rather than link a networking library.

## Why this is not built into `flowcode`

The `flowcode` CLI's default plugins are documented, and tested, to do no
I/O — see [`fc_builtins.h`](../../include/fc_builtins.h). This library is the
opposite: `os.exec` runs whatever text reaches it as a shell command. That is
correct behavior for an automation tool that wants to shell out — the same
thing an Ansible `shell:` task or a Node-RED `exec` node does — but it must
be an explicit choice, never a default a workflow gets by writing `http.get`.

Load it on purpose:

```c
fc_plugins_load(plugins, "./libfc_os.so");   /* or .dylib on macOS */
```

There is no CLI flag for this. `flowcode run` has no plugin argument by
design — see [Running Workflows](https://github.com/tayyebi/flowcode/wiki/Running-Workflows).
Loading a real plugin means embedding the runtime; see
[Plugins and Integrations](https://github.com/tayyebi/flowcode/wiki/Plugins-and-Integrations)
for a complete host program.

**Treat any workflow wired to this plugin as running arbitrary shell
commands with whatever data flows into it.** Do not load it for a workflow
built from untrusted input.

## Why the token is the whole argument

Flowcode's call ABI passes a plugin only the current token — `url = "..."` in
source is discarded by the compiler and never reaches a plugin (see the
[Language Guide](https://github.com/tayyebi/flowcode/wiki/Language-Guide#plugin-calls--do-outside-work)).
So every plugin here takes its one input as the token's text: `http.get`'s
token is the URL, `webhook`'s token is the port. And because a plugin call's
own result becomes the next token only when invoked as `transform` — a call
by dotted name (`http.get`) discards its result — write these as
`transform`, not as a bare call:

```
step u:
    emit
        value = "https://example.com"
end

step fetched:
    transform http.get      ← the response body becomes the token
end

step saved:
    store set
        key = "page"
end
```

## Building

```bash
make plugins
```

Produces `libfc_os.so` (Linux) or `libfc_os.dylib` (macOS) in this directory.
Not part of `make all`.

## Requirements

`os.exec` and `os.print` need nothing beyond a POSIX shell. `http.get` needs
one of `curl`, `wget`, or `python3` on `PATH`; `webhook` needs `nc` or
`python3`. All are tried in order and the first one that runs successfully
wins — if none are present, the call fails with `FC_ERR_PLUGIN_CALL`.

## `webhook` is single-shot

A Flowcode run is a single pass through a program, not a long-lived server.
`webhook` blocks until exactly one request arrives, captures it, and returns
— it does not keep listening. Run the workflow again to accept another
request, or drive it from a process supervisor that restarts it per request.

## Safety notes

- URLs and ports are shell-quoted before being embedded in a generated
  command line (`shell_quote` in `os_plugins.c`), so a hostile token cannot
  break out of its argument and run something else. `os.exec` itself has no
  such protection — its entire contract is "run this text as a command".
- Output is capped at 1 MiB and commands/arguments at a few KB; both are
  hard ceilings, not configurable, to keep a single call bounded.
- Every exec call heap-allocates its output for the lifetime of the run and
  never frees it — matching the plugin ABI's requirement that a token's
  value outlive the run. Acceptable for a short CLI process; an embedder
  running many workflows in one long-lived process should not reuse this
  plugin unmodified.
