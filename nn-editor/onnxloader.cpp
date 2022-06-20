#include "onnxloader.h"


#include <iostream>
#include <fstream>
#include <sstream>

#include "yaml-cpp/yaml.h"

#include "fmt1.h"

#define ONNX_ML

#include "onnx/onnx_pb.h"


int calculateTensorDims
(
    const onnx::GraphProto& graph_proto,
    std::map<int, std::map<std::string, std::string>>& net,
    std::map<int, std::map<std::string, std::vector<int>>>& tensorDims
)
{
    std::map<std::string, std::vector<int>> input_tensor_dim_map;

    //Inputs to the graph.
    for (int i = 0; i < graph_proto.input_size(); i++)
    {
        const onnx::ValueInfoProto& value_info_proto = graph_proto.input(i);
        std::string layer_input = value_info_proto.name();
        std::vector<int> dims;

        const onnx::TypeProto& type_proto = value_info_proto.type();
        const onnx::TypeProto::Tensor& tensor = type_proto.tensor_type();
        const onnx::TensorShapeProto& tensor_shape = tensor.shape();

        for (int j = 0; j < tensor_shape.dim_size(); j++)
        {
            const onnx::TensorShapeProto::Dimension& dimension = tensor_shape.dim(j);
            dims.push_back(dimension.dim_value());
        }

        input_tensor_dim_map[layer_input] = dims;
    }

    for (int i = 0; i < net.size(); i++)
    {
        std::map<std::string, std::string> layer_details = net.find(i)->second;
        std::string layer_type = layer_details.find("type")->second;
        std::string layer_input = layer_details.find("input")->second;
        std::string layer_output = layer_details.find("output")->second;
        int in_w, in_h, in_c, in_n;
        int out_w, out_h, out_c, out_n;
        std::vector<int> output_dims;

        std::vector<int> input_dims = input_tensor_dim_map.find(layer_input)->second;
        in_n = input_dims[0]; in_c = input_dims[1]; in_h = input_dims[2]; in_w = input_dims[3];
        std::map<std::string, std::vector<int>> in_out_map;
        in_out_map[layer_input] = input_dims;

        if (layer_type == "Conv")
        {
            std::string layer_weights = " ";
            std::vector<int> weight_dims, bias_dims;
            if (layer_details.size() > 4)
            {
                layer_weights = layer_details.find("weights")->second;
                weight_dims = input_tensor_dim_map.find(layer_weights)->second;
            }
            std::string layer_bias = " ";
            if (layer_details.size() > 5)
            {
                layer_bias = layer_details.find("bias")->second;
                bias_dims = input_tensor_dim_map.find(layer_bias)->second;
            }
            std::string params = layer_details.find("params")->second;

            int kernel_w, kernel_h, pad_w, pad_h, stride_w, stride_h, dilation_w, dilation_h;
            std::stringstream ss(params);
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h;

            out_w = ((in_w + 2 * (pad_w)-kernel_w - (kernel_w - 1) * (dilation_w - 1)) / stride_w) + 1;
            out_h = ((in_h + 2 * (pad_h)-kernel_h - (kernel_h - 1) * (dilation_h - 1)) / stride_h) + 1;
            out_c = weight_dims[0];
            out_n = in_n;

            if (layer_details.size() > 4)
            {
                in_out_map[layer_weights] = weight_dims;
            }

            if (layer_details.size() > 5)
            {
                in_out_map[layer_bias] = bias_dims;
            }
        }
        else if (layer_type == "Relu")
        {
            out_w = in_w;
            out_h = in_h;
            out_c = in_c;
            out_n = in_n;
        }
        else if (layer_type == "LRN")
        {
            out_w = in_w;
            out_h = in_h;
            out_c = in_c;
            out_n = in_n;
        }
        else if (layer_type == "Dropout")
        {
            out_w = in_w;
            out_h = in_h;
            out_c = in_c;
            out_n = in_n;
        }
        else if (layer_type == "MaxPool")
        {
            std::string params = layer_details.find("params")->second;
            std::stringstream ss(params);
            int kernel_w, kernel_h, pad_w, pad_h, stride_w, stride_h;
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h;

            out_w = static_cast<int>(ceil(static_cast<float> (in_w + 2 * pad_w + stride_w - kernel_w) / stride_w));
            out_h = static_cast<int>(ceil(static_cast<float> (in_h + 2 * pad_h + stride_h - kernel_h) / stride_h));
            if (pad_h > 0) if ((out_h - 1) * stride_h >= (in_h + pad_h)) out_h = out_h - 1;
            if (pad_w > 0) if ((out_w - 1) * stride_w >= (in_w + pad_w)) out_w = out_w - 1;

            out_c = in_c;
            out_n = in_n;
        }
        else if (layer_type == "Gemm")
        {

            std::string layer_weights = " ";
            std::vector<int> weight_dims, bias_dims;
            std::vector<int> weight_dims_gemm;
            if (layer_details.size() > 4)
            {
                layer_weights = layer_details.find("weights")->second;
                weight_dims = input_tensor_dim_map.find(layer_weights)->second;
                weight_dims_gemm.push_back(in_w);
                weight_dims_gemm.push_back(in_h);
                weight_dims_gemm.push_back(in_c);
                weight_dims_gemm.push_back(weight_dims[0]);

            }
            std::string layer_bias = " ";
            if (layer_details.size() > 5)
            {
                layer_bias = layer_details.find("bias")->second;
                bias_dims = input_tensor_dim_map.find(layer_bias)->second;
            }

            out_n = 1;
            out_c = weight_dims[0];
            out_h = 1;
            out_w = 1;

            if (layer_details.size() > 4)
            {
                in_out_map[layer_weights] = weight_dims_gemm;
            }

            if (layer_details.size() > 5)
            {
                in_out_map[layer_bias] = bias_dims;
            }
        }

        output_dims.push_back(out_n);
        output_dims.push_back(out_c);
        output_dims.push_back(out_h);
        output_dims.push_back(out_w);
        input_tensor_dim_map[layer_output] = output_dims;
        in_out_map[layer_output] = output_dims;

        tensorDims[i] = in_out_map;
    }

    return 0;
}

