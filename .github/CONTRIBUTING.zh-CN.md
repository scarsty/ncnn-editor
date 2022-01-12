## 如何提交代码

### 一、fork 分支

在浏览器中打开 [shufaCV](https://github.com/scarsty/shufaCV/), `fork` 到自己的 repositories，例如

```shell
https://github.com/zchrissirhcz/shufaCV.git
```

clone 项目到本地，添加官方 remote 并 fetch:

```shell
git clone https://github.com/zchrissirhcz/shufaCV.git && cd shufaCV
git remote add upstream https://github.com/scarsty/shufaCV.git
git fetch upstream
```

对于 `git clone` 下来的项目，它现在有两个 remote，分别是 origin 和 upstream：

```shell
git remote -v
origin  https://github.com/zchrissirhcz/shufaCV.git (fetch)
origin  https://github.com/zchrissirhcz/shufaCV.git (push)
upstream    https://github.com/scarsty/shufaCV.git (fetch)
upstream    https://github.com/scarsty/shufaCV.git (push)
```

origin 指向你 fork 的仓库地址；upstream 即官方 repo。可以基于不同的 remote 创建和提交分支。

例如切换到官方 main 分支，并基于此创建自己的分支（命名尽量言简意赅。一个分支只做一件事，方便 review 和 revert）

```shell
git rebase upstream/main
git checkout -b fix-readme
```

或创建分支时指定基于官方 main 分支：

```shell
git checkout -b fix-readme upstream/main
```

> `git fetch` 是从远程获取最新代码到本地。如果是第二次 pr shufaCV，直接从 `git fetch upstream` 开始即可，不需要 `git remote add upstream`，也不需要修改 `github.com/zchrissirhcz/shufaCV`。

### 二、代码习惯

为了增加沟通效率，reviewer 一般要求 contributor 遵从以下规则

* `if-else` 和花括号 `{` 中间需要换行
* 不能随意增删空行
* tab 替换为 4 个空格
* 为了保证平台兼容性，尽量增加CI测试
* 若是新增功能或平台，`test` 目录需有对应测试用例
* 文档放到 `doc` 对应目录下，中文用 `.zh-CN.md` 做后缀；英文直接用 `.md` 后缀
* 同时我们也提供了如下 python 脚本在本地自动化修复格式

  ```shell
  python tools/run_clang_format.py --files shufaCV tests --recursive \
      --clang_format_executable clang-format --style file --fix
  ```

开发完成后提交到自己的 repository

```shell
git commit -a
git push origin fix-readme
```

推荐使用 [`commitizen`](https://pypi.org/project/commitizen/) 或 [`gitlint`](https://jorisroovers.com/gitlint/) 等工具格式化 commit message，方便事后检索海量提交记录

### 三、代码提交

浏览器中打开 [scarsty pulls](https://github.com/scarsty/shufaCV/pulls) ，此时应有此分支 pr 提示，点击 `Compare & pull request`

* 标题**必须**是英文。未完成的分支应以 `WIP:` 开头，例如 `WIP: add conv int8`
* 正文宜包含以下内容，中英不限
  * 内容概述和实现方式
  * 功能或性能测试
  * 测试结果

CI 已集成了自动格式化，需要 merge 自动 restyled 的分支，例如

```shell
git fetch upstream
git checkout fix-readme
git merge upstream/restyled/pull-22
git push origin fix-readme
```

回到浏览器签署 CLA，所有 CI 测试通过后通知 reviewer merge 此分支。

### 四、彩蛋

留下个人 qq 号会触发隐藏事件。
