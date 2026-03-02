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

## Building

```bash
make build   # Build all plugins (.so for dynamic loading)
make dist    # Build all plugins (.o for bundling)
make clean   # Remove all built plugins
```

Each plugin outputs to its own `dist/` directory:
- **Shared library** — `dist/<name>-<arch>.so`
- **Object file** — `dist/<name>-<arch>.o` (for bundling)

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
