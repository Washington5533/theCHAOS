# 贡献指南

欢迎对混沌摆跑酷提出建议和改进！

## 如何贡献

### 报告问题
- 通过 [Issues](https://github.com/Washington5533/theCHAOS/issues) 提交 bug 或功能建议
- 描述尽量详细：复现步骤、期望行为、截图

### 提交代码
1. Fork 本仓库
2. 创建功能分支：`git checkout -b feat/your-idea`
3. 提交更改：`git commit -m "feat: 描述"`
4. 推送到你的 Fork：`git push origin feat/your-idea`
5. 创建 Pull Request

### 提交规范
- 使用 [Conventional Commits](https://www.conventionalcommits.org/) 格式
- 类型：`feat` / `fix` / `refactor` / `docs` / `chore`

## 技术栈

- **语言**: C++17
- **图形库**: SDL2 + SDL2_ttf（主版本）/ EasyX（副版本）
- **构建**: Visual Studio (MSVC)

## 开发环境

| 组件 | 版本要求 |
|------|---------|
| Visual Studio | 2022+ |
| SDL2 | 2.32.4 (已包含) |
| SDL2_ttf | 2.22.0 (已包含) |

## 项目结构

详见 [README.md](README.md) 和 [CHECKPOINTS.md](CHECKPOINTS.md)

## 许可证

本项目为课程项目，仅供学习和参考。第三方库（SDL2、EasyX）遵循各自的许可证。