void formatFileName(std::string& str, const std::string& from, const std::string& to)
{
    //Written to avoid conflicts with file creation with filenames that contain "/"
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != std::string::npos)
    {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // Handles case where 'to' is a substring of 'from'
    }
}

int writeGDF
(
    std::ofstream& ofsGDF,
    std::map<int, std::map<std::string, std::string>> net,
    std::map<int, std::map<std::string, std::vector<int>>> tensorDims
)
{
    std::cout << "INFO: Writing the GDF " << std::endl;
    ofsGDF << "import vx_nn" << std::endl;
    ofsGDF << std::endl;

    //network input.
    std::map<std::string, std::string> first_layer = net.find(0)->second;
    std::map<std::string, std::vector<int>> first_layer_dims = tensorDims.find(0)->second;
    auto&& layer_input = first_layer.find("input")->second;
    auto& input_dims = first_layer_dims.find(layer_input)->second;
    formatFileName(layer_input, "/", "_");
    ofsGDF << "data " << layer_input << " = tensor:4,{" << input_dims[3] << "," << input_dims[2] << "," << input_dims[1] << "," << input_dims[0] << "},"
        << "VX_TYPE_FLOAT32,0" << std::endl;
    ofsGDF << "read " << layer_input << " input.f32" << std::endl;

    for (int i = 0; i < net.size(); i++)
    {
        std::map<std::string, std::string> layer_details = net.find(i)->second;
        std::map<std::string, std::vector<int>> in_out_map = tensorDims.find(i)->second;

        auto&& layer_type = layer_details.find("type")->second;
        auto&& layer_output = layer_details.find("output")->second;
        auto&& layer_input = layer_details.find("input")->second;

        //output dims.
        auto& output_dims = in_out_map.find(layer_output)->second;
        formatFileName(layer_output, "/", "_");
        ofsGDF << "data " << layer_output << " = tensor:4,{" << output_dims[3] << "," << output_dims[2] << "," << output_dims[1] << "," << output_dims[0] << "},"
            << "VX_TYPE_FLOAT32,0" << std::endl;

        //TODO: Generate dims of layers and create nodes.
        if (layer_type == "Conv")
        {
            auto&& layer_params = layer_details.find("params")->second;
            int kernel_w, kernel_h, pad_w, pad_h, dilation_w, dilation_h, stride_w, stride_h;
            std::stringstream ss(layer_params);
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h >> dilation_w >> dilation_h;
            auto&& layer_weights = layer_details.find("weights")->second;
            auto& weight_dims = in_out_map.find(layer_weights)->second;

            if (layer_details.size() > 4)
            {
                formatFileName(layer_weights, "/", "_");
                ofsGDF << "data " << layer_weights << " = tensor:4,{" << weight_dims[3] << "," << weight_dims[2] << "," << weight_dims[1] << ","
                    << weight_dims[0] << "}," << "VX_TYPE_FLOAT32,0" << std::endl;
                ofsGDF << "init " << layer_weights << " weights/" << layer_weights << ".f32" << std::endl;
            }

            std::string layer_bias;
            if (layer_details.size() > 5)
            {
                layer_bias = layer_details.find("bias")->second;
                auto& bias_dims = in_out_map.find(layer_bias)->second;
                formatFileName(layer_bias, "/", "_");
                ofsGDF << "data " << layer_bias << " = tensor:1,{" << bias_dims[0] << "},VX_TYPE_FLOAT32,0" << std::endl;
                ofsGDF << "init " << layer_bias << " weights/" << layer_bias << ".f32" << std::endl;
            }
            else if (layer_details.size() == 5)
            {
                layer_bias = layer_output + "_b";
                ofsGDF << "data " << layer_bias << " = tensor:1,{" << weight_dims[0] << "},VX_TYPE_FLOAT32,0" << std::endl;
            }

            //conv params.
            ofsGDF << "data " << layer_output << "_params =" << " scalar:VX_TYPE_NN_CONV_PARAMS,{" << pad_w << "," << pad_h << ","
                << "VX_CONVERT_POLICY_SATURATE" << "," << "VX_ROUND_POLICY_TO_NEAREST_EVEN" << ",VX_NN_DS_SIZE_ROUNDING_FLOOR,"
                << dilation_w - 1 << "," << dilation_h - 1 << "}" << std::endl;

            //conv node.
            ofsGDF << "node org.khronos.nn_extension.convolution_layer " << layer_input << " " << layer_weights << " "
                << layer_bias << " " << layer_output << "_params" << " " << layer_output << std::endl;

        }
        else if (layer_type == "Relu")
        {
            ofsGDF << "data " << layer_output << "_mode = " << "scalar:VX_TYPE_ENUM,VX_NN_ACTIVATION_RELU" << std::endl;
            ofsGDF << "data " << layer_output << "_param_a = " << "scalar:VX_TYPE_FLOAT32,0" << std::endl;
            ofsGDF << "data " << layer_output << "_param_b = " << "scalar:VX_TYPE_FLOAT32,0" << std::endl;
            ofsGDF << "node org.khronos.nn_extension.activation_layer " << layer_input << " " << layer_output << "_mode" << " "
                << layer_output << "_param_a" << " " << layer_output << "_param_b" << " " << layer_output << std::endl;
        }
        else if (layer_type == "MaxPool")
        {
            auto&& layer_params = layer_details.find("params")->second;
            std::stringstream ss(layer_params);
            int kernel_w, kernel_h, pad_w, pad_h, stride_w, stride_h;
            ss >> kernel_w >> kernel_h >> stride_w >> stride_h >> pad_w >> pad_h;

            ofsGDF << "data " << layer_output << "_type = " << "scalar:VX_TYPE_ENUM,VX_NN_POOLING_MAX" << std::endl;
            ofsGDF << "data " << layer_output << "_kernel_w = " << "scalar:VX_TYPE_SIZE," << kernel_w << std::endl;
            ofsGDF << "data " << layer_output << "_kernel_h = " << "scalar:VX_TYPE_SIZE," << kernel_h << std::endl;
            ofsGDF << "data " << layer_output << "_pad_w = " << "scalar:VX_TYPE_SIZE," << pad_w << std::endl;
            ofsGDF << "data " << layer_output << "_pad_h = " << "scalar:VX_TYPE_SIZE," << pad_h << std::endl;
            ofsGDF << "data " << layer_output << "_roundPolicy = " << "scalar:VX_TYPE_ENUM,VX_ROUND_POLICY_TO_NEAREST_EVEN" << std::endl;
            ofsGDF << "node " << "org.khronos.nn_extension.pooling_layer " << layer_input << " "
                << layer_output << "_type " << layer_output << "_kernel_w " << layer_output << "_kernel_h "
                << layer_output << "_pad_w " << layer_output << "_pad_h "
                << layer_output << "_roundPolicy"
                << " " << layer_output
                << std::endl;
        }
        else if (layer_type == "LRN")
        {
            auto&& layer_params = layer_details.find("params")->second;
            std::stringstream ss(layer_params);
            int lrn_local_size;
            float alpha, beta, bias;
            ss >> lrn_local_size >> alpha >> beta >> bias;

            ofsGDF << "data " << layer_output << "_mode = " << "scalar:VX_TYPE_ENUM,VX_NN_NORMALIZATION_ACROSS_MAPS" << std::endl;
            ofsGDF << "data " << layer_output << "_size = " << "scalar:VX_TYPE_SIZE," << lrn_local_size << std::endl;
            ofsGDF << "data " << layer_output << "_alpha = " << "scalar:VX_TYPE_FLOAT32," << alpha << std::endl;
            ofsGDF << "data " << layer_output << "_beta = " << "scalar:VX_TYPE_FLOAT32," << beta << std::endl;
            ofsGDF << "data " << layer_output << "_bias = " << "scalar:VX_TYPE_FLOAT32," << bias << std::endl;
            ofsGDF << "node org.khronos.nn_extension.normalization_layer " << layer_input << " "
                << layer_output << "_mode "
                << layer_output << "_size "
                << layer_output << "_alpha "
                << layer_output << "_beta "
                << layer_output << "_bias "
                << std::endl;
        }
        else if (layer_type == "Gemm")
        {
            int stride_w, stride_h, pad_w, pad_h, dilation_w, dilation_h;
            stride_w = 1; stride_h = 1; pad_w = 0; pad_h = 0; dilation_w = 1; dilation_h = 1;

            auto&& layer_weights = layer_details.find("weights")->second;
            auto& weight_dims = in_out_map.find(layer_weights)->second;

            if (layer_details.size() > 4)
            {
                formatFileName(layer_weights, "/", "_");
                ofsGDF << "data " << layer_weights << " =tensor:4,{" << weight_dims[0] << "," << weight_dims[1] << "," << weight_dims[2]
                    << "," << weight_dims[3] << "}," << "VX_TYPE_FLOAT32,0" << std::endl;
                ofsGDF << "init " << layer_weights << " weights/" << layer_weights << ".f32" << std::endl;
            }

            std::string layer_bias;
            if (layer_details.size() > 5)
            {
                layer_bias = layer_details.find("bias")->second;
                std::vector<int> bias_dims = in_out_map.find(layer_bias)->second;
                formatFileName(layer_bias, "/", "_");
                ofsGDF << "data " << layer_bias << " = tensor:1,{" << bias_dims[0] << "}, VX_TYPE_FLOAT32,0" << std::endl;
                ofsGDF << "init " << layer_bias << " weights/" << layer_bias << ".f32" << std::endl;
            }
            else if (layer_details.size() == 5)
            {
                layer_bias = layer_output + "_b";
                ofsGDF << "data " << layer_bias << " = tensor:1,{" << weight_dims[3] << "},VX_TYPE_FLOAT32,0" << std::endl;
            }

            ofsGDF << "data " << layer_output << "_params = " << "scalar:VX_TYPE_NN_CONV_PARAMS,{" << pad_w << "," << pad_h << ","
                << "VX_CONVERT_POLICY_SATURATE, VX_ROUND_POLICY_TO_NEAREST_EVEN,VX_NN_DS_SIZE_ROUNDING_FLOOR,0,0}" << std::endl;

            ofsGDF << "node org.khronos.nn_extension.convolution_layer " << layer_input << " " << layer_weights << " " << layer_bias
                << " " << layer_output << "_params" << " " << layer_output << std::endl;
        }
        else if (layer_type == "Dropout")
        {
            ofsGDF << "node org.khronos.openvx.copy " << layer_input << " " << layer_output << std::endl;
        }
        else if (layer_type == "Softmax")
        {
            ofsGDF << "node org.khronos.nn_extension.softmax_layer " << layer_input << " " << layer_output << std::endl;
        }

        if (i == net.size() - 1)
        {
            ofsGDF << "write " << layer_output << " output.f32" << std::endl;
        }

        ofsGDF << std::endl;
    }

    return 0;
}

