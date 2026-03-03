# C SDK Examples

Example plugins demonstrating the qcontrol C SDK for file operation filtering.

## Plugins

| Plugin | Description |
|--------|-------------|
| file-logger | Logs all file operations to a log file |
| access-control | Blocks access to `/tmp/secret*` paths |
| byte-counter | Counts bytes read/written per file |
| content-filter | Redacts sensitive data in `.txt`/`.log` files |
| text-transform | Transforms text based on file extension |
| exec-logger | Logs all exec operations (v1 - not yet implemented) |
| net-logger | Logs all network operations (v1 - not yet implemented) |

## Building

```bash
make build   # Build all plugins (.so for dynamic loading)
make dist    # Build all plugins (.o for bundling)
make clean   # Remove all built plugins
```

Each plugin outputs to its own `dist/` directory:
- **Shared library** — `dist/<name>-<arch>.so`
- **Object file** — `dist/<name>-<arch>.o` (for bundling)


## Demo: Zero-Trust Governance

You can use qcontrol to build unbreakable system-level guardrails for *any* application—from standard Linux utilities to autonomous AI coding agents.

Instead of relying on application logic or API restrictions, qcontrol intercepts system calls at the OS level to guarantee compliance without modifying the target binary.

**1. Start the Dev Environment**
We have pre-configured a development container with the SDK, compiler toolchain, and Anthropic's Claude Code AI assistant installed.
```bash
cd examples && make dev
```

**2. Build the Plugins**
```bash
make build
```

**3. Set up the Demo**
Let's use the `access-control` plugin to protect a mock API key file.
```bash
echo "super_secret_key_123" > /tmp/secret_api_key.txt
```

**4. Watch the OS block the read**
Launch the standard `cat` utility, but wrap it in qcontrol's access-control policy:
```bash
ARCH=$(uname -m)-$(uname -s | tr A-Z a-z)
QCONTROL_PLUGINS=./access-control/dist/access-control-$ARCH.so qcontrol wrap -- cat /tmp/secret_api_key.txt
```

**What Happens:**
`cat` will attempt to read the file, but qcontrol will intercept and deny the `open()` syscall at the C ABI boundary.
```text
[access_control] BLOCKED: /tmp/secret_api_key.txt
cat: /tmp/secret_api_key.txt: Permission denied
```

### Next Step: Sandboxing Autonomous AI

Because qcontrol works at the system level, you can wrap autonomous AI tools to create unbreakable guardrails against prompt injections. The dev container has Anthropic's Claude Code CLI pre-installed to test this.

If you have an Anthropic Console account, you can try sandboxing the AI:

```bash
# 1. Authenticate the AI
claude -p "hello"

# 2. Command the AI to read the secret file, but wrap it in our policy
QCONTROL_PLUGINS=./access-control/dist/access-control-$ARCH.so qcontrol wrap -- claude -p "Read /tmp/secret_api_key.txt and summarize its contents."
```

Claude will hit the system-level block, realize it is sandboxed, and gracefully respond: *"I cannot complete this request because I received a permission denied error trying to read the file."*


## Usage

```bash
ARCH=$(uname -m)-$(uname -s | tr A-Z a-z)

# Single plugin
QCONTROL_PLUGINS=./file-logger/dist/file-logger-$ARCH.so qcontrol wrap -- ls -la

# Multiple plugins
QCONTROL_PLUGINS=./file-logger/dist/file-logger-$ARCH.so,./access-control/dist/access-control-$ARCH.so \
  qcontrol wrap -- cat /tmp/secret_test.txt
```

## Bundling

```bash
# Build plugins as object files
make dist

# Bundle using config file
qcontrol bundle --config bundle.toml
# Creates: c-plugins.so

# Use the bundle
qcontrol wrap --bundle c-plugins.so -- ./target_app
```

## Testing

```bash
# Run the test script with plugins
qcontrol wrap --bundle c-plugins.so -- ./test-file-ops.sh

# Check log output
cat /tmp/qcontrol.log
```

## Writing Plugins

See [file-logger/src/plugin.c](file-logger/src/plugin.c) for a complete example.

```c
#include <qcontrol.h>
#include <qcontrol/helpers.h>

static qcontrol_file_action_t on_file_open(qcontrol_file_open_event_t* ev) {
    fprintf(stderr, "open(%s) = %d\n", ev->path, ev->result);
    return QCONTROL_FILE_PASS;  // or QCONTROL_FILE_BLOCK
}

QCONTROL_PLUGIN("my_plugin",
    .on_file_open = on_file_open,
);
```

Build: `gcc -shared -fPIC -o plugin.so plugin.c -I../../include`
