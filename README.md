## node-editor

基于 [imnodes](https://github.com/Nelarius/imnodes) 的流程图编辑工具。

目前的主要功能是用于神经网络结构的简单编辑，或者某些特殊流程图的编辑。

目标是老妪能训。

注意除了main.cpp和color_node_editor.cpp与imnodes给出的范例不同之外，其余文件均未改动。

### 操作

* 按住中键可以拖动整个图。
* 右键可以新建节点。
* 左键可以单选或框选节点和连接。
* 选中的节点可以拖动其位置（一些格式中，位置可能不会被保存）。
* Del删除选中的节点和连接。
* 节点中的框是可以编辑的，目前的功能比较简单。

### 支持的格式

ini范例，[ncnn](https://github.com/Tencent/ncnn/wiki/param-and-model-file-structure)。

<img src='https://raw.githubusercontent.com/scarsty/node-editor/master/images/lenet.png'/>
<img src='https://raw.githubusercontent.com/scarsty/node-editor/master/images/ncnn.png'/>