int dumpOnnxModel(const onnx::GraphProto& graph_proto)
{
    for (int i = 0; i < graph_proto.initializer_size(); i++)
    {
        const onnx::TensorProto tensor_proto = graph_proto.initializer(i);
        const onnx::TensorProto_DataType datatype = onnx::TensorProto_DataType(tensor_proto.data_type());
        int tensor_size = 1;

        for (int j = 0; j < tensor_proto.dims_size(); j++)
        {
            tensor_size *= tensor_proto.dims(j);
        }

        std::string weight_file_name = tensor_proto.name();
        formatFileName(weight_file_name, "/", "_");
        std::string fileName_weights = "weights/" + weight_file_name + ".f32";

        if (datatype == onnx::TensorProto_DataType_FLOAT)
        {

            FILE* fs;
            fs = fopen(fileName_weights.c_str(), "wb");
            if (!fs)
            {
                std::cout << "ERROR: Unable to create a file, make sure weights folder is writable." << std::endl;
                return -1;
            }
            std::string raw_data_val = tensor_proto.raw_data();
            const char* val = raw_data_val.c_str();

            int count = 0;
            for (int k = 0; k < tensor_size * 4 - 4; k += 4)
            {
                float weight;
                char b[] = { val[k], val[k + 1], val[k + 2], val[k + 3] };
                memcpy(&weight, &b, sizeof(float));
                fwrite(&weight, sizeof(float), 1, fs);
                count++;
            }

            fclose(fs);
            //std::cout << "INFO: Weights dumped for: " << tensor_proto.name() <<  std::endl;
        }
        else
        {
            std::cout << "ERROR: Unsupported data types will be supported in future." << std::endl;
            return -1;
        }
    }

    return 0;
}

