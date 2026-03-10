# Qcontrol C SDK

Intercept. Observe. Transform. Build secure sandboxes for AI agents and control any application from the inside.

---

**Overview:** [Introduction](#introduction) · [What Can You Build?](#what-can-you-build) · [Quick Start](#quick-start) · [Core Concepts](#core-concepts) · [Examples](#examples)

**API Reference:** [File Operations](#file-operations) · [Exec Operations](#exec-operations) · [Network Operations](#network-operations)

**Development:** [Building Plugins](#building-plugins) · [Bundling Plugins](#bundling-plugins) · [Environment Variables](#environment-variables)

---

## Introduction

The Qcontrol C SDK lets you build plugins that hook directly into system calls, giving you precise control over how applications interact with files, processes, and the network.

This makes Qcontrol ideal for building **AI agent sandboxes and runtimes**. As agents gain autonomy to read files, execute commands, and make network requests, you need visibility and control at the syscall level. Qcontrol gives you that:

- **Intercept file operations** — See every open, read, write, and close. Block access to sensitive paths or transform data as it flows through.
- **Intercept exec operations** — Monitor process spawning, modify arguments, capture stdin/stdout/stderr.
- **Intercept network operations** — Watch connections form, inspect send/recv traffic, detect TLS and protocols.

Your plugins run inside the target process via native function hooking. Observe silently, block operations, or transform data in transit. No application changes required.

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

Bundle and run from a plugin directory using the Makefile pattern shown in [Building Plugins](#building-plugins):

```bash
qcontrol bundle --plugins ./my-plugin -o hello-plugin-bundle.so
qcontrol wrap --bundle ./hello-plugin-bundle.so -- cat /etc/passwd
```

That's it. Your plugin now intercepts every file open in the wrapped process.

## Core Concepts

### Hooks

Qcontrol intercepts operations at three levels:

| Domain | Operations | Status |
|--------|------------|--------|
| **File** | open, read, write, close | Fully implemented |
| **Exec** | spawn, stdin, stdout, stderr, exit | SDK ready, agent coming soon |
| **Network** | connect, accept, send, recv, close | SDK ready, agent coming soon |

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

## File Operations

File operations are fully implemented. Use these to observe, block, or transform file I/O.

### Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_file_open` | `qcontrol_file_action_t (*)(qcontrol_file_open_event_t*)` | After open()/openat() | Decide interception |
| `on_file_read` | `qcontrol_file_action_t (*)(void*, qcontrol_file_read_event_t*)` | After read() | Observe or block |
| `on_file_write` | `qcontrol_file_action_t (*)(void*, qcontrol_file_write_event_t*)` | Before write() | Observe or block |
| `on_file_close` | `void (*)(void*, qcontrol_file_close_event_t*)` | After close() | Cleanup state |

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

### Actions

**Action macros** — return from callbacks:

| Macro | Description |
|-------|-------------|
| `QCONTROL_FILE_PASS` | No interception, continue normally |
| `QCONTROL_FILE_BLOCK` | Block with EACCES |
| `QCONTROL_FILE_BLOCK_WITH(errno)` | Block with custom errno |
| `QCONTROL_FILE_STATE(ptr)` | Track state only, no transforms |

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

> **Note:** Proxy-backed wrap mode already exercises this ABI today. Native agent-side network hooks are still coming.

### Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_net_connect` | `qcontrol_net_action_t (*)(qcontrol_net_connect_event_t*)` | After connect() | Decide interception |
| `on_net_accept` | `qcontrol_net_action_t (*)(qcontrol_net_accept_event_t*)` | After accept() | Decide interception |
| `on_net_tls` | `void (*)(void*, qcontrol_net_tls_event_t*)` | After TLS handshake | Observe |
| `on_net_domain` | `void (*)(void*, qcontrol_net_domain_event_t*)` | Domain discovered | Observe |
| `on_net_protocol` | `void (*)(void*, qcontrol_net_protocol_event_t*)` | Protocol detected | Observe |
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
| `domain` | `const char*` | Domain name (may be NULL) |
| `domain_len` | `size_t` | Length of domain |
| `protocol` | `const char*` | Protocol (may be NULL) |
| `protocol_len` | `size_t` | Length of protocol |

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

```c
#include <qcontrol.h>

static int init(void) {
    // Called on load
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
    .on_init = init,                    // Optional
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
};
```

All callbacks are optional. Only implement what you need.

### Building

```bash
# Shared library for dynamic loading
cc -shared -fPIC -o libmy_plugin.so my_plugin.c -I/path/to/sdk/include

# Object file for bundling
cc -c -fPIC -o my_plugin.o my_plugin.c -I/path/to/sdk/include
```

Output locations:
- Shared library: `libmy_plugin.so`
- Object file: `my_plugin.o`

### Using Bundles

Bundle plugins directly from plugin directories:

```bash
qcontrol bundle --plugins ./my-plugin -o my-plugin-bundle.so
qcontrol wrap --bundle ./my-plugin-bundle.so -- ./target

# Multiple plugins
qcontrol bundle --plugins ./logger,./blocker -o my-plugins.so
qcontrol wrap --bundle ./my-plugins.so -- ./target
```

## Bundling Plugins

For distribution, bundle plugins with the agent core into a single `.so` file.

### Creating a Bundle

Create the bundle directly from plugin directories:

```bash
qcontrol bundle --plugins ./file-logger,./access-control -o my-bundle.so
```

You can also pass prebuilt object files, or use a `bundle.toml` file when you want to describe larger bundles declaratively:

```toml
[bundle]
output = "my-plugins.so"

[[plugins]]
source = "./file-logger"

[[plugins]]
source = "./access-control"
```

```bash
qcontrol bundle --config bundle.toml
```

### Using Bundles

```bash
# Via command line flag
qcontrol wrap --bundle my-bundle.so -- ./target

# Via environment variable
QCONTROL_BUNDLE=./my-bundle.so qcontrol wrap -- ./target
```

## API Reference

### Plugin Lifecycle

```c
static int init(void) {
    // Called after plugin is loaded
    // Initialize resources, open log files, etc.
    return 0;  // Return 0 on success, non-zero on failure
}

static void cleanup(void) {
    // Called before plugin is unloaded
    // Clean up resources, close files, etc.
}
```

### Logger Utility

Thread-safe file logger with reentrancy protection:

```c
#include <qcontrol.h>
#include <qcontrol/log.h>

QCONTROL_LOGGER(logger);

static int init(void) {
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
| `QCONTROL_BUNDLE` | (none) | Path to bundled plugin `.so` file |
| `QCONTROL_LOG_FILE` | `/tmp/qcontrol.log` | Log file path for Logger utility |

## License

Apache License 2.0
