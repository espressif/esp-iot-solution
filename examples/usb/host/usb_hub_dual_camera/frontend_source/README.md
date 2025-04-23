# USB-Hub Dual Camera Web Display Component

## Build Guide

Please install `node >= 20`. It is recommended to use `pnpm` as the package manager.

In the `frontend_source` directory:

```bash
pnpm install    # install dependencies
# You may need to run `pnpm approve-builds` to execute the postinstall script for some dependencies.
pnpm build      # build

# Gzip compress the build artifacts and move them to the spiffs directory.
./../scripts/gzip_webserver_files.sh
```
