# Web GitHub Pages 发布计划

用户已批准新建公开仓库littlegenius44/programming-course-assignment，只上传Web；命令行和raylib修改继续留在本地，稍后再上传。发布范围为Matchgamevisual以及根目录的Web部署工作流。

**目标：** 发布可实际游玩的GitHub Pages网址，保留原本地工作区及未完成修改。

**架构：** GitHub Actions安装锁定依赖、运行正式测试、按Pages base_path构建并部署dist。普通本地开发仍使用根路径；线上图片与代码使用现有BASE_URL接口。工作流仅由main上的Web变更或手动操作触发。

**技术栈：** Git、GitHub CLI、GitHub Actions/Pages、Node24及现有Vite/Vitest，不新增游戏依赖。

状态：2026-10-13完成Web部署，课程其他版本继续留在本地。

- [x] 检查登录、目标仓库、所有改动及暂存区，记录其他版本的工作区状态。
- [x] 添加.github/workflows/deploy-web.yml，固定官方Action提交，build/deploy分开授予最小权限。
- [x] 本地运行122项正式测试，以/programming-course-assignment/子路径输出到独立dist-github，验证页面、图片、演示及提示。
- [x] 更新部署说明，暂存并提交仅Web及工作流文件。核对没有Matchgame、raylib、课程材料或压缩包变更。
- [x] 从Web与工作流的真实Git树创建不含本地旧C版本祖先的首次发布提交；使用web/publish分支推送远程main，不切换或重写当前工作分支。
- [x] 创建公开仓库、配置GitHub Pages为workflow模式、启动Actions并检查完整日志及部署结果。
- [x] 检查线上页面与资源200、演示布局、H提示后真实交换、暂停恢复及控制台健康；记录线上链接和提交。

不上传本地旧dist、node_modules、C/raylib或课程材料，也不删除任何文件。GitHub首次发布历史独立于本地课程初始提交；后续Web发布提交接在web/publish上，普通推送无需force。其余课程版本准备好后另行合入，不能直接推送包含未批准目录的本地历史。

验收记录：本地122项测试、类型检查、独立dist-github子路径构建及浏览器交互通过。Web变更在本地matchlab/stage-0提交ba3bee5；远程以单个Web忽略文件初始化独立根，再发布真实50文件树，不连接原C提交。Git传输连接失败后通过GitHub API上传，同一blob/tree SHA及发布提交校验通过，main快进至3db744e，本地web/publish及origin/main与其一致。

Pages已设workflow模式，首次运行38343255990的build/deploy成功，Linux122项测试、类型检查与构建通过。线上页面和三张图片200，侧栏、k4提示交换、摘要及暂停恢复通过，无浏览器控制台错误/警告或请求失败。当前本地其他版本的修改、SDK及课程材料保留，未纳入暂存或远程历史。只读发布复核未发现重要问题；部署后仅文档记录更新跳过CI，游戏产物不变。
