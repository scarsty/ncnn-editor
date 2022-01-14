# nn-editor

<div align="right">

<img src="assets/logo.png" width="100px">

</div>

基于 [imnodes](https://github.com/Nelarius/imnodes) 的流程图编辑工具。

目前的主要功能是用于神经网络结构的简单编辑，或者某些特殊流程图的编辑。

目标是老妪能训。

QQ 群: 822911263

### 操作

* 按住中键，或者Alt+左键可以拖动整个图。
* 右键可以新建节点。
* 左键可以单选或框选节点和连接。
* 选中的节点可以拖动其位置（一些格式中，位置可能不会被保存）。
* Del删除选中的节点和连接，也可以使用右键选项删除。
* 选中一个节点时，节点会变大，此时可以编辑节点内容，目前的功能比较简单。
* 右键点某个节点可以建立连接点或清除空连接点。空连接点也可以不必清除，保存时会忽略。

### 支持的格式

* ini范例
* [ncnn](https://github.com/Tencent/ncnn/wiki/param-and-model-file-structure)
* [pnnx](https://github.com/pnnx/pnnx) 该格式与ncnn基本一致，故可以直接支持，实际因为参数列表较长体验有些问题

请大佬们pr支持新格式！

<div align="center">

<img src="assets/lenet.png">
<img src="assets/ncnn.png">
<img src="assets/ncnn2.png">

</div>

## 授权

```bash
以 BSD 3-Clause License 授权发布。若将其商业应用，我们建议您提交一张书法照到官方 QQ 群。
```
