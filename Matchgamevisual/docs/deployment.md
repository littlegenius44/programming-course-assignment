# GitHub Pages 部署

已发布仓库：https://github.com/littlegenius44/programming-course-assignment

在线网址：https://littlegenius44.github.io/programming-course-assignment/

本次用户仅批准发布Web。GitHub仓库保留Matchgamevisual子目录，以及.github/workflows/deploy-web.yml；命令行版、raylib、SDK、压缩包和课程材料暂时留在本地。

## 构建与自动部署

main上Matchgamevisual或Web工作流变更触发Actions，也可从Actions手动运行Deploy MatchLab Web to GitHub Pages。工作流使用Node24和npm ci，先测试，再类型检查/构建，最后上传静态网站并部署Pages。

工作流读取configure-pages输出的base_path，并通过vite build --base传入。网址子路径为/programming-course-assignment/，现有图片配置使用import.meta.env.BASE_URL，因此脚本、样式和图片共享正确前缀。本地npm run dev仍使用默认根路径。

```powershell
Set-Location 'E:\AI\programming-course-assignment\Matchgamevisual'
npm run test
npm run build -- --base /programming-course-assignment/ --outDir dist-github
npm run preview -- --base /programming-course-assignment/ --outDir dist-github
```

dist-github仅用于本地部署路径验证并已忽略。GitHub使用全新checkout生成dist，不上传本地保留的旧哈希产物；生产网站只包含当前构建内容。

## 本地与远程历史

本地matchlab/stage-0已有课程初始提交，其祖先包含旧命令行版。远程使用独立的Web-only根提交，只包含一个Web忽略文件；后续发布提交包含已核对的Matchgamevisual及.github树，不包含原C祖先、未完成文件或课程材料。本地web/publish对应远程main，当前工作分支保持matchlab/stage-0。

本次Git HTTPS连接被重置，改用已登录GitHub CLI的API上传Git对象。50个发布文件的blob SHA和整棵tree SHA均与本地Git一致，远程提交经过SHA校验导入本地；main只执行正常快进更新，不使用force。该传输替代不改变发布范围或Git历史约束。

本次不切换当前工作分支。后续Web更新先仅提交Matchgamevisual与Web工作流，再基于上次web/publish创建相同范围的新发布提交并普通推送。其余版本准备完成后，再按当时明确批准的范围加入远程main；不要直接推送本地所有分支或使用force。

## 验证依据

- Vite官方部署说明：https://vite.dev/guide/static-deploy.html#github-pages
- GitHub Pages自定义工作流：https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages

线上部署状态以Actions结果和实际网址检查为准。

## 2026-10-13首次部署验收

游戏部署提交：3db744ec952e11dc16d91961d1373b20bbf59db3。本地完整Web提交：ba3bee5597cb5da8a9af2632766a6c0c604882fa。首次运行：https://github.com/littlegenius44/programming-course-assignment/actions/runs/38343255990 ，build及deploy均成功。

GitHub Linux运行122项正式测试、类型检查、Pages子路径构建及部署通过。线上页面、脚本、样式、三张图片请求均200；1366×768普通/演示棋盘同尺寸，说明在右；k4的H/按钮提示一致，按真实坐标点击后成功消除并更新侧栏摘要，暂停冻结计时、恢复继续。浏览器控制台错误/警告及请求失败均为0。

远程发布清单核对50个文件，仅Web及一个部署工作流；命令行、raylib、SDK、压缩包和课程材料未上传，本地修改未暂存。未删除文件、新增游戏依赖、制作截图或临时脚本。部署后的记录更新仅修改文档，使用[skip ci]，线上游戏代码及资源保持首次通过验收的版本。
