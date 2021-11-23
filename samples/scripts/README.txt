=====================================================
Scripts to build and run OpenVINO samples
=====================================================

These scripts simplify process of build samples, download and convert models and run samples to perform inference. They can used to quick validation of OpenVINO installation and proper environment initialization.

Setting Up
================
If you are behind a proxy, set the following environment variables in the console session:

On Linux* and Mac OS:
export http_proxy=http://<proxyHost>:<proxyPort>
export https_proxy=https://<proxyHost>:<proxyPort>

On Windows* OS:
set http_proxy=http://<proxyHost>:<proxyPort>
set https_proxy=https://<proxyHost>:<proxyPort>

Running Samples
=============

The "demo" folder contains two scripts:

1. Classification sample using public SqueezeNet topology (run_sample_squeezenet.sh|bat)

2. Benchmark sample using public SqueezeNet topology (run_sample_benchmark_app.sh|bat) 

To run the samples, invoke run_sample_squeezenet.sh or run_sample_benchmark_app.sh (*.bat on Windows) scripts from the console without parameters, for example:

./run_sample_squeezenet.sh

The script allows to specify the target device to infer on using -d <CPU|GPU|MYRIAD> option.

Classification Sample Using SqueezeNet
====================================

The sample illustrates the general workflow of using the Intel(R) Deep Learning Deployment Toolkit and performs the following:

  - Downloads a public SqueezeNet model using the Model Downloader (extras\open_model_zoo\tools\downloader\downloader.py)
  - Installs all prerequisites required for running the Model Optimizer using the scripts from the "tools\model_optimizer\install_prerequisites" folder
  - Converts SqueezeNet to an IR using the Model Optimizer (tools\model_optimizer\mo.py) via the Model Converter (extras\open_model_zoo\tools\downloader\converter.py)
  - Builds the Inference Engine classification_sample (samples\cpp\classification_sample)
  - Runs the sample with the car.png picture located in the demo folder

The sample application prints top-10 inference results for the picture.

For more information about the Inference Engine classification sample, refer to the documentation available in the sample folder.

Benchmark Sample Using SqueezeNet
===============================

The sample illustrates how to use the Benchmark Application to estimate deep learning inference performance on supported devices.

The sample script does the following:

  - Downloads a public SqueezeNet model using the Model Downloader (extras\open_model_zoo\tools\downloader\downloader.py)
  - Installs all prerequisites required for running the Model Optimizer using the scripts from the "tools\model_optimizer\install_prerequisites" folder
  - Converts SqueezeNet to an IR using the Model Optimizer (tools\model_optimizer\mo.py) via the Model Converter (extras\open_model_zoo\tools\downloader\converter.py)
  - Builds the Inference Engine benchmark tool (samples\benchmark_app)
  - Runs the tool with the car.png picture located in the demo folder

The benchmark app prints performance counters, resulting latency, and throughput values.

For more information about the Inference Engine benchmark app, refer to the documentation available in the sample folder.