int getLayerParams(const onnx::NodeProto& node_proto, std::string& params)
{
    std::string layer_type = node_proto.op_type();

    if (layer_type == "Conv")
    {

        int pad_h, pad_w;
        int stride_h, stride_w;
        int kernel_h, kernel_w;
        int dilation_h = 1, dilation_w = 1;

        for (int i = 0; i < node_proto.attribute_size(); i++)
        {
            const onnx::AttributeProto& attribute_proto = node_proto.attribute(i);
            std::string attribute_name = attribute_proto.name();

            if (attribute_name == "strides")
            {
                stride_h = attribute_proto.ints(0);
                stride_w = attribute_proto.ints(1);
            }
            else if (attribute_name == "pads")
            {
                pad_h = attribute_proto.ints(0);
                pad_w = attribute_proto.ints(1);
            }
            else if (attribute_name == "kernel_shape")
            {
                kernel_h = attribute_proto.ints(0);
                kernel_w = attribute_proto.ints(1);
            }
        }

        params = std::to_string(kernel_w)
            + " " + std::to_string(kernel_h)
            + " " + std::to_string(stride_w)
            + " " + std::to_string(stride_h)
            + " " + std::to_string(pad_w)
            + " " + std::to_string(pad_h)
            + " " + std::to_string(dilation_w)
            + " " + std::to_string(dilation_h);

    }
    else if (layer_type == "MaxPool")
    {

        int pad_h, pad_w;
        int stride_h, stride_w;
        int kernel_h, kernel_w;

        for (int i = 0; i < node_proto.attribute_size(); i++)
        {
            const onnx::AttributeProto& attribute_proto = node_proto.attribute(i);
            std::string attribute_name = attribute_proto.name();

            if (attribute_name == "strides")
            {
                stride_h = attribute_proto.ints(0);
                stride_w = attribute_proto.ints(1);
            }
            else if (attribute_name == "pads")
            {
                pad_h = attribute_proto.ints(0);
                pad_w = attribute_proto.ints(1);
            }
            else if (attribute_name == "kernel_shape")
            {
                kernel_h = attribute_proto.ints(0);
                kernel_w = attribute_proto.ints(1);
            }

        }

        params = std::to_string(kernel_w)
            + " " + std::to_string(kernel_h)
            + " " + std::to_string(stride_w)
            + " " + std::to_string(stride_h)
            + " " + std::to_string(pad_w)
            + " " + std::to_string(pad_h);

    }
    else if (layer_type == "LRN")
    {

        int lrn_local_size;
        float alpha, beta, bias;

        for (int i = 0; i < node_proto.attribute_size(); i++)
        {
            const onnx::AttributeProto& attribute_proto = node_proto.attribute(i);
            std::string attribute_name = attribute_proto.name();

            if (attribute_name == "size")
            {
                lrn_local_size = attribute_proto.i();
            }
            else if (attribute_name == "alpha")
            {
                alpha = attribute_proto.f();
            }
            else if (attribute_name == "beta")
            {
                beta = attribute_proto.f();
            }
            else if (attribute_name == "bias")
            {
                bias = attribute_proto.f();
            }
        }

        params = std::to_string(lrn_local_size)
            + " " + std::to_string(alpha)
            + " " + std::to_string(beta)
            + " " + std::to_string(bias);

    }

    return 0;
}


