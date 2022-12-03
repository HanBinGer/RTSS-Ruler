#pragma once
#include "stdafx.h"

void cropImg() {
	int x_left = 0, x_right = 0, y_top = 0, y_bottom = 0;
	cv::Mat img = cv::imread("temp/screen.png");
	if (img.size().height == 1080 && img.size().width == 1920) {
		x_left = 825;
		x_right = 1648;
		y_top = 193;
		y_bottom = 1016;
		/*x_left = 866;
		x_right = 1665;
		y_top = 212;
		y_bottom = 1011;*/
	}
	else if (img.size().height == 1440 && img.size().width == 2560) {
		x_left = 1094;
		x_right = 2203;
		y_top = 251;
		y_bottom = 1360;
	}
	cv::Mat imgCrop = img(cv::Range(y_top, y_bottom), cv::Range(x_left, x_right));
	imwrite("temp/map.png", imgCrop);
}

std::vector<std::string> load_class_list()
{
    std::vector<std::string> class_list;
    std::ifstream ifs("model/names.txt");
    std::string line;
    while (getline(ifs, line))
    {
        class_list.push_back(line);
    }
    return class_list;
}

void load_net(cv::dnn::Net& net, bool is_cuda)
{
    auto result = cv::dnn::readNet("model/best.onnx");
    if (is_cuda)
    {
        std::cout << "Attempty to use CUDA\n";
        result.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        result.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA_FP16);
    }
    else
    {
        std::cout << "Running on CPU\n";
        result.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        result.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
    }
    net = result;
}

const std::vector<cv::Scalar> colors = { cv::Scalar(255, 255, 0), cv::Scalar(0, 255, 0), cv::Scalar(0, 255, 255), cv::Scalar(255, 0, 0) };

const float INPUT_WIDTH = 640.0;
const float INPUT_HEIGHT = 640.0;
const float SCORE_THRESHOLD = 0.2;
const float NMS_THRESHOLD = 0.4;
const float CONFIDENCE_THRESHOLD = 0.4;
const int classCount = 1;

struct Detection
{
    int class_id;
    float confidence;
    cv::Rect box;
    bool operator<(const Detection& X) const {
        if (confidence < X.confidence) return true;
        return false;
    }
};

cv::Mat format_yolov5(const cv::Mat& source) {
    int col = source.cols;
    int row = source.rows;
    int _max = MAX(col, row);
    cv::Mat result = cv::Mat::zeros(_max, _max, CV_8UC3);
    source.copyTo(result(cv::Rect(0, 0, col, row)));
    return result;
}

void detect(cv::Mat& image, cv::dnn::Net& net, std::vector<Detection>& output, const std::vector<std::string>& className) {
    cv::Mat blob;

    auto input_image = format_yolov5(image);

    cv::dnn::blobFromImage(input_image, blob, 1. / 255., cv::Size(INPUT_WIDTH, INPUT_HEIGHT), cv::Scalar(), true, false);
    net.setInput(blob);
    std::vector<cv::Mat> outputs;
    net.forward(outputs, net.getUnconnectedOutLayersNames());

    float x_factor = input_image.cols / INPUT_WIDTH;
    float y_factor = input_image.rows / INPUT_HEIGHT;

    float* data = (float*)outputs[0].data;

    const int dimensions = 85;
    const int rows = 25200;

    std::vector<int> class_ids;
    std::vector<float> confidences;
    std::vector<cv::Rect> boxes;

    for (int i = 0; i < rows; ++i) {

        float confidence = data[4];
        if (confidence >= CONFIDENCE_THRESHOLD) {

            float* classes_scores = data + 5;
            cv::Mat scores(1, className.size(), CV_32FC1, classes_scores);
            cv::Point class_id;
            double max_class_score;
            minMaxLoc(scores, 0, &max_class_score, 0, &class_id);
            if (max_class_score > SCORE_THRESHOLD) {

                confidences.push_back(confidence);

                class_ids.push_back(class_id.x);

                float x = data[0];
                float y = data[1];
                float w = data[2];
                float h = data[3];
                int left = int((x - 0.5 * w) * x_factor);
                int top = int((y - 0.5 * h) * y_factor);
                int width = int(w * x_factor);
                int height = int(h * y_factor);
                boxes.push_back(cv::Rect(left, top, width, height));
            }

        }

        data += (5 + className.size());

    }

    std::vector<int> nms_result;
    cv::dnn::NMSBoxes(boxes, confidences, SCORE_THRESHOLD, NMS_THRESHOLD, nms_result);
    for (int i = 0; i < nms_result.size(); i++) {
        int idx = nms_result[i];
        Detection result;
        result.class_id = class_ids[idx];
        result.confidence = confidences[idx];
        result.box = boxes[idx];
        output.push_back(result);
    }
}

void initNet(cv::dnn::Net& net) {
    bool is_cuda = false;   // argc > 1 && strcmp(argv[1], "cuda") == 0;
    load_net(net, is_cuda);
}

void yolo(cv::dnn::Net &net, std::vector<Detection> &myMark, std::vector<Detection> &yellMark)
{

    std::vector<std::string> class_list = load_class_list();

    cv::Mat frame = cv::imread("temp/map.png");

    std::vector<Detection> output;
    detect(frame, net, output, class_list);

    int detections = output.size();

    for (int i = 0; i < detections; ++i)
    {
        auto detection = output[i];
        auto box = detection.box;
        auto classId = detection.class_id;
        const auto color = colors[classId % colors.size()];
        if (classId == 3)
            yellMark.push_back(detection);
        else if (classId == 0)
            myMark.push_back(detection);
        cv::rectangle(frame, box, color, 1);

        //cv::rectangle(frame, cv::Point(box.x, box.y - 20), cv::Point(box.x + box.width, box.y), color, cv::FILLED);
        cv::putText(frame, (class_list[classId] + "  " + std::to_string(detection.confidence)).c_str(), cv::Point(box.x, box.y - 5), cv::FONT_HERSHEY_COMPLEX, 0.4, cv::Scalar(0, 0, 0), 1.4);
    }
    cv::imwrite("temp/recogn.png", frame);
    std::sort(yellMark.rbegin(), yellMark.rend());
    std::sort(myMark.rbegin(), myMark.rend());
}