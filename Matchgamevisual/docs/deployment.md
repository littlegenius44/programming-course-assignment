# GitHub Pages 部署

目标仓库：https://github.com/littlegenius44/programming-course-assignment

目标网址：https://littlegenius44.github.io/programming-course-assignment/

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

本地matchlab/stage-0已有课程初始提交，其祖先包含旧命令行版。首次发布从已核对的Matchgamevisual及.github树建立独立根提交，使用本地web/publish分支推送远程main；不会把原C提交、未完成文件或课程材料一起上传。

本次不切换当前工作分支。后续Web更新先仅提交Matchgamevisual与Web工作流，再基于上次web/publish创建相同范围的新发布提交并普通推送。其余版本准备完成后，再按当时明确批准的范围加入远程main；不要直接推送本地所有分支或使用force。

## 验证依据

- Vite官方部署说明：https://vite.dev/guide/static-deploy.html#github-pages
- GitHub Pages自定义工作流：https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages

线上部署状态以Actions结果和实际网址检查为准。
