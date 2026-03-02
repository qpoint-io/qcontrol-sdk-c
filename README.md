# qcontrol C SDK

C bindings for writing qcontrol plugins that intercept file, exec, and network operations.

## Table of Contents

- [Introduction](#introduction)
- [Quick Start](#quick-start)
- [Examples](#examples)
- [Building Plugins](#building-plugins)
  - [Project Setup](#project-setup)
  - [Plugin Structure](#plugin-structure)
  - [Building](#building)
  - [Using Plugins](#using-plugins)
- [Bundling Plugins](#bundling-plugins)
  - [Bundle Configuration](#bundle-configuration)
  - [Creating a Bundle](#creating-a-bundle)
  - [Using Bundles](#using-bundles)
- [API Reference](#api-reference)
  - [Plugin Lifecycle](#plugin-lifecycle)
  - [Logger Utility](#logger-utility)
- [File Operations](#file-operations)
  - [Callbacks](#file-callbacks)
  - [Events](#file-events)
  - [Actions](#file-actions)
  - [Sessions and Transforms](#sessions-and-transforms)
  - [Buffer API](#buffer-api)
  - [Patterns](#patterns)
- [Exec Operations](#exec-operations)
  - [Callbacks](#exec-callbacks)
  - [Events](#exec-events)
  - [Actions](#exec-actions)
  - [Sessions](#exec-sessions)
- [Network Operations](#network-operations)
  - [Callbacks](#network-callbacks)
  - [Events](#network-events)
  - [Actions](#network-actions)
  - [Sessions](#network-sessions)
  - [Context](#network-context)
- [Environment Variables](#environment-variables)
- [License](#license)

## Introduction

**qcontrol** is a CLI tool for observing and controlling applications via native function hooking. The C SDK provides bindings for writing plugins that can:

- **File operations**: Intercept open, read, write, and close syscalls
- **Exec operations** (v1): Intercept process spawning and I/O (not yet implemented in agent)
- **Network operations** (v1): Intercept connections, sends, and receives (not yet implemented in agent)

Plugins can observe operations, block them, or transform data in transit.

## Quick Start

Create a minimal plugin that logs file opens:

```c
#include <qcontrol.h>
#include <qcontrol/helpers.h>
#include <stdio.h>

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    fprintf(stderr, "open: %s\n", ev->path);
    return QCONTROL_FILE_PASS;
}

QCONTROL_PLUGIN("hello-plugin",
    .on_file_open = on_file_open,
);
```

Build and run:

```bash
gcc -shared -fPIC -o libhello_plugin.so plugin.c -I/path/to/sdk/include
qcontrol wrap --plugins ./libhello_plugin.so -- cat /etc/passwd
```

## Examples

| Plugin | Description | Demonstrates |
|--------|-------------|--------------|
| [file-logger](examples/file-logger/) | Logs all file operations | Basic callbacks, Logger utility |
| [access-control](examples/access-control/) | Blocks `/tmp/secret*` paths | Blocking with `QCONTROL_FILE_BLOCK` |
| [byte-counter](examples/byte-counter/) | Tracks bytes read/written | State tracking with `QCONTROL_FILE_STATE` |
| [content-filter](examples/content-filter/) | Redacts sensitive data | Session with RwConfig patterns |
| [text-transform](examples/text-transform/) | Custom buffer manipulation | Session with transform function |
| [exec-logger](examples/exec-logger/) | Logs exec operations | v1 exec API |
| [net-logger](examples/net-logger/) | Logs network operations | v1 network API |

## Building Plugins

### Project Setup

Create the following directory structure:

```
my-plugin/
  Makefile
  src/
    plugin.c
```

**Makefile** - Build configuration:

```makefile
NAME := my-plugin
ARCH := $(shell uname -m)-$(shell uname -s | tr A-Z a-z)
DIST := dist

CC ?= gcc
CFLAGS_SHARED := -shared -fPIC -Wall -Wextra
CFLAGS_OBJECT := -c -fPIC -Wall -Wextra

# SDK dependency
SDK_REPO ?= https://github.com/qpoint-io/qcontrol-sdk-c
SDK_REF ?= main
SDK_DIR := sdk

INCLUDES := -I$(SDK_DIR)/include
LIBS := -lpthread

SRC := src/plugin.c

.PHONY: all build dist clean sdk

all: build

sdk:
	@if [ ! -d "$(SDK_DIR)" ]; then \
		git clone --depth 1 --branch $(SDK_REF) $(SDK_REPO) $(SDK_DIR); \
	fi

build: sdk
	@mkdir -p $(DIST)
	$(CC) $(CFLAGS_SHARED) -o $(DIST)/$(NAME)-$(ARCH).so $(SRC) $(INCLUDES) $(LIBS)

dist: sdk
	@mkdir -p $(DIST)
	$(CC) $(CFLAGS_OBJECT) -o $(DIST)/$(NAME)-$(ARCH).o $(SRC) $(INCLUDES)

clean:
	rm -rf $(DIST)

clean-all: clean
	rm -rf $(SDK_DIR)
```

### Plugin Structure

Plugins export a single `qcontrol_plugin` descriptor:

```c
#include <qcontrol.h>
#include <qcontrol/helpers.h>

static int plugin_init(void) {
    // Called on plugin load
    return 0;  // 0 = success
}

static void plugin_cleanup(void) {
    // Called on plugin unload
}

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    // Handle file open
    return QCONTROL_FILE_PASS;
}

QCONTROL_PLUGIN("my-plugin",
    .on_init = plugin_init,           // Optional: called on load
    .on_cleanup = plugin_cleanup,     // Optional: called on unload
    .on_file_open = on_file_open,     // Optional: file callbacks
    .on_file_read = on_file_read,
    .on_file_write = on_file_write,
    .on_file_close = on_file_close,
    .on_exec = on_exec,               // Optional: exec callbacks (v1)
    .on_exec_stdin = on_exec_stdin,
    .on_exec_stdout = on_exec_stdout,
    .on_exec_stderr = on_exec_stderr,
    .on_exec_exit = on_exec_exit,
    .on_net_connect = on_net_connect, // Optional: net callbacks (v1)
    .on_net_accept = on_net_accept,
    .on_net_tls = on_net_tls,
    .on_net_domain = on_net_domain,
    .on_net_protocol = on_net_protocol,
    .on_net_send = on_net_send,
    .on_net_recv = on_net_recv,
    .on_net_close = on_net_close,
);
```

All callbacks are optional - only implement what you need.

### Building

```bash
# Build shared library for dynamic loading
make build

# Build object file for bundling
make dist

# Clean build artifacts
make clean
```

Output locations:
- Shared library: `dist/<name>-<arch>.so`
- Object file: `dist/<name>-<arch>.o`

### Using Plugins

Load plugins dynamically via `QCONTROL_PLUGINS`:

```bash
# Single plugin
QCONTROL_PLUGINS=./my_plugin.so qcontrol wrap -- ./target

# Multiple plugins (comma-separated)
QCONTROL_PLUGINS=./logger.so,./blocker.so qcontrol wrap -- ./target
```

Or with the `--plugins` flag:

```bash
qcontrol wrap --plugins ./my_plugin.so -- ./target
```

## Bundling Plugins

For distribution, bundle plugins with the agent core into a single `.so` file.

### Bundle Configuration

Create a `bundle.toml` file:

```toml
[bundle]
output = "my-plugins.so"

[[plugins]]
source = "./file-logger"    # Plugin directory (auto-builds)

[[plugins]]
source = "./access-control"

[[plugins]]
source = "./content-filter"
```

### Creating a Bundle

1. Build plugins as object files:

```bash
# Build all plugins in examples/
make -C examples dist

# Or build individual plugin
cd my-plugin && make dist
```

2. Create the bundle:

```bash
qcontrol bundle --config bundle.toml
```

Or manually with object files:

```bash
qcontrol bundle --plugins plugin1.o,plugin2.o -o my-bundle.so
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
static int plugin_init(void) {
    // Called after plugin is loaded
    // Initialize resources, open log files, etc.
    return 0;  // Return 0 on success, non-zero on failure
}

static void plugin_cleanup(void) {
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

static int plugin_init(void) {
    qcontrol_log_init(&logger, NULL);  // NULL = use QCONTROL_LOG_FILE
    qcontrol_log(&logger, "[my-plugin] started\n");
    return 0;
}

static void plugin_cleanup(void) {
    qcontrol_log(&logger, "[my-plugin] stopped\n");
    qcontrol_log_cleanup(&logger);
}

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    qcontrol_log(&logger, "open: %s flags=0x%x\n", ev->path, ev->flags);
    return QCONTROL_FILE_PASS;
}
```

The log file path is controlled by `QCONTROL_LOG_FILE` (default: `/tmp/qcontrol.log`).

## File Operations

### File Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_file_open` | `qcontrol_file_action_t (*)(qcontrol_file_open_event_t*)` | After open() | Decide interception |
| `on_file_read` | `qcontrol_file_action_t (*)(void*, qcontrol_file_read_event_t*)` | After read() | Observe or block |
| `on_file_write` | `qcontrol_file_action_t (*)(void*, qcontrol_file_write_event_t*)` | Before write() | Observe or block |
| `on_file_close` | `void (*)(void*, qcontrol_file_close_event_t*)` | After close() | Cleanup state |

The `void*` state parameter is your custom state returned from `on_file_open`.

### File Events

**qcontrol_file_open_event_t** - passed to `on_file_open`:

| Field | Type | Description |
|-------|------|-------------|
| `path` | `const char*` | File path being opened |
| `path_len` | `size_t` | Path length |
| `flags` | `int` | Open flags (O_RDONLY, O_WRONLY, etc.) |
| `mode` | `unsigned int` | File mode (for O_CREAT) |
| `result` | `int` | Result fd on success, negative errno on failure |

**qcontrol_file_read_event_t** - passed to `on_file_read`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | File descriptor |
| `buf` | `void*` | Buffer containing data read |
| `count` | `size_t` | Requested byte count |
| `result` | `ssize_t` | Bytes read or negative errno |

**qcontrol_file_write_event_t** - passed to `on_file_write`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | File descriptor |
| `buf` | `const void*` | Buffer containing data to write |
| `count` | `size_t` | Byte count to write |
| `result` | `ssize_t` | Bytes written or negative errno |

**qcontrol_file_close_event_t** - passed to `on_file_close`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | File descriptor |
| `result` | `int` | Result (0 or negative errno) |

### File Actions

**Return values from `on_file_open`:**

| Macro | Description |
|-------|-------------|
| `QCONTROL_FILE_PASS` | No interception, continue normally |
| `QCONTROL_FILE_BLOCK` | Block with EACCES |
| `QCONTROL_FILE_BLOCK_WITH(errno)` | Block with custom errno |
| `QCONTROL_FILE_SESSION(session)` | Intercept with transform config |
| `QCONTROL_FILE_STATE(ptr)` | Track state only, no transforms |

**Return values from `on_file_read`/`on_file_write`:**

| Macro | Description |
|-------|-------------|
| `QCONTROL_FILE_PASS` | Continue normally |
| `QCONTROL_FILE_BLOCK` | Block with EACCES |
| `QCONTROL_FILE_BLOCK_WITH(errno)` | Block with custom errno |

### Sessions and Transforms

Return a session from `on_file_open` to configure read/write transforms:

```c
static qcontrol_file_rw_config_t read_config = {
    .prefix = "[LOG] ",
    .prefix_len = 6,
    .suffix = "\n",
    .suffix_len = 1,
};

static qcontrol_file_pattern_t patterns[] = {
    QCONTROL_PATTERN("password", "********"),
    QCONTROL_PATTERN("secret", "[REDACTED]"),
};

static qcontrol_file_rw_config_t filter_config = {
    .replace = patterns,
    .replace_count = 2,
};

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    if (ev->result < 0) return QCONTROL_FILE_PASS;

    qcontrol_file_session_t session = {
        .state = my_state,        // Optional: custom state pointer
        .read = &read_config,     // Optional: read transform config
        .write = &filter_config,  // Optional: write transform config
    };
    return QCONTROL_FILE_SESSION(session);
}
```

**qcontrol_file_rw_config_t fields:**

| Field | Type | Description |
|-------|------|-------------|
| `prefix` | `const char*` | Static prefix to prepend |
| `prefix_len` | `size_t` | Prefix length |
| `suffix` | `const char*` | Static suffix to append |
| `suffix_len` | `size_t` | Suffix length |
| `prefix_fn` | `qcontrol_file_prefix_fn` | Dynamic prefix function |
| `suffix_fn` | `qcontrol_file_suffix_fn` | Dynamic suffix function |
| `replace` | `const qcontrol_file_pattern_t*` | Pattern replacements array |
| `replace_count` | `size_t` | Number of patterns |
| `transform` | `qcontrol_file_transform_fn` | Custom transform function |

**Transform pipeline order:** `prefix` -> `replace` -> `transform` -> `suffix`

**Custom transform function:**

```c
static qcontrol_file_action_t my_transform(
    void* state,
    qcontrol_file_ctx_t* ctx,
    qcontrol_buffer_t* buf
) {
    // ctx provides: fd, path, path_len, flags
    // buf provides: read and modify functions

    if (qcontrol_buffer_contains(buf, QCONTROL_STR("sensitive"))) {
        qcontrol_buffer_replace_all(buf, QCONTROL_STR("sensitive"), QCONTROL_STR("[HIDDEN]"));
    }

    return QCONTROL_FILE_PASS;  // or QCONTROL_FILE_BLOCK
}
```

**Dynamic prefix/suffix functions:**

```c
static const char* dynamic_prefix(void* state, qcontrol_file_ctx_t* ctx, size_t* out_len) {
    // Return NULL for no prefix
    if (ends_with(ctx->path, ".log")) {
        *out_len = 6;
        return "[LOG] ";
    }
    return NULL;
}
```

### Buffer API

The `qcontrol_buffer_t*` type provides functions for inspecting and modifying data:

**Read operations:**

| Function | Description |
|----------|-------------|
| `qcontrol_buffer_data(buf)` | Get pointer to buffer contents |
| `qcontrol_buffer_len(buf)` | Get buffer length |
| `qcontrol_buffer_contains(buf, needle, needle_len)` | Check if buffer contains needle |
| `qcontrol_buffer_starts_with(buf, prefix, prefix_len)` | Check if buffer starts with prefix |
| `qcontrol_buffer_ends_with(buf, suffix, suffix_len)` | Check if buffer ends with suffix |
| `qcontrol_buffer_index_of(buf, needle, needle_len)` | Find position of needle (-1 if not found) |

**Write operations:**

| Function | Description |
|----------|-------------|
| `qcontrol_buffer_prepend(buf, data, len)` | Add data to beginning |
| `qcontrol_buffer_append(buf, data, len)` | Add data to end |
| `qcontrol_buffer_replace(buf, needle, n_len, repl, r_len)` | Replace first occurrence (returns bool) |
| `qcontrol_buffer_replace_all(buf, needle, n_len, repl, r_len)` | Replace all occurrences (returns count) |
| `qcontrol_buffer_remove(buf, needle, len)` | Remove first occurrence (returns bool) |
| `qcontrol_buffer_remove_all(buf, needle, len)` | Remove all occurrences (returns count) |
| `qcontrol_buffer_clear(buf)` | Clear buffer contents |
| `qcontrol_buffer_set(buf, data, len)` | Replace entire buffer contents |
| `qcontrol_buffer_insert_at(buf, pos, data, len)` | Insert data at position |
| `qcontrol_buffer_remove_range(buf, start, end)` | Remove byte range |

Use the `QCONTROL_STR()` macro for string literals:

```c
qcontrol_buffer_contains(buf, QCONTROL_STR("needle"));
qcontrol_buffer_replace_all(buf, QCONTROL_STR("old"), QCONTROL_STR("new"));
```

### Patterns

Use `QCONTROL_PATTERN()` macro for declarative string replacements:

```c
static qcontrol_file_pattern_t patterns[] = {
    QCONTROL_PATTERN("password", "********"),
    QCONTROL_PATTERN("secret", "[REDACTED]"),
    QCONTROL_PATTERN("api_key", "[HIDDEN]"),
};

static qcontrol_file_rw_config_t config = {
    .replace = patterns,
    .replace_count = 3,
};
```

Or create patterns manually:

```c
static qcontrol_file_pattern_t my_patterns[] = {
    { .needle = "foo", .needle_len = 3, .replacement = "bar", .replacement_len = 3 },
    { .needle = "baz", .needle_len = 3, .replacement = "qux", .replacement_len = 3 },
};
```

## Exec Operations

> **Note:** v1 spec - not yet implemented in agent. Plugins will compile but callbacks won't be invoked.

### Exec Callbacks

| Callback | Signature | Phase | Purpose |
|----------|-----------|-------|---------|
| `on_exec` | `qcontrol_exec_action_t (*)(qcontrol_exec_event_t*)` | Before exec | Decide interception |
| `on_exec_stdin` | `qcontrol_exec_action_t (*)(void*, qcontrol_exec_stdin_event_t*)` | Before stdin write | Observe or block |
| `on_exec_stdout` | `qcontrol_exec_action_t (*)(void*, qcontrol_exec_stdout_event_t*)` | After stdout read | Observe or block |
| `on_exec_stderr` | `qcontrol_exec_action_t (*)(void*, qcontrol_exec_stderr_event_t*)` | After stderr read | Observe or block |
| `on_exec_exit` | `void (*)(void*, qcontrol_exec_exit_event_t*)` | After exit | Cleanup state |

### Exec Events

**qcontrol_exec_event_t** - passed to `on_exec`:

| Field | Type | Description |
|-------|------|-------------|
| `path` | `const char*` | Executable path |
| `path_len` | `size_t` | Path length |
| `argv` | `const char* const*` | Arguments (NULL-terminated) |
| `argc` | `size_t` | Argument count |
| `envp` | `const char* const*` | Environment (NULL-terminated) |
| `envc` | `size_t` | Environment variable count |
| `cwd` | `const char*` | Working directory (may be NULL) |
| `cwd_len` | `size_t` | CWD length |

**qcontrol_exec_stdin_event_t** - passed to `on_exec_stdin`:

| Field | Type | Description |
|-------|------|-------------|
| `pid` | `pid_t` | Child process ID |
| `buf` | `const void*` | Data being written to stdin |
| `count` | `size_t` | Byte count |

**qcontrol_exec_stdout_event_t** / **qcontrol_exec_stderr_event_t**:

| Field | Type | Description |
|-------|------|-------------|
| `pid` | `pid_t` | Child process ID |
| `buf` | `void*` | Data read |
| `count` | `size_t` | Requested byte count |
| `result` | `ssize_t` | Bytes read or negative errno |

**qcontrol_exec_exit_event_t** - passed to `on_exec_exit`:

| Field | Type | Description |
|-------|------|-------------|
| `pid` | `pid_t` | Child process ID |
| `exit_code` | `int` | Exit code (if normal exit) |
| `exit_signal` | `int` | Signal number (0 if normal) |

### Exec Actions

**Return values from `on_exec`:**

| Macro | Description |
|-------|-------------|
| `QCONTROL_EXEC_PASS` | No interception |
| `QCONTROL_EXEC_BLOCK` | Block with EACCES |
| `QCONTROL_EXEC_BLOCK_WITH(errno)` | Block with custom errno |
| `QCONTROL_EXEC_SESSION(session)` | Intercept with config |
| `QCONTROL_EXEC_STATE(ptr)` | Track state only |

**Return values from stdin/stdout/stderr callbacks:**

| Macro | Description |
|-------|-------------|
| `QCONTROL_EXEC_PASS` | Continue normally |
| `QCONTROL_EXEC_BLOCK` | Block operation |
| `QCONTROL_EXEC_BLOCK_WITH(errno)` | Block with custom errno |

### Exec Sessions

```c
static const char* new_argv[] = { "wrapper", "--safe", NULL };
static const char* prepend_argv[] = { "--verbose", NULL };
static const char* append_argv[] = { "--", "extra", NULL };
static const char* set_env[] = { "SAFE_MODE=1", NULL };
static const char* unset_env[] = { "DEBUG", NULL };

static qcontrol_exec_action_t on_exec(qcontrol_exec_event_t* ev) {
    qcontrol_exec_session_t session = {
        .state = my_state,

        // Exec modifications
        .set_path = "/usr/bin/safe-wrapper",
        .set_argv = new_argv,
        .prepend_argv = prepend_argv,
        .append_argv = append_argv,
        .set_env = set_env,
        .unset_env = unset_env,
        .set_cwd = "/tmp/sandbox",

        // I/O transforms
        .stdin_config = &stdin_cfg,
        .stdout_config = &stdout_cfg,
        .stderr_config = &stderr_cfg,
    };
    return QCONTROL_EXEC_SESSION(session);
}
```

## Network Operations

> **Note:** v1 spec - not yet implemented in agent. Plugins will compile but callbacks won't be invoked.

### Network Callbacks

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

### Network Events

**qcontrol_net_connect_event_t** - passed to `on_net_connect`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket file descriptor |
| `dst_addr` | `const char*` | Destination IP address |
| `dst_addr_len` | `size_t` | Destination address length |
| `dst_port` | `uint16_t` | Destination port |
| `src_addr` | `const char*` | Local source address (may be NULL) |
| `src_addr_len` | `size_t` | Source address length |
| `src_port` | `uint16_t` | Local source port |
| `result` | `int` | 0 on success, negative errno |

**qcontrol_net_accept_event_t** - passed to `on_net_accept`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Accepted socket fd |
| `listen_fd` | `int` | Listening socket fd |
| `src_addr` | `const char*` | Remote client address |
| `src_addr_len` | `size_t` | Source address length |
| `src_port` | `uint16_t` | Remote client port |
| `dst_addr` | `const char*` | Local server address |
| `dst_addr_len` | `size_t` | Destination address length |
| `dst_port` | `uint16_t` | Local server port |
| `result` | `int` | fd on success, negative errno |

**qcontrol_net_tls_event_t** - passed to `on_net_tls`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `version` | `const char*` | TLS version (e.g., "TLSv1.3") |
| `version_len` | `size_t` | Version string length |
| `cipher` | `const char*` | Cipher suite (may be NULL) |
| `cipher_len` | `size_t` | Cipher string length |

**qcontrol_net_domain_event_t** - passed to `on_net_domain`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `domain` | `const char*` | Domain name |
| `domain_len` | `size_t` | Domain name length |

**qcontrol_net_protocol_event_t** - passed to `on_net_protocol`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `protocol` | `const char*` | Protocol (e.g., "http/1.1", "h2") |
| `protocol_len` | `size_t` | Protocol string length |

**qcontrol_net_send_event_t** - passed to `on_net_send`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `buf` | `const void*` | Data being sent |
| `count` | `size_t` | Byte count |

**qcontrol_net_recv_event_t** - passed to `on_net_recv`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `buf` | `void*` | Data received |
| `count` | `size_t` | Requested byte count |
| `result` | `ssize_t` | Bytes received or negative errno |

**qcontrol_net_close_event_t** - passed to `on_net_close`:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `result` | `int` | 0 on success, negative errno |

### Network Actions

**Return values from `on_net_connect`/`on_net_accept`:**

| Macro | Description |
|-------|-------------|
| `QCONTROL_NET_PASS` | No interception |
| `QCONTROL_NET_BLOCK` | Block with EACCES |
| `QCONTROL_NET_BLOCK_WITH(errno)` | Block with custom errno |
| `QCONTROL_NET_SESSION(session)` | Intercept with config |
| `QCONTROL_NET_STATE(ptr)` | Track state only |

**Return values from send/recv callbacks:**

| Macro | Description |
|-------|-------------|
| `QCONTROL_NET_PASS` | Continue normally |
| `QCONTROL_NET_BLOCK` | Block operation |
| `QCONTROL_NET_BLOCK_WITH(errno)` | Block with custom errno |

### Network Sessions

```c
static qcontrol_net_action_t on_net_connect(qcontrol_net_connect_event_t* ev) {
    qcontrol_net_session_t session = {
        .state = my_state,

        // Connection modifications (connect only)
        .set_addr = "proxy.example.com",
        .set_port = 8080,

        // I/O transforms
        .send_config = &send_cfg,
        .recv_config = &recv_cfg,
    };
    return QCONTROL_NET_SESSION(session);
}
```

### Network Context

The `qcontrol_net_ctx_t` type in transform functions provides connection metadata:

| Field | Type | Description |
|-------|------|-------------|
| `fd` | `int` | Socket fd |
| `direction` | `qcontrol_net_direction_t` | `QCONTROL_NET_OUTBOUND` or `QCONTROL_NET_INBOUND` |
| `src_addr` | `const char*` | Source address |
| `src_addr_len` | `size_t` | Source address length |
| `src_port` | `uint16_t` | Source port |
| `dst_addr` | `const char*` | Destination address |
| `dst_addr_len` | `size_t` | Destination address length |
| `dst_port` | `uint16_t` | Destination port |
| `is_tls` | `int` | Whether TLS is active |
| `tls_version` | `const char*` | TLS version |
| `tls_version_len` | `size_t` | TLS version length |
| `domain` | `const char*` | Domain name (if discovered) |
| `domain_len` | `size_t` | Domain name length |
| `protocol` | `const char*` | Protocol (if detected) |
| `protocol_len` | `size_t` | Protocol length |

## Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `QCONTROL_PLUGINS` | (none) | Comma-separated paths to plugin `.so` files |
| `QCONTROL_BUNDLE` | (none) | Path to bundled plugin `.so` file |
| `QCONTROL_LOG_FILE` | `/tmp/qcontrol.log` | Log file path for Logger utility |

## License

Apache License 2.0
