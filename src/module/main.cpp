#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <viam/sdk/common/instance.hpp>
#include <viam/sdk/module/service.hpp>

namespace vsdk = ::viam::sdk;

int serve(int argc, char** argv) try {
  vsdk::Instance inst;

  VIAM_SDK_LOG(info) << "[serve] Starting ZED module";

  std::vector<std::shared_ptr<vsdk::ModelRegistration>> registrations;

  auto module_service = std::make_shared<vsdk::ModuleService>(argc, argv, std::move(registrations));
  module_service->serve();

  return EXIT_SUCCESS;
} catch (const std::exception& ex) {
  std::cerr << "ERROR: std::exception from `serve`: " << ex.what() << std::endl;
  return EXIT_FAILURE;
} catch (...) {
  std::cerr << "ERROR: unknown exception from `serve`" << std::endl;
  return EXIT_FAILURE;
}

int main(int argc, char* argv[]) {
  const std::string usage = "usage: viam-camera-zed /path/to/unix/socket";
  if (argc < 2) {
    std::cout << "ERROR: insufficient arguments\n" << usage << "\n";
    return EXIT_FAILURE;
  }
  return serve(argc, argv);
}
