# MCP Client 组件迁移包

这是一个完整的 MCP Client 组件包，可以直接复制到新的 ESP-IDF 项目中使用。

## 目录结构

```
mcp_client_package/
├── mcp_client/              # 组件目录（复制到新项目的 components/ 下）
│   ├── mcp_client.h
│   ├── mcp_client.c
│   ├── CMakeLists.txt
│   ├── idf_component.yml
│   └── README.md
├── AI_MIGRATION_PROMPT.md   # AI 迁移提示词
├── MIGRATION_GUIDE.md       # 迁移指南
├── QUICK_START.md           # 快速开始
├── MIGRATION_INSTRUCTIONS.md # 迁移说明
└── verify_component.sh      # 验证脚本
```

## 使用方法

1. 将 `mcp_client/` 目录复制到新项目的 `components/` 目录下
2. 按照 `MIGRATION_INSTRUCTIONS.md` 中的步骤完成迁移
3. 运行 `verify_component.sh` 验证组件完整性

## 更多信息

- 详细迁移步骤：`MIGRATION_INSTRUCTIONS.md`
- AI 提示词：`AI_MIGRATION_PROMPT.md`
- 快速开始：`QUICK_START.md`
