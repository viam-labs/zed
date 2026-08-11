#include "zed_camera.hpp"

#include <turbojpeg.h>

#include <algorithm>
#include <chrono>
#include <cstring>
#include <stdexcept>
#include <string>

namespace zed {

namespace {

std::vector<unsigned char> encode_bgra_as_jpeg(const unsigned char* bgra, int width, int height,
                                               int stride, int quality) {
  tjhandle tj = tjInitCompress();
  if (!tj) {
    throw std::runtime_error("tjInitCompress failed");
  }
  unsigned char* out = nullptr;
  unsigned long out_size = 0;
  const int subsamp = TJSAMP_420;
  const int rc = tjCompress2(tj, bgra, width, stride, height, TJPF_BGRA, &out, &out_size, subsamp,
                             quality, TJFLAG_FASTDCT);
  if (rc != 0) {
    std::string err = tjGetErrorStr2(tj);
    tjDestroy(tj);
    throw std::runtime_error("tjCompress2 failed: " + err);
  }
  std::vector<unsigned char> result(out, out + out_size);
  tjFree(out);
  tjDestroy(tj);
  return result;
}

}  // namespace

const viam::sdk::Model Zed2i::model{"viam", "zed", "zed2i"};

std::vector<std::string> Zed2i::validate(viam::sdk::ResourceConfig /*cfg*/) { return {}; }

Zed2i::Config Zed2i::parse_config(const viam::sdk::ResourceConfig& cfg) {
  Config out;
  const auto& attrs = cfg.attributes();
  auto it = attrs.find("serial_number");
  if (it != attrs.end()) {
    if (const auto* s = it->second.get<std::string>()) {
      out.serial_number = *s;
    }
  }
  return out;
}

Zed2i::Zed2i(viam::sdk::Dependencies /*deps*/, viam::sdk::ResourceConfig cfg)
    : viam::sdk::Camera(cfg.name()) {
  const Config config = parse_config(cfg);

  sl::InitParameters params;
  params.camera_resolution = sl::RESOLUTION::HD720;
  params.camera_fps = 30;
  params.depth_mode = sl::DEPTH_MODE::PERFORMANCE;

  if (!config.serial_number.empty()) {
    unsigned int sn = 0;
    try {
      sn = static_cast<unsigned int>(std::stoul(config.serial_number));
    } catch (const std::exception&) {
      throw std::invalid_argument("serial_number must be a positive integer: " +
                                  config.serial_number);
    }
    params.input.setFromSerialNumber(sn);
    VIAM_RESOURCE_LOG(info) << "binding to camera S/N " << sn;
  }

  const auto err = camera_.open(params);
  if (err != sl::ERROR_CODE::SUCCESS) {
    const std::string msg = "sl::Camera::open failed: " + std::string(sl::toString(err).c_str());
    VIAM_RESOURCE_LOG(error) << msg;
    throw std::runtime_error(msg);
  }
  VIAM_RESOURCE_LOG(info) << "opened S/N " << camera_.getCameraInformation().serial_number
                          << " at HD720@30 PERFORMANCE";
}

Zed2i::~Zed2i() {
  if (camera_.isOpened()) {
    camera_.close();
  }
}

viam::sdk::Camera::image_collection Zed2i::get_images(std::vector<std::string> filter_source_names,
                                                      const viam::sdk::ProtoStruct& /*extra*/) {
  const bool want_color = filter_source_names.empty() ||
                          std::find(filter_source_names.begin(), filter_source_names.end(),
                                    std::string("color")) != filter_source_names.end();
  if (!want_color) {
    return {};
  }

  std::lock_guard<std::mutex> lock(grab_mu_);

  sl::RuntimeParameters runtime;
  const auto grab_err = camera_.grab(runtime);
  if (grab_err != sl::ERROR_CODE::SUCCESS) {
    const std::string msg =
        "sl::Camera::grab failed: " + std::string(sl::toString(grab_err).c_str());
    VIAM_RESOURCE_LOG(error) << msg;
    throw std::runtime_error(msg);
  }

  sl::Mat left;
  const auto retrieve_err = camera_.retrieveImage(left, sl::VIEW::LEFT, sl::MEM::CPU);
  if (retrieve_err != sl::ERROR_CODE::SUCCESS) {
    const std::string msg =
        "sl::Camera::retrieveImage failed: " + std::string(sl::toString(retrieve_err).c_str());
    VIAM_RESOURCE_LOG(error) << msg;
    throw std::runtime_error(msg);
  }

  const int width = static_cast<int>(left.getWidth());
  const int height = static_cast<int>(left.getHeight());
  const int stride = static_cast<int>(left.getStepBytes(sl::MEM::CPU));
  auto* bgra = left.getPtr<sl::uchar1>(sl::MEM::CPU);

  auto jpeg = encode_bgra_as_jpeg(bgra, width, height, stride, /*quality=*/85);

  viam::sdk::Camera::raw_image color;
  color.source_name = "color";
  color.mime_type = "image/jpeg";
  color.bytes = std::move(jpeg);

  const auto ts_ns = camera_.getTimestamp(sl::TIME_REFERENCE::IMAGE).getNanoseconds();
  viam::sdk::Camera::image_collection out;
  out.images.push_back(std::move(color));
  out.metadata.captured_at =
      std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>(
          std::chrono::nanoseconds(ts_ns));
  return out;
}

viam::sdk::Camera::point_cloud Zed2i::get_point_cloud(std::string /*mime_type*/,
                                                      const viam::sdk::ProtoStruct& /*extra*/) {
  throw std::runtime_error("get_point_cloud not implemented");
}

viam::sdk::Camera::properties Zed2i::get_properties() {
  throw std::runtime_error("get_properties not implemented");
}

viam::sdk::ProtoStruct Zed2i::get_status() {
  throw std::runtime_error("get_status not implemented");
}

std::vector<viam::sdk::GeometryConfig> Zed2i::get_geometries(
    const viam::sdk::ProtoStruct& /*extra*/) {
  return {};
}

viam::sdk::ProtoStruct Zed2i::do_command(const viam::sdk::ProtoStruct& /*command*/) {
  throw std::runtime_error("do_command not implemented");
}

}  // namespace zed