onnxloader::onnxloader()
{
//    YAML::Node node;
//#ifdef __APPLE__
//    node = YAML::LoadFile(mainPath() + "/../Resources/ncnn-metadata.json");
//#else
//    node = YAML::LoadFile(mainPath() + "/ncnn-metadata.json");
//#endif
//    for (auto n : node)
//    {
//        //std::cout << n;
//        if (n["attributes"].IsSequence())
//        {
//            for (int i = 0; i < n["attributes"].size(); i++)
//            {
//                int_to_string_[n["name"].as<std::string>()][i] = n["attributes"][i]["name"].as<std::string>();
//                string_to_int_[n["name"].as<std::string>()][n["attributes"][i]["name"].as<std::string>()] = i;
//            }
//        }
//    }
}

void onnxloader::fileToNodes(const std::string& filename, std::deque<Node>& nodes)
{

    onnx::ModelProto model_proto;
    std::cout << "INFO: Reading the binary onnx model file." << std::endl;
    std::fstream input(filename, std::ios::in | std::ios::binary);
    bool isSuccess = model_proto.ParseFromIstream(&input);

    if (isSuccess)
    {
        std::cout << "INFO: Sucessfully read onnx model file. " << std::endl;
        if (model_proto.has_graph())
        {
            std::cout << "DEBUG: Parsing the onnx model." << std::endl;
            auto graph_proto = model_proto.graph();
            std::map<int, std::map<std::string, std::string>> net;
            //if (graph_proto.has_name())
            //{
            //    std::cout << "INFO: Extracting the weights for : " << graph_proto.name() << std::endl;
            //}

            //if (dumpOnnxModel(graph_proto) < 0)
            //{
            //    std::cout << "ERROR: Unable to dump weights from onnx model. " << std::endl;
            //    return;
            //}
            //else
            //{
            //    std::cout << "RESULT: Weights and bias extraction successful" << std::endl;
            //}

            std::cout << "INFO: Extracting the network structure for : " << graph_proto.name() << std::endl;


            for (int i = 0; i < graph_proto.node_size(); i++)
            {
                Node node;

                const onnx::NodeProto node_proto = graph_proto.node(i);
                std::string params;
                getLayerParams(node_proto, params);
                node.title = node_proto.name();
                node.type = node_proto.op_type();
                node.text = params;

                //node_proto.

                for (int k = 0; k < node_proto.attribute_size(); k++)
                {
                    auto a = node_proto.attribute(k);
                    std::string atr = a.s();
                    switch (a.type())
                    {
                    case onnx::AttributeProto_AttributeType_FLOAT:
                        atr = std::to_string(a.f());
                        break;
                    case onnx::AttributeProto_AttributeType_INT:
                        atr = std::to_string(a.i());
                        break;
                    case onnx::AttributeProto_AttributeType_STRING:
                        atr = a.s();
                        break;
                    case onnx::AttributeProto_AttributeType_TENSOR:
                        break;
                    case onnx::AttributeProto_AttributeType_GRAPH:
                        break;
                    case onnx::AttributeProto_AttributeType_SPARSE_TENSOR:
                        break;
                    case onnx::AttributeProto_AttributeType_TYPE_PROTO:
                        break;
                    case onnx::AttributeProto_AttributeType_FLOATS:
                        for (auto a1 : a.floats())
                        {
                            atr += std::to_string(a1) + " ";
                        }
                        break;
                    case onnx::AttributeProto_AttributeType_INTS:
                        for (auto a1 : a.ints())
                        {
                            atr += std::to_string(a1) + " ";
                        }
                        break;
                    case onnx::AttributeProto_AttributeType_STRINGS:
                        for (auto a1 : a.strings())
                        {
                            atr += a1 + " ";
                        }
                        break;
                    case onnx::AttributeProto_AttributeType_TENSORS:
                        break;
                    case onnx::AttributeProto_AttributeType_GRAPHS:
                        break;
                    case onnx::AttributeProto_AttributeType_SPARSE_TENSORS:
                        break;
                    case onnx::AttributeProto_AttributeType_TYPE_PROTOS:
                        break;
                    }
                    auto type_name = a.AttributeType_Name(a.type());
                    //auto pos = type_name.find("_");
                    //if (pos != std::string::npos)
                    //{
                    //    type_name = type_name.substr(pos);
                    //}
                    node.values[a.name() + "(" + type_name+ ")"] = atr;
                }

                for (int j = 0; j < node_proto.input_size(); j++)
                {
                    node.in.push_back(node_proto.input(j));
                }
                for (int j = 0; j < node_proto.output_size(); j++)
                {
                    node.out.push_back(node_proto.output(j));
                }
                node.prevs.resize(node_proto.input_size());
                //node.nexts.resize(node_proto.output_size());
                nodes.push_back(node);

                std::map<std::string, std::string> layer_details;
                layer_details["title"] = node_proto.name();
                layer_details["params"] = params;
                layer_details["type"] = node_proto.op_type();
                if (node_proto.input_size() > 0)
                {
                    layer_details["input"] = node_proto.input(0);
                }
                if (node_proto.output_size() > 0)
                {
                    layer_details["output"] = node_proto.output(0);
                }
                if (node_proto.input_size() > 1)
                {
                    std::string layer_weights = node_proto.input(1);
                    layer_details["weights"] = layer_weights;
                }
                if (node_proto.input_size() > 2)
                {
                    std::string layer_bias = node_proto.input(2);
                    layer_details["bias"] = layer_bias;
                }
                net[i] = layer_details;
            }

        }
    }
    for (auto& node1 : nodes)
    {
        for (auto& node2 : nodes)
        {
            for (int i_from = 0; i_from < node1.out.size(); i_from++)
            {
                for (int i_to = 0; i_to < node2.in.size(); i_to++)
                {
                    if (node1.out[i_from] == node2.in[i_to])
                    {
                        node1.nexts.push_back(&node2);
                        node2.prevs[i_to] = &node1;
                    }
                }
            }
        }
    }
    for (auto& n : nodes)
    {
        n.prevs.erase(std::remove(n.prevs.begin(), n.prevs.end(), nullptr), n.prevs.end());
        n.nexts.erase(std::remove(n.nexts.begin(), n.nexts.end(), nullptr), n.nexts.end());
    }
    calPosition(nodes);
}

void onnxloader::nodesToFile(const std::deque<Node>& nodes, const std::string& filename)
{

}

void onnxloader::refreshNodeValues(Node& node)
{
}

