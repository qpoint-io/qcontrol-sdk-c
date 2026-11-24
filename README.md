# Qcontrol C SDK

Intercept. Observe. Transform. Build secure sandboxes for AI agents and control any application from the inside.

---

**Overview:** [Introduction](#introduction) · [What Can You Build?](#what-can-you-build) · [Quick Start](#quick-start) · [Core Concepts](#core-concepts) · [Examples](#examples)

**API Reference:** [File Operations](#file-operations) · [Exec Operations](#exec-operations) · [Network Operations](#network-operations) · [HTTP Operations](#http-operations) · [SSE Operations](#sse-operations) · [WebSocket Operations](#websocket-operations) · [Configuration](#configuration)

**Development:** [Building Plugins](#building-plugins) · [Running Plugins](#running-plugins) · [Environment Variables](#environment-variables)

---

## Introduction

The Qcontrol C SDK lets you build plugins that hook directly into system calls and host-backed protocol surfaces, giving you precise control over host-observed file, process, network, and HTTP surfaces.

This makes Qcontrol ideal for building **AI agent sandboxes and runtimes**. As agents gain autonomy to read files, execute commands, and make network requests, you need visibility and control at the syscall level. Qcontrol gives you that:

- **Intercept file operations** — See every open, read, write, and close. Block access to sensitive paths or transform data as it flows through.
- **Intercept exec operations** — Monitor process spawning, modify arguments, capture stdin/stdout/stderr.
- **Intercept network operations** — Watch connections form, inspect send/recv traffic, detect TLS and protocols.
- **Intercept HTTP operations** — Inspect normalized request/response metadata, edit headers inline, and rewrite buffered bodies through host-backed HTTP callbacks.
- **Intercept stream protocols above HTTP** — Observe parsed SSE records and logical WebSocket messages with shared connection/exchange correlation.

Your plugins load in qcontrol host processes (qproxy, qmcp). Observe silently, block operations, or transform data on host-backed event paths.

The SDK provides a clean C API with convenience macros for common patterns.

## What Can You Build?

| Category | What You Can Do | Example Plugin |
|----------|-----------------|----------------|
| **AI Agent Sandboxes** | Constrain file access, limit network destinations, audit all agent actions | [access-control](examples/access-control/) |
| **Agent Runtimes** | Build secure execution environments with fine-grained syscall control | [file-logger](examples/file-logger/) |
| **Security** | Enforce allowlists, block sensitive paths, create application sandboxes | [access-control](examples/access-control/) |
| **Observability** | Trace all I/O, log syscalls, build audit trails, count bytes | [file-logger](examples/file-logger/), [byte-counter](examples/byte-counter/) |
| **Compliance** | Redact PII from output, mask credentials, filter sensitive data | [content-filter](examples/content-filter/) |
| **Development** | Mock file systems, inject test data, transform formats on the fly | [text-transform](examples/text-transform/) |
| **HTTP Governance** | Observe exchanges, normalize headers, rewrite API responses inline | [http-logger](examples/http-logger/), [http-rewrite](examples/http-rewrite/) |
| **Streaming APIs** | Observe or terminate SSE/WebSocket flows and attach stream-scoped state | Public `qcontrol/sse.h`, `qcontrol/websocket.h`, `qcontrol/stream.h` |

## Quick Start

A plugin that logs every file open:

```c
#include <qcontrol.h>
#include <stdio.h>

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    fprintf(stderr, "open: %.*s\n", (int)ev->path_len, ev->path);
    return QCONTROL_FILE_PASS;
}

const qcontrol_plugin_t qcontrol_plugin = {
    .version = QCONTROL_PLUGIN_VERSION,
    .name = "hello-plugin",
    .on_file_open = on_file_open,
};
```

Build a shared library using the Makefile pattern shown in
[Building Plugins](#building-plugins), then list it in a behavior's `plugins`
in `config.yaml`:

```yaml
behaviors:
  hello:
    file:
      enabled: true
    plugins:
      - name: hello-plugin
        path: /absolute/path/to/libhello_plugin.so
```

A behavior that loads a path plugin states the surfaces it uses; this plugin
hooks file opens, so the behavior sets `file.enabled`.

Select that behavior when you run a command:

```bash
qcontrol run --behavior hello -- cat /etc/passwd
```

That's it. Your plugin now intercepts every file open in the instrumented
process.

## Core Concepts

### Hooks

Qcontrol intercepts operations at four levels:

| Domain | Operations | Status |
|--------|------------|--------|
| **File** | open, read, write, close | Implemented in the native run/tap host |
| **Exec** | spawn, stdin, stdout, stderr, exit | Implemented by the explicit exec host |
| **Network** | connect, accept, send, recv, close | Implemented by the proxy-backed run path |
| **HTTP** | request, request body, request trailers, request done, response, response body, response trailers, response done, exchange close | Implemented by the proxy-backed run path |

### Actions

Every callback returns an action that controls what happens next:

```c
// Let the operation proceed normally
return QCONTROL_FILE_PASS;

// Block with EACCES
return QCONTROL_FILE_BLOCK;

// Block with custom errno
return QCONTROL_FILE_BLOCK_WITH(ENOENT);

// Track custom state for this file
return QCONTROL_FILE_STATE(my_state_ptr);
```

### Sessions

Sessions are where Qcontrol gets powerful. Return a session from your open callback to configure automatic transforms:

```c
static qcontrol_file_rw_config_t read_config = {
    .prefix = "[LOG] ",
    .prefix_len = 6,
    .suffix = "\n",
    .suffix_len = 1,
    .replace = patterns,
    .replace_count = 2,
    .transform = my_custom_transform,
};

static qcontrol_file_session_t session = {
    .state = NULL,
    .read = &read_config,
};

return (qcontrol_file_action_t){
    .type = QCONTROL_FILE_ACTION_SESSION,
    .session = session,
};
```

**Transform pipeline:** `prefix` → `replace` → `transform` → `suffix`

### Progressive Examples

**Observe** — Log operations without affecting them:

```c
static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    fprintf(stderr, "open(%.*s) = %d\n", (int)ev->path_len, ev->path, ev->result);
    return QCONTROL_FILE_PASS;  // No interception
}
```

**Block** — Deny access to specific paths:

```c
static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    if (ev->path_len >= 11 && strncmp(ev->path, "/tmp/secret", 11) == 0) {
        return QCONTROL_FILE_BLOCK;  // EACCES
    }
    return QCONTROL_FILE_PASS;
}
```

**Transform** — Modify data in transit:

```c
static qcontrol_file_pattern_t patterns[] = {
    { "ERROR", 5, "[REDACTED]", 10 },
};

static qcontrol_file_rw_config_t read_config = {
    .replace = patterns,
    .replace_count = 1,
};

static qcontrol_file_session_t session = {
    .read = &read_config,
};

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    // Check if it's a .log file
    if (ev->path_len > 4 && strcmp(ev->path + ev->path_len - 4, ".log") == 0) {
        return (qcontrol_file_action_t){
            .type = QCONTROL_FILE_ACTION_SESSION,
            .session = session,
        };
    }
    return QCONTROL_FILE_PASS;
}
```

## Examples

| Plugin | Description | Key Concepts |
|--------|-------------|--------------|
| [file-logger](examples/file-logger/) | Logs all file operations to `/tmp/qcontrol.log` | Basic callbacks, Logger utility |
| [access-control](examples/access-control/) | Blocks access to `/tmp/secret*` paths | Blocking with `QCONTROL_FILE_BLOCK` |
| [byte-counter](examples/byte-counter/) | Tracks total bytes read and written | State tracking with `QCONTROL_FILE_STATE` |
| [content-filter](examples/content-filter/) | Redacts "password" and "secret" from reads | Sessions with pattern replacement |
| [text-transform](examples/text-transform/) | Uppercases all file reads | Custom transform functions |
| [exec-logger](examples/exec-logger/) | Logs process spawns and exits | Exec API |
| [net-logger](examples/net-logger/) | Logs network connections and traffic | Network API |
| [net-transform](examples/net-transform/) | Rewrites plaintext network traffic | Network transform configuration |
| [http-logger](examples/http-logger/) | Logs structured HTTP exchange lifecycle events | HTTP callbacks, per-exchange state |
| [http-rewrite](examples/http-rewrite/) | Rewrites JSON responses with inline head/body edits | Buffered body scheduling, mutable headers |

## File Operations

File operations are fully implemented. Use these to observe, block, or transform file I/O.

### Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_file_open` | `qcontrol_file_action_t (*)(qcontrol_file_open_event_t*)` | After open()/openat() | Decide interception |
| `on_file_read` | `qcontrol_file_action_t (*)(void*, qcontrol_file_read_event_t*)` | After read() | Observe or block |
| `on_file_write` | `qcontrol_file_action_t (*)(void*, qcontrol_file_write_event_t*)` | Before write() | Observe or block |
| `on_file_close` | `void (*)(void*, qcontrol_file_close_event_t*)` | After close() | Cleanup state |
| `on_file_op` | `qcontrol_file_op_action_t (*)(qcontrol_file_op_event_t*)` | Path operations outside the fd stream lifecycle | Allow or deny metadata / mutation operations |

The `void*` state parameter is your custom state returned from `on_file_open`.

### Events

**qcontrol_file_open_event_t** — passed to `on_file_open`:

| Field | Type | Description |
|-------|------|-------------|
| `path` | `const char*` | File path being opened |
| `path_len` | `size_t` | Length of path |
| `flags` | `int` | Open flags (O_RDONLY, O_WRONLY, etc.) |
| `mode` | `unsigned int` | File mode (for O_CREAT) |
| `result` | `int` | Result fd on success, -errno on failure |

**qcontrol_file_read_event_t** — passed to `on_file_read`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | File descriptor |
| `buf` | `void*` | Buffer containing read data |
| `count` | `size_t` | Requested byte count |
| `result` | `ssize_t` | Bytes read or -errno |

**qcontrol_file_write_event_t** — passed to `on_file_write`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | File descriptor |
| `buf` | `const void*` | Buffer containing data to write |
| `count` | `size_t` | Byte count to write |
| `result` | `ssize_t` | Bytes written or -errno |

**qcontrol_file_close_event_t** — passed to `on_file_close`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | File descriptor |
| `result` | `int` | Result (0 or -errno) |

**qcontrol_file_op_event_t** — passed to `on_file_op`:

| Field | Type | Description |
|-------|------|-------------|
| `kind` | `qcontrol_file_op_kind_t` | Filesystem operation kind such as `QCONTROL_FILE_OP_STAT` or `QCONTROL_FILE_OP_RENAME` |
| `policy_class` | `qcontrol_file_policy_class_t` | Coarse OpenShell-style read/write bucket |
| `path` | `const char*` | Primary path argument |
| `path_len` | `size_t` | Length of path |
| `secondary_path` | `const char*` | Secondary path for two-path operations like rename/link/symlink |
| `secondary_path_len` | `size_t` | Length of secondary_path |
| `dirfd` | `int` | Primary-path `dirfd` context for `*at`-style operations, or `-1` |
| `at_flags` | `int` | `AT_*` flags or `0` |
| `result` | `int` | Result (0 or -errno) |
| `secondary_dirfd` | `int` | Secondary-path `dirfd` context for two-path `*at` operations, or `-1` |

### Actions

**Action macros** — return from callbacks:

| Macro | Description |
|-------|-------------|
| `QCONTROL_FILE_PASS` | No interception, continue normally |
| `QCONTROL_FILE_BLOCK` | Block with EACCES |
| `QCONTROL_FILE_BLOCK_WITH(errno)` | Block with custom errno |
| `QCONTROL_FILE_STATE(ptr)` | Track state only, no transforms |

**Path-operation action macros** — return from `on_file_op`:

| Macro | Description |
|-------|-------------|
| `QCONTROL_FILE_OP_PASS` | Allow the path operation |
| `QCONTROL_FILE_OP_BLOCK` | Block with EACCES |
| `QCONTROL_FILE_OP_BLOCK_WITH(errno)` | Block with custom errno |

For sessions, return a `qcontrol_file_action_t` with `.type = QCONTROL_FILE_ACTION_SESSION`.

### Sessions and Transforms

Return a session from `on_file_open` to configure read/write transforms:

```c
static qcontrol_file_pattern_t patterns[] = {
    { "password", 8, "********", 8 },
    { "secret", 6, "[REDACTED]", 10 },
};

static qcontrol_file_rw_config_t read_config = {
    .prefix = "[LOG] ",
    .prefix_len = 6,
    .suffix = "\n",
    .suffix_len = 1,
    .replace = patterns,
    .replace_count = 2,
    .transform = my_transform_fn,
};

static qcontrol_file_session_t session = {
    .state = my_state,           // Optional: custom state pointer
    .read = &read_config,        // Optional: read transform config
    .write = NULL,               // Optional: write transform config
};

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    if (ev->result < 0) return QCONTROL_FILE_PASS;

    return (qcontrol_file_action_t){
        .type = QCONTROL_FILE_ACTION_SESSION,
        .session = session,
    };
}
```

**qcontrol_file_rw_config_t fields:**

| Field | Type | Description |
|-------|------|-------------|
| `prefix` | `const char*` | Static prefix to prepend |
| `prefix_len` | `size_t` | Length of prefix |
| `suffix` | `const char*` | Static suffix to append |
| `suffix_len` | `size_t` | Length of suffix |
| `prefix_fn` | `qcontrol_file_prefix_fn` | Dynamic prefix function |
| `suffix_fn` | `qcontrol_file_suffix_fn` | Dynamic suffix function |
| `replace` | `const qcontrol_file_pattern_t*` | Pattern replacements array |
| `replace_count` | `size_t` | Number of patterns |
| `transform` | `qcontrol_file_transform_fn` | Custom transform function |

**Transform pipeline order:** `prefix` → `replace` → `transform` → `suffix`

**Custom transform function:**

```c
static qcontrol_file_action_t my_transform(
    void* state,
    qcontrol_file_ctx_t* ctx,
    qcontrol_buffer_t* buf
) {
    // ctx provides: fd, path, path_len, flags
    // buf provides: buffer manipulation functions

    if (qcontrol_buffer_contains(buf, "sensitive", 9)) {
        qcontrol_buffer_replace_all(buf, "sensitive", 9, "[HIDDEN]", 8);
    }

    return QCONTROL_FILE_PASS;  // or QCONTROL_FILE_BLOCK
}
```

**Dynamic prefix/suffix functions:**

```c
static const char* dynamic_prefix(
    void* state,
    qcontrol_file_ctx_t* ctx,
    size_t* out_len
) {
    if (ctx->path_len > 4 &&
        strcmp(ctx->path + ctx->path_len - 4, ".log") == 0) {
        *out_len = 6;
        return "[LOG] ";
    }
    *out_len = 0;
    return NULL;  // No prefix
}
```

### Buffer API

The `qcontrol_buffer_t` type provides functions for inspecting and modifying data:

**Read operations:**

| Function | Description |
|----------|-------------|
| `qcontrol_buffer_len(buf)` | Get buffer length |
| `qcontrol_buffer_data(buf)` | Get pointer to buffer data |
| `qcontrol_buffer_contains(buf, needle, len)` | Check if buffer contains needle (returns 1/0) |
| `qcontrol_buffer_starts_with(buf, prefix, len)` | Check if buffer starts with prefix |
| `qcontrol_buffer_ends_with(buf, suffix, len)` | Check if buffer ends with suffix |
| `qcontrol_buffer_index_of(buf, needle, len)` | Find position of needle (SIZE_MAX if not found) |

**Write operations:**

| Function | Description |
|----------|-------------|
| `qcontrol_buffer_prepend(buf, data, len)` | Add data to beginning |
| `qcontrol_buffer_append(buf, data, len)` | Add data to end |
| `qcontrol_buffer_replace(buf, needle, nlen, repl, rlen)` | Replace first occurrence (returns 1/0) |
| `qcontrol_buffer_replace_all(buf, needle, nlen, repl, rlen)` | Replace all occurrences (returns count) |
| `qcontrol_buffer_remove(buf, needle, len)` | Remove first occurrence (returns 1/0) |
| `qcontrol_buffer_remove_all(buf, needle, len)` | Remove all occurrences (returns count) |
| `qcontrol_buffer_clear(buf)` | Clear buffer contents |
| `qcontrol_buffer_set(buf, data, len)` | Replace entire buffer contents |
| `qcontrol_buffer_insert_at(buf, pos, data, len)` | Insert data at position |
| `qcontrol_buffer_remove_range(buf, start, end)` | Remove byte range |

### Patterns

Define patterns as a static array:

```c
static qcontrol_file_pattern_t patterns[] = {
    { "password", 8, "********", 8 },
    { "secret", 6, "[REDACTED]", 10 },
    { "api_key", 7, "[HIDDEN]", 8 },
};

static qcontrol_file_rw_config_t read_config = {
    .replace = patterns,
    .replace_count = sizeof(patterns) / sizeof(patterns[0]),
};
```

## Exec Operations

> **Note:** Agent support coming soon. The SDK is stable, so you can write plugins now.

### Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_exec` | `qcontrol_exec_action_t (*)(qcontrol_exec_event_t*)` | Before exec | Decide interception |
| `on_exec_stdin` | `qcontrol_exec_action_t (*)(void*, qcontrol_exec_stdin_event_t*)` | Before stdin write | Observe or block |
| `on_exec_stdout` | `qcontrol_exec_action_t (*)(void*, qcontrol_exec_stdout_event_t*)` | After stdout read | Observe or block |
| `on_exec_stderr` | `qcontrol_exec_action_t (*)(void*, qcontrol_exec_stderr_event_t*)` | After stderr read | Observe or block |
| `on_exec_exit` | `void (*)(void*, qcontrol_exec_exit_event_t*)` | After exit | Cleanup state |

### Events

**qcontrol_exec_event_t** — passed to `on_exec`:

| Field | Type | Description |
|-------|------|-------------|
| `path` | `const char*` | Executable path |
| `path_len` | `size_t` | Length of path |
| `argv` | `const char* const*` | Arguments (NULL-terminated) |
| `argc` | `size_t` | Argument count |
| `envp` | `const char* const*` | Environment (NULL-terminated) |
| `envc` | `size_t` | Environment variable count |
| `cwd` | `const char*` | Working directory (may be NULL) |
| `cwd_len` | `size_t` | Length of cwd |

**qcontrol_exec_stdin_event_t** — passed to `on_exec_stdin`:

| Field | Type | Description |
|-------|------|-------------|
| `pid` | `pid_t` | Child process ID |
| `buf` | `const void*` | Data being written to stdin |
| `count` | `size_t` | Byte count |

**qcontrol_exec_stdout_event_t** / **qcontrol_exec_stderr_event_t**:

| Field | Type | Description |
|-------|------|-------------|
| `pid` | `pid_t` | Child process ID |
| `buf` | `void*` | Buffer containing data read |
| `count` | `size_t` | Requested byte count |
| `result` | `ssize_t` | Bytes read or -errno |

**qcontrol_exec_exit_event_t** — passed to `on_exec_exit`:

| Field | Type | Description |
|-------|------|-------------|
| `pid` | `pid_t` | Child process ID |
| `exit_code` | `int` | Exit code (if normal exit) |
| `exit_signal` | `int` | Signal number (0 if normal) |

### Actions

**Action macros** — return from callbacks:

| Macro | Description |
|-------|-------------|
| `QCONTROL_EXEC_PASS` | No interception |
| `QCONTROL_EXEC_BLOCK` | Block with EACCES |
| `QCONTROL_EXEC_BLOCK_WITH(errno)` | Block with custom errno |
| `QCONTROL_EXEC_STATE(ptr)` | Track state only |

### Sessions

```c
static const char* set_argv[] = { "wrapper", "--safe", NULL };
static const char* set_env[] = { "SAFE_MODE=1", NULL };
static const char* unset_env[] = { "DEBUG", NULL };

static qcontrol_exec_rw_config_t stdout_config = {
    .prefix = "[OUT] ",
    .prefix_len = 6,
};

static qcontrol_exec_session_t session = {
    .state = my_state,

    // Exec modifications
    .set_path = "/usr/bin/safe-wrapper",
    .set_argv = set_argv,
    .prepend_argv = NULL,
    .append_argv = NULL,
    .set_env = set_env,
    .unset_env = unset_env,
    .set_cwd = "/tmp/sandbox",
    .set_uid = 1001,
    .set_gid = 1001,
    .has_set_uid = 1,
    .has_set_gid = 1,

    // I/O transforms
    .stdin_config = NULL,
    .stdout_config = &stdout_config,
    .stderr_config = NULL,
};

static qcontrol_exec_action_t on_exec(qcontrol_exec_event_t* ev) {
    return (qcontrol_exec_action_t){
        .type = QCONTROL_EXEC_ACTION_SESSION,
        .session = session,
    };
}
```

## Network Operations

> **Note:** Proxy-backed `qcontrol run` already exercises this ABI today. Native agent-side network hooks are still coming.

### Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_net_connect` | `qcontrol_net_action_t (*)(qcontrol_net_connect_event_t*)` | After connect() | Decide interception |
| `on_net_accept` | `qcontrol_net_action_t (*)(qcontrol_net_accept_event_t*)` | After accept() | Decide interception |
| `on_net_tls` | `qcontrol_net_phase_action_t (*)(void*, qcontrol_net_tls_event_t*)` | After TLS handshake | Observe or terminate |
| `on_net_domain` | `qcontrol_net_phase_action_t (*)(void*, qcontrol_net_domain_event_t*)` | Domain discovered | Observe or terminate |
| `on_net_protocol` | `qcontrol_net_phase_action_t (*)(void*, qcontrol_net_protocol_event_t*)` | Protocol detected | Observe or terminate |
| `on_net_send` | `qcontrol_net_action_t (*)(void*, qcontrol_net_send_event_t*)` | Before send | Observe or block |
| `on_net_recv` | `qcontrol_net_action_t (*)(void*, qcontrol_net_recv_event_t*)` | After recv | Observe or block |
| `on_net_close` | `void (*)(void*, qcontrol_net_close_event_t*)` | After close | Cleanup state |

### Events

**qcontrol_net_connect_event_t** — passed to `on_net_connect`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket file descriptor |
| `dst_addr` | `const char*` | Destination IP address |
| `dst_addr_len` | `size_t` | Length of dst_addr |
| `dst_port` | `uint16_t` | Destination port |
| `src_addr` | `const char*` | Local source address (may be NULL) |
| `src_addr_len` | `size_t` | Length of src_addr |
| `src_port` | `uint16_t` | Local source port |
| `result` | `int` | 0 on success, -errno on failure |
| `process` | `qcontrol_process_identity_t` | Originating process identity and ancestor executable lineage |

**qcontrol_net_accept_event_t** — passed to `on_net_accept`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Accepted socket fd |
| `listen_fd` | `int` | Listening socket fd |
| `src_addr` | `const char*` | Remote client address |
| `src_addr_len` | `size_t` | Length of src_addr |
| `src_port` | `uint16_t` | Remote client port |
| `dst_addr` | `const char*` | Local server address |
| `dst_addr_len` | `size_t` | Length of dst_addr |
| `dst_port` | `uint16_t` | Local server port |
| `result` | `int` | fd on success, -errno on failure |

**qcontrol_net_tls_event_t** — passed to `on_net_tls`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `version` | `const char*` | TLS version (e.g., "TLSv1.3") |
| `version_len` | `size_t` | Length of version |
| `cipher` | `const char*` | Cipher suite (may be NULL) |
| `cipher_len` | `size_t` | Length of cipher |

**qcontrol_net_domain_event_t** — passed to `on_net_domain`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `domain` | `const char*` | Domain name |
| `domain_len` | `size_t` | Length of domain |

**qcontrol_net_protocol_event_t** — passed to `on_net_protocol`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `protocol` | `const char*` | Protocol (e.g., "http/1.1", "h2") |
| `protocol_len` | `size_t` | Length of protocol |

**qcontrol_net_send_event_t** — passed to `on_net_send`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `buf` | `const void*` | Data being sent |
| `count` | `size_t` | Byte count |

**qcontrol_net_recv_event_t** — passed to `on_net_recv`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `buf` | `void*` | Buffer containing received data |
| `count` | `size_t` | Requested byte count |
| `result` | `ssize_t` | Bytes received or -errno |

**qcontrol_net_close_event_t** — passed to `on_net_close`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `result` | `int` | 0 on success, -errno on failure |

### Actions

**Action macros** — return from callbacks:

| Macro | Description |
|-------|-------------|
| `QCONTROL_NET_PASS` | No interception |
| `QCONTROL_NET_BLOCK` | Block with EACCES |
| `QCONTROL_NET_BLOCK_WITH(errno)` | Block with custom errno |
| `QCONTROL_NET_STATE(ptr)` | Track state only |

**Late-phase action macros** — return from `on_net_tls`, `on_net_domain`, and `on_net_protocol`:

| Macro | Description |
|-------|-------------|
| `QCONTROL_NET_PHASE_PASS` | Keep the tracked connection open |
| `QCONTROL_NET_PHASE_BLOCK` | Terminate the tracked connection |
| `QCONTROL_NET_PHASE_BLOCK_WITH(errno)` | Terminate with a host-specific errno when supported |

### Sessions

```c
static qcontrol_net_rw_config_t recv_config = {
    .prefix = "[RECV] ",
    .prefix_len = 7,
};

static qcontrol_net_session_t session = {
    .state = my_state,

    // Connection modifications (connect only)
    .set_addr = "proxy.example.com",
    .set_port = 8080,
    .inspection_mode = QCONTROL_NET_INSPECTION_INTERCEPT,

    // I/O transforms
    .send_config = NULL,
    .recv_config = &recv_config,
};

static qcontrol_net_action_t on_net_connect(qcontrol_net_connect_event_t* ev) {
    return (qcontrol_net_action_t){
        .type = QCONTROL_NET_ACTION_SESSION,
        .session = session,
    };
}
```

### Context

The `qcontrol_net_ctx_t` type in transform functions provides connection metadata:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `direction` | `qcontrol_net_direction_t` | `QCONTROL_NET_OUTBOUND` or `QCONTROL_NET_INBOUND` |
| `src_addr` | `const char*` | Source address |
| `src_addr_len` | `size_t` | Length of src_addr |
| `src_port` | `uint16_t` | Source port |
| `dst_addr` | `const char*` | Destination address |
| `dst_addr_len` | `size_t` | Length of dst_addr |
| `dst_port` | `uint16_t` | Destination port |
| `is_tls` | `int` | Whether TLS is active (1/0) |
| `tls_version` | `const char*` | TLS version (may be NULL) |
| `tls_version_len` | `size_t` | Length of tls_version |
| `tls_cipher` | `const char*` | TLS cipher suite (may be NULL) |
| `tls_cipher_len` | `size_t` | Length of tls_cipher |
| `domain` | `const char*` | Domain name (may be NULL) |
| `domain_len` | `size_t` | Length of domain |
| `protocol` | `const char*` | Protocol (may be NULL) |
| `protocol_len` | `size_t` | Length of protocol |
| `process` | `qcontrol_process_identity_t` | Originating process identity and ancestor executable lineage |

## HTTP Operations

> **Note:** HTTP-aware hosts emit this ABI directly today. Inline request/response edits are host-backed, so mutable head/body handles may be `NULL` when a host only supports observation.

### Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_http_request` | `qcontrol_http_action_t (*)(qcontrol_http_request_event_t*)` | Request start | Observe, block, attach exchange state, choose request body scheduling |
| `on_http_request_body` | `qcontrol_http_action_t (*)(void*, qcontrol_http_body_event_t*)` | Request body | Observe or edit decoded request body |
| `on_http_request_trailers` | `qcontrol_http_action_t (*)(void*, qcontrol_http_trailers_event_t*)` | Request trailers | Observe or edit request trailers |
| `on_http_request_done` | `void (*)(void*, qcontrol_http_message_done_event_t*)` | Request completion | Bookkeeping after request body/trailers |
| `on_http_response` | `qcontrol_http_action_t (*)(void*, qcontrol_http_response_event_t*)` | Response start | Observe, block, edit response head, choose response body scheduling |
| `on_http_response_body` | `qcontrol_http_action_t (*)(void*, qcontrol_http_body_event_t*)` | Response body | Observe or edit decoded response body |
| `on_http_response_trailers` | `qcontrol_http_action_t (*)(void*, qcontrol_http_trailers_event_t*)` | Response trailers | Observe or edit response trailers |
| `on_http_response_done` | `void (*)(void*, qcontrol_http_message_done_event_t*)` | Response completion | Bookkeeping after response body/trailers |
| `on_http_exchange_close` | `void (*)(void*, qcontrol_http_exchange_close_event_t*)` | Exchange close | Final cleanup for tracked exchange state |

The `void*` state parameter is your custom per-exchange state returned from `on_http_request` or `on_http_response`.

### Events

**qcontrol_http_request_event_t** — passed to `on_http_request`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_http_ctx_t` | Exchange context, including `exchange_id`, `stream_id`, HTTP version, and underlying `qcontrol_net_ctx_t` snapshot |
| `raw_target` | `const char*` | Raw request-target as seen on the wire |
| `raw_target_len` | `size_t` | Length of raw target |
| `method` | `const char*` | Normalized request method |
| `method_len` | `size_t` | Length of method |
| `scheme` | `const char*` | Normalized request scheme, or `NULL` if unavailable |
| `scheme_len` | `size_t` | Length of scheme |
| `authority` | `const char*` | Normalized request authority, or `NULL` if unavailable |
| `authority_len` | `size_t` | Length of authority |
| `path` | `const char*` | Normalized request path |
| `path_len` | `size_t` | Length of path |
| `headers` | `const qcontrol_http_header_t*` | Runtime-owned request header array |
| `header_count` | `size_t` | Number of request headers |
| `head` | `qcontrol_http_request_head_t*` | Mutable request head handle for inline edits, or `NULL` |

**qcontrol_http_response_event_t** — passed to `on_http_response`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_http_ctx_t` | Exchange context |
| `status_code` | `uint16_t` | Response status code |
| `reason` | `const char*` | Reason phrase, or `NULL` if unavailable |
| `reason_len` | `size_t` | Length of reason phrase |
| `headers` | `const qcontrol_http_header_t*` | Runtime-owned response header array |
| `header_count` | `size_t` | Number of response headers |
| `head` | `qcontrol_http_response_head_t*` | Mutable response head handle for inline edits, or `NULL` |

**qcontrol_http_body_event_t** — passed to request/response body callbacks:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_http_ctx_t` | Exchange context |
| `kind` | `qcontrol_http_message_kind_t` | `QCONTROL_HTTP_MESSAGE_REQUEST` or `QCONTROL_HTTP_MESSAGE_RESPONSE` |
| `bytes` | `const char*` | Read-only decoded input bytes for this callback |
| `bytes_len` | `size_t` | Length of `bytes` |
| `body` | `qcontrol_buffer_t*` | Mutable output buffer for host-backed body edits, or `NULL` |
| `offset` | `uint64_t` | Decoded body offset within the current message |
| `flags` | `uint32_t` | `qcontrol_http_body_flag_t` bitset |
| `end_of_stream` | `int` | Non-zero on the terminal body callback for this message |

**qcontrol_http_trailers_event_t** — passed to request/response trailers callbacks:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_http_ctx_t` | Exchange context |
| `kind` | `qcontrol_http_message_kind_t` | Request or response trailers |
| `headers` | `const qcontrol_http_header_t*` | Runtime-owned trailer array |
| `header_count` | `size_t` | Number of trailers |
| `header_block` | `qcontrol_http_headers_t*` | Mutable trailer block for host-backed edits, or `NULL` |

**qcontrol_http_message_done_event_t** — passed to request/response done callbacks:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_http_ctx_t` | Exchange context |
| `kind` | `qcontrol_http_message_kind_t` | Completed request or response |
| `body_bytes` | `uint64_t` | Total decoded body bytes observed for the completed message |

**qcontrol_http_exchange_close_event_t** — passed to `on_http_exchange_close`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_http_ctx_t` | Exchange context |
| `reason` | `qcontrol_http_close_reason_t` | Terminal close reason |
| `flags` | `uint32_t` | `qcontrol_http_exchange_flag_t` bitset indicating which done callbacks ran |

### Actions

**Action macros** — return from HTTP callbacks:

| Macro | Description |
|-------|-------------|
| `QCONTROL_HTTP_PASS` | No interception, continue normally |
| `QCONTROL_HTTP_BLOCK` | Block the exchange |
| `QCONTROL_HTTP_STATE(ptr)` | Track per-exchange state only |
| `QCONTROL_HTTP_BUFFER(action)` | Request buffered decoded body delivery for this request or response |
| `QCONTROL_HTTP_STREAM(action)` | Request incremental decoded body delivery for this request or response |

### Inline Head And Body Edits

Use the mutable request/response head handles to edit normalized HTTP metadata when the host provides them:

```c
static qcontrol_http_action_t on_http_request(qcontrol_http_request_event_t* ev) {
    qcontrol_http_headers_t* headers = NULL;

    if (ev->head != NULL) {
        qcontrol_http_request_set_path(ev->head, QCONTROL_STR("/v2/profile"));
        headers = qcontrol_http_request_headers(ev->head);
        if (headers != NULL) {
            QCONTROL_HTTP_HEADER_REMOVE(headers, "proxy-connection");
            QCONTROL_HTTP_HEADER_SET(headers, "x-qcontrol", "1");
        }
    }

    return QCONTROL_HTTP_PASS;
}
```

Response callbacks can request buffered scheduling early and then replace the final logical body in the later body callback:

```c
static qcontrol_http_action_t on_http_response(void* state, qcontrol_http_response_event_t* ev) {
    (void)state;
    (void)ev;
    return QCONTROL_HTTP_BUFFER(QCONTROL_HTTP_PASS);
}

static qcontrol_http_action_t on_http_response_body(void* state, qcontrol_http_body_event_t* ev) {
    static const char replacement[] = "{\"rewritten\":true}";
    (void)state;

    if (ev->end_of_stream && ev->body != NULL) {
        qcontrol_buffer_set(ev->body, replacement, sizeof(replacement) - 1);
    }
    return QCONTROL_HTTP_PASS;
}
```

Available request head accessors and setters:

| API | Description |
|-----|-------------|
| `qcontrol_http_request_headers(head)` | Mutable request headers handle |
| `qcontrol_http_request_raw_target(head)` / `_len(head)` | Current raw target |
| `qcontrol_http_request_method(head)` / `_len(head)` | Current method |
| `qcontrol_http_request_set_method(head, value, value_len)` | Replace method |
| `qcontrol_http_request_scheme(head)` / `_len(head)` | Current scheme |
| `qcontrol_http_request_set_scheme(head, value, value_len)` | Replace scheme |
| `qcontrol_http_request_authority(head)` / `_len(head)` | Current authority |
| `qcontrol_http_request_set_authority(head, value, value_len)` | Replace authority |
| `qcontrol_http_request_path(head)` / `_len(head)` | Current path |
| `qcontrol_http_request_set_path(head, value, value_len)` | Replace path |

Available response head accessors and setters:

| API | Description |
|-----|-------------|
| `qcontrol_http_response_headers(head)` | Mutable response headers handle |
| `qcontrol_http_response_status_code(head)` | Current status code |
| `qcontrol_http_response_set_status_code(head, status_code)` | Replace status code |
| `qcontrol_http_response_reason(head)` / `_len(head)` | Current reason phrase |
| `qcontrol_http_response_set_reason(head, value, value_len)` | Replace reason phrase |
| `qcontrol_http_headers_add(headers, name, name_len, value, value_len)` | Append a header |
| `qcontrol_http_headers_set(headers, name, name_len, value, value_len)` | Replace all headers with a matching name |
| `qcontrol_http_headers_remove(headers, name, name_len)` | Remove matching headers |

The convenience macros in `<qcontrol/helpers.h>` wrap common literal-based edits:

| Macro | Expands To |
|-------|------------|
| `QCONTROL_HTTP_HEADER_ADD(headers, name, value)` | `qcontrol_http_headers_add(...)` |
| `QCONTROL_HTTP_HEADER_SET(headers, name, value)` | `qcontrol_http_headers_set(...)` |
| `QCONTROL_HTTP_HEADER_REMOVE(headers, name)` | `qcontrol_http_headers_remove(...)` |

## SSE Operations

SSE stays inside one HTTP response. HTTP callbacks still own status, headers, and trailers, while qcontrol parses `text/event-stream` bodies into complete logical SSE events before your plugin sees them.

For `text/event-stream` responses, `on_http_response_body` still receives decoded body chunks. `on_sse_event` is an additional parsed view of those chunks; plugins that implement both should keep exchange state and choose one semantic parser per exchange.

### Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_sse_open` | `qcontrol_stream_action_t (*)(qcontrol_sse_open_event_t*)` | After SSE response detection | Start one SSE stream and optionally attach stream-scoped state |
| `on_sse_event` | `qcontrol_stream_action_t (*)(void*, qcontrol_sse_event_record_t*)` | Once per parsed event | Observe or rewrite one logical SSE event |
| `on_sse_close` | `void (*)(void*, qcontrol_sse_close_event_t*)` | When the stream ends | Observe terminal close state and clean up |

### Events

All SSE callbacks carry `qcontrol_stream_ctx_t` through the embedded `ctx` field:

| Field | Type | Description |
|-------|------|-------------|
| `net` | `qcontrol_net_ctx_t` | Underlying normalized network context |
| `connection_id` | `uint64_t` | Stable identifier for the owning tracked connection |
| `exchange_id` | `uint64_t` | Stable identifier for the owning HTTP exchange |
| `http_version` | `qcontrol_http_version_t` | HTTP version that created this SSE stream |
| `protocol` | `qcontrol_stream_protocol_t` | `QCONTROL_STREAM_PROTOCOL_SSE` for this family |

**qcontrol_sse_open_event_t** — passed to `on_sse_open`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_stream_ctx_t` | Shared stream correlation context |
| `content_type` | `const char*` | Response content type that identified this stream as SSE |
| `content_type_len` | `size_t` | Length of `content_type` |

**qcontrol_sse_event_record_t** — passed to `on_sse_event`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_stream_ctx_t` | Shared stream correlation context |
| `event_name` | `const char*` | Parsed `event:` field, or `NULL` when absent |
| `event_name_len` | `size_t` | Length of `event_name` |
| `id` | `const char*` | Parsed `id:` field, or `NULL` when absent |
| `id_len` | `size_t` | Length of `id` |
| `retry_ms` | `uint64_t` | Parsed `retry:` field value |
| `has_retry_ms` | `int` | Non-zero when `retry_ms` is present |
| `data` | `const char*` | Parsed logical event data |
| `data_len` | `size_t` | Length of `data` |
| `event_handle` | `qcontrol_sse_event_t*` | Host-backed mutable event handle, or `NULL` |

**qcontrol_sse_close_event_t** — passed to `on_sse_close`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_stream_ctx_t` | Shared stream correlation context |
| `reason` | `qcontrol_http_close_reason_t` | Why the owning SSE response stream closed |

### Actions

Use the shared stream action helpers from `qcontrol/stream.h`:

| Macro | Description |
|-------|-------------|
| `QCONTROL_STREAM_PASS` | Continue forwarding the current SSE callback normally |
| `QCONTROL_STREAM_BLOCK` | Terminate the SSE response stream |
| `QCONTROL_STREAM_STATE(ptr)` | Attach or replace one SSE-stream-scoped state pointer |

### Mutable Event Handle

When `event_handle` is non-`NULL`, `event_handle->ops` exposes host-backed mutation helpers:

| API | Description |
|-----|-------------|
| `set_event_name()` / `clear_event_name()` | Replace or remove the parsed `event:` field |
| `set_id()` / `clear_id()` | Replace or remove the parsed `id:` field |
| `set_retry_ms()` / `clear_retry_ms()` | Replace or remove the parsed `retry:` field |
| `set_data()` / `clear_data()` | Replace or clear the logical event data |

### SSE Rewrite Example

```c
static qcontrol_stream_action_t on_sse_event(
    void* state,
    qcontrol_sse_event_record_t* ev
) {
    (void)state;
    if (ev->event_handle != NULL && ev->event_handle->ops->set_data != NULL) {
        ev->event_handle->ops->set_data(ev->event_handle->context, "pong", 4);
    }
    return QCONTROL_STREAM_PASS;
}
```

## WebSocket Operations

WebSocket is HTTP only through the upgrade handshake. HTTP callbacks still own the upgrade request and `101` response, and qcontrol delivers logical post-upgrade WebSocket messages after frame parsing, client unmasking, and message reassembly.

### Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_websocket_open` | `qcontrol_stream_action_t (*)(qcontrol_websocket_open_event_t*)` | After a successful upgrade | Start one upgraded stream and optionally attach stream-scoped state |
| `on_websocket_send` | `qcontrol_stream_action_t (*)(void*, qcontrol_websocket_message_event_t*)` | Once per client-to-server message | Observe or rewrite one logical outgoing message |
| `on_websocket_recv` | `qcontrol_stream_action_t (*)(void*, qcontrol_websocket_message_event_t*)` | Once per server-to-client message | Observe or rewrite one logical incoming message |
| `on_websocket_close` | `void (*)(void*, qcontrol_websocket_close_event_t*)` | When the upgraded stream ends | Observe terminal close state and clean up |

### Events

All WebSocket callbacks also carry `qcontrol_stream_ctx_t` through the embedded `ctx` field with the same `net`, `connection_id`, `exchange_id`, and `http_version` correlation described above.

**qcontrol_websocket_open_event_t** — passed to `on_websocket_open`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_stream_ctx_t` | Shared stream correlation context |
| `subprotocol` | `const char*` | Negotiated subprotocol, or `NULL` |
| `subprotocol_len` | `size_t` | Length of `subprotocol` |
| `extensions` | `const char*` | Negotiated extensions header value, or `NULL` |
| `extensions_len` | `size_t` | Length of `extensions` |

**qcontrol_websocket_message_event_t** — passed to `on_websocket_send` and `on_websocket_recv`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_stream_ctx_t` | Shared stream correlation context |
| `kind` | `qcontrol_websocket_message_type_t` | `QCONTROL_WEBSOCKET_MESSAGE_TEXT` or `QCONTROL_WEBSOCKET_MESSAGE_BINARY` |
| `bytes` | `const char*` | Logical message payload bytes |
| `bytes_len` | `size_t` | Length of `bytes` |
| `body` | `qcontrol_buffer_t*` | Host-backed mutable message buffer, or `NULL` |

**qcontrol_websocket_close_event_t** — passed to `on_websocket_close`:

| Field | Type | Description |
|-------|------|-------------|
| `ctx` | `qcontrol_stream_ctx_t` | Shared stream correlation context |
| `direction` | `qcontrol_stream_direction_t` | First observed close-frame direction, or `QCONTROL_STREAM_DIRECTION_UNKNOWN` |
| `code` | `uint16_t` | Observed close code, or `0` |
| `reason` | `const char*` | UTF-8 close reason, or `NULL` |
| `reason_len` | `size_t` | Length of `reason` |
| `clean` | `int` | Non-zero when the close handshake completed cleanly |

### Actions

| Macro | Description |
|-------|-------------|
| `QCONTROL_STREAM_PASS` | Continue forwarding the current WebSocket callback normally |
| `QCONTROL_STREAM_BLOCK` | Terminate the upgraded WebSocket stream |
| `QCONTROL_STREAM_STATE(ptr)` | Attach or replace one upgraded-stream-scoped state pointer |

### Mutable Message Buffer

When `body` is non-`NULL`, use the regular buffer helpers to replace the logical message payload in place:

| API | Description |
|-----|-------------|
| `qcontrol_buffer_set(body, bytes, len)` | Replace the entire logical payload |
| `qcontrol_buffer_clear(body)` | Clear the current logical payload |

qcontrol handles client masking and negotiated `permessage-deflate` before plugin callbacks, so plugins work with logical payload bytes rather than raw frames or compressed wire payloads.

### WebSocket Rewrite Example

```c
static qcontrol_stream_action_t on_websocket_send(
    void* state,
    qcontrol_websocket_message_event_t* ev
) {
    (void)state;
    if (ev->body != NULL) {
        qcontrol_buffer_set(ev->body, "rewritten", 9);
    }
    return QCONTROL_STREAM_PASS;
}
```

## Building Plugins

### Project Setup

Create the following directory structure:

```
my-plugin/
  Makefile
  my_plugin.c
```

**Makefile** — Build configuration:

```makefile
CC = cc
CFLAGS = -Wall -Wextra -fPIC -I/path/to/qcontrol/sdk/c/include

.PHONY: all clean

all: libmy_plugin.so my_plugin.o

# Shared library for dynamic loading
libmy_plugin.so: my_plugin.c
	$(CC) $(CFLAGS) -shared -o $@ $<

# Object file for bundling
my_plugin.o: my_plugin.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f libmy_plugin.so my_plugin.o
```

### Plugin Structure

Plugins export a single `qcontrol_plugin` descriptor:

The ABI version remains `1`; rebuild plugins against the current headers.
The version does not detect incompatible binaries built against older headers.

```c
#include <qcontrol.h>

static int init(const qcontrol_string_view_t* config, char* error, size_t error_cap) {
    // Called once on load with the initial effective config
    (void)config; (void)error; (void)error_cap;
    return 0;  // 0 = success
}

static void cleanup(void) {
    // Called on unload
}

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    // Handle file open
    return QCONTROL_FILE_PASS;
}

const qcontrol_plugin_t qcontrol_plugin = {
    .version = QCONTROL_PLUGIN_VERSION,
    .name = "my-plugin",
    .on_init = init,                    // Optional: initial config
    .on_cleanup = cleanup,              // Optional
    // File callbacks (optional)
    .on_file_open = on_file_open,
    .on_file_read = NULL,
    .on_file_write = NULL,
    .on_file_close = NULL,
    // Exec callbacks (optional)
    .on_exec = NULL,
    .on_exec_stdin = NULL,
    .on_exec_stdout = NULL,
    .on_exec_stderr = NULL,
    .on_exec_exit = NULL,
    // Net callbacks (optional)
    .on_net_connect = NULL,
    .on_net_accept = NULL,
    .on_net_tls = NULL,
    .on_net_domain = NULL,
    .on_net_protocol = NULL,
    .on_net_send = NULL,
    .on_net_recv = NULL,
    .on_net_close = NULL,
    // HTTP callbacks (optional)
    .on_http_request = NULL,
    .on_http_request_body = NULL,
    .on_http_request_trailers = NULL,
    .on_http_request_done = NULL,
    .on_http_response = NULL,
    .on_http_response_body = NULL,
    .on_http_response_trailers = NULL,
    .on_http_response_done = NULL,
    .on_http_exchange_close = NULL,
    // SSE callbacks (optional)
    .on_sse_open = NULL,
    .on_sse_event = NULL,
    .on_sse_close = NULL,
    // WebSocket callbacks (optional)
    .on_websocket_open = NULL,
    .on_websocket_send = NULL,
    .on_websocket_recv = NULL,
    .on_websocket_close = NULL,
};
```

All callbacks are optional. Only implement what you need.

### Building

```bash
# Shared library for dynamic loading
cc -shared -fPIC -o libmy_plugin.so my_plugin.c -I/path/to/sdk/include

# Object file for static embedding or distribution-specific packaging
cc -c -fPIC -o my_plugin.o my_plugin.c -I/path/to/sdk/include
```

Output locations:
- Shared library: `libmy_plugin.so`
- Object file: `my_plugin.o`

## Running Plugins

List built shared libraries by `name` and `path` under a behavior's `plugins`
in `config.yaml`, then select the behavior with `qcontrol run --behavior`:

```yaml
behaviors:
  my-plugins:
    plugins:
      - name: my-plugin
        path: /absolute/path/to/libmy_plugin.so
```

```bash
qcontrol run --behavior my-plugins -- ./target
```

The lower-level hosts also honor `QCONTROL_PLUGINS` for comma-separated plugin
library paths:

```bash
QCONTROL_PLUGINS=./logger.so,./blocker.so qcontrol run -- ./target
```

## API Reference

### Plugin Lifecycle

```c
static int init(const qcontrol_string_view_t* config, char* error, size_t error_cap) {
    // Called once after plugin is loaded, with the initial effective config
    // Initialize resources, open log files, etc.
    (void)config; (void)error; (void)error_cap;
    return 0;  // 0 = success, 1 = config invalid, other nonzero = failure
}

static void cleanup(void) {
    // Called before plugin is unloaded
    // Clean up resources, close files, etc.
}
```

### Configuration

Operators configure a plugin through the `config:` value on its behavior entry.
Plugin-specific settings belong there, not in environment variables, and a
plugin never sets process environment. Reference secrets by file path; never
put a secret in config.

#### Delivery

`on_init` receives the initial effective config once, on the loading thread,
before the plugin is published. `on_config` receives later updates only, never
the initial config, and only for a plugin that initialized successfully. Both
take a `qcontrol_string_view_t` plus a separate error buffer; neither receives
a host handle or secret resolver. Leave `on_config` NULL to accept no updates.
The host calls `on_config` when the configuration a running session started
from is edited: each plugin gets its complete new effective config, or `null`
once its entry is removed. Calls for one plugin never overlap, unchanged config
is skipped, and only the newest pending update is delivered.

Read exactly `config->len` bytes from `config->data`: NUL termination is not
guaranteed. The view and its bytes are valid only during the call, so copy
anything you keep. The contents are UTF-8 JSON and always either `null` (no
config, never a NULL pointer) or a JSON object: the host rejects any other shape
before loading, so a plugin does not check the shape itself. A plugin without
`on_init` accepts only `null`.

#### Schema

The SDK does not parse JSON or choose a schema; parse the object with the JSON
library of your choice. Give every field a default, and reject any key the
plugin does not define, so a misspelled field is rejected instead of ignored.
`null` selects the defaults. A plugin that needs `on_init` for setup but takes
no settings accepts only `null` and an empty object.

#### Lifecycle

Use one parse-and-validate function for both callbacks, and build everything a
config needs before it takes effect:

1. Parse and validate the complete document.
2. Open or start anything the new settings require.
3. Publish the new settings; release what the previous settings held.

A rejected update therefore changes nothing. A failing `on_init` owns its own
unwind: the host does not call `on_cleanup` and never publishes the plugin.

```c
#include <qcontrol.h>
#include <stdio.h>
#include <string.h>

/* Validate a complete document without touching active state. This plugin
   takes no settings: only `null` and an empty object are accepted. */
static int resolve(const qcontrol_string_view_t* config, char* error, size_t error_cap) {
    if ((config->len == 4 && memcmp(config->data, "null", 4) == 0) ||
        (config->len == 2 && memcmp(config->data, "{}", 2) == 0)) {
        return QCONTROL_CONFIG_OK;
    }
    if (error != NULL && error_cap > 0) {
        snprintf(error, error_cap, "config takes no fields");
    }
    return QCONTROL_CONFIG_INVALID;
}

static int init(const qcontrol_string_view_t* config, char* error, size_t error_cap) {
    int status = resolve(config, error, error_cap);
    /* Start resources and publish settings only after status is OK. */
    return status;
}

static int on_config(const qcontrol_string_view_t* config,
                     char* error, size_t error_cap) {
    /* Prepare replacements, publish, then release the old ones. */
    return resolve(config, error, error_cap);
}
```

#### Applying settings

By default each callback uses the settings in effect when it runs, so an
accepted update applies from the next event. Pin settings to an operation
instead only when one decision spans several callbacks (for example an HTTP
request and its response), or when resources built from the settings must
stay alive until the operation ends.

#### Errors

Return `QCONTROL_CONFIG_OK` (0) to accept, `QCONTROL_CONFIG_INVALID` (1) to
reject the settings, or another nonzero value for a failure. To explain a
failure, write a NUL-terminated reason into `error`, using at most `error_cap`
bytes including the NUL. `error` may be NULL and `error_cap` may be 0, in which
case write nothing. The host supplies a zeroed buffer (currently 1024 bytes),
but its capacity is not an ABI promise; the host reads at most `error_cap`
bytes and does not add a terminator.

A reason names the field and the problem, never the value: config may hold
paths or other sensitive text.

#### Testing

Unit-test updates by calling the exported callbacks directly. Cover accepted
settings, each rejection, and that a rejected update leaves the previous
settings in effect. An end-to-end check edits the session's configuration while
the plugin runs; the host delivers the change through `on_config`.

### Logger Utility

Thread-safe file logger with reentrancy protection:

```c
#include <qcontrol.h>
#include <qcontrol/log.h>

QCONTROL_LOGGER(logger);

static int init(const qcontrol_string_view_t* config, char* error, size_t error_cap) {
    (void)config; (void)error; (void)error_cap;  // this plugin takes no settings
    qcontrol_log_init(&logger, NULL);
    qcontrol_log(&logger, "[my-plugin] started\n");
    return 0;
}

static void cleanup(void) {
    qcontrol_log(&logger, "[my-plugin] stopped\n");
    qcontrol_log_cleanup(&logger);
}

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    qcontrol_log(&logger, "open: %.*s flags=0x%x\n",
        (int)ev->path_len, ev->path, ev->flags);
    return QCONTROL_FILE_PASS;
}
```

The log file path is controlled by `QCONTROL_LOG_FILE` (default: `/tmp/qcontrol.log`).

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `QCONTROL_PLUGINS` | (none) | Comma-separated paths to plugin `.so` files |
| `QCONTROL_LOG_FILE` | `/tmp/qcontrol.log` | Log file path for Logger utility |

## License

Apache License 2.0
