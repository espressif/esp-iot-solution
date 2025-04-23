# USB-Hub Dual Camera 网页显示组件

## 编译指南

请安装 `node >= 20`，推荐使用 `pnpm` 作为包管理器。

在 `frontend_source` 目录下：

```bash
pnpm install    # install dependencies
# You may need to run `pnpm approve-builds` to execute the postinstall script for some dependencies.
pnpm build      # build

# Gzip compress the build artifacts and move them to the spiffs directory.
./../scripts/gzip_webserver_files.sh
```
