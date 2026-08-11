#include "zed_camera.hpp"

#include <turbojpeg.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace zed {

namespace {

constexpr char kColorSourceName[] = "color";
constexpr char kDepthSourceName[] = "depth";
constexpr char kColorMimeType[] = "image/jpeg";
constexpr char kDepthMimeTypeViamDep[] = "image/vnd.viam.dep";
constexpr char kPcdMimeType[] = "pointcloud/pcd";

struct PointXYZRGB {
  float x;
  float y;
  float z;
  uint32_t rgb;
};
static_assert(std::is_trivially_copyable_v<PointXYZRGB>);
static_assert(std::is_standard_layout_v<PointXYZRGB>);
static_assert(sizeof(PointXYZRGB) == 16);

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
  bool should_process_color = false;
  bool should_process_depth = false;
  if (filter_source_names.empty()) {
    should_process_color = true;
    should_process_depth = true;
  } else {
    for (const auto& name : filter_source_names) {
      if (name == kColorSourceName) {
        should_process_color = true;
      }
      if (name == kDepthSourceName) {
        should_process_depth = true;
      }
    }
  }
  if (!should_process_color && !should_process_depth) {
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

  viam::sdk::Camera::image_collection out;

  if (should_process_color) {
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

    viam::sdk::Camera::raw_image color;
    color.source_name = kColorSourceName;
    color.mime_type = kColorMimeType;
    color.bytes = encode_bgra_as_jpeg(bgra, width, height, stride, /*quality=*/85);
    out.images.push_back(std::move(color));
  }

  if (should_process_depth) {
    sl::Mat depth_mat;
    const auto depth_err = camera_.retrieveMeasure(depth_mat, sl::MEASURE::DEPTH, sl::MEM::CPU);
    if (depth_err != sl::ERROR_CODE::SUCCESS) {
      const std::string msg =
          "sl::Camera::retrieveMeasure failed: " + std::string(sl::toString(depth_err).c_str());
      VIAM_RESOURCE_LOG(error) << msg;
      throw std::runtime_error(msg);
    }
    out.images.push_back(convert_zed_depth_to_viam(depth_mat));
  }

  const auto ts_ns = camera_.getTimestamp(sl::TIME_REFERENCE::IMAGE).getNanoseconds();
  out.metadata.captured_at =
      std::chrono::time_point<std::chrono::system_clock, std::chrono::nanoseconds>(
          std::chrono::nanoseconds(ts_ns));
  return out;
}

viam::sdk::Camera::raw_image Zed2i::convert_zed_depth_to_viam(const sl::Mat& depth_mat) {
  const int width = static_cast<int>(depth_mat.getWidth());
  const int height = static_cast<int>(depth_mat.getHeight());
  const int pixels_per_row = static_cast<int>(depth_mat.getStepBytes(sl::MEM::CPU) / sizeof(float));
  const auto* depth_data = depth_mat.getPtr<float>(sl::MEM::CPU);

  viam::sdk::Camera::depth_map dm = viam::sdk::Camera::depth_map::from_shape(
      {static_cast<size_t>(height), static_cast<size_t>(width)});
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const float v = depth_data[y * pixels_per_row + x];
      uint16_t d = 0;
      if (std::isfinite(v) && v > 0.0f && v < 65535.0f) {
        d = static_cast<uint16_t>(std::lround(v));
      }
      dm(y, x) = d;
    }
  }

  viam::sdk::Camera::raw_image depth;
  depth.source_name = kDepthSourceName;
  depth.mime_type = kDepthMimeTypeViamDep;
  depth.bytes = viam::sdk::Camera::encode_depth_map(dm);
  return depth;
}

viam::sdk::Camera::point_cloud Zed2i::get_point_cloud(std::string /*mime_type*/,
                                                      const viam::sdk::ProtoStruct& /*extra*/) {
  std::lock_guard<std::mutex> lock(grab_mu_);

  sl::RuntimeParameters runtime;
  const auto grab_err = camera_.grab(runtime);
  if (grab_err != sl::ERROR_CODE::SUCCESS) {
    const std::string msg =
        "sl::Camera::grab failed: " + std::string(sl::toString(grab_err).c_str());
    VIAM_RESOURCE_LOG(error) << msg;
    throw std::runtime_error(msg);
  }

  sl::Mat cloud;
  const auto cloud_err = camera_.retrieveMeasure(cloud, sl::MEASURE::XYZRGBA, sl::MEM::CPU);
  if (cloud_err != sl::ERROR_CODE::SUCCESS) {
    const std::string msg =
        "sl::Camera::retrieveMeasure failed: " + std::string(sl::toString(cloud_err).c_str());
    VIAM_RESOURCE_LOG(error) << msg;
    throw std::runtime_error(msg);
  }

  return encode_zed_cloud_to_pcd(cloud);
}

viam::sdk::Camera::point_cloud Zed2i::encode_zed_cloud_to_pcd(const sl::Mat& cloud) {
  const size_t width = cloud.getWidth();
  const size_t height = cloud.getHeight();
  const size_t pixels_per_row = cloud.getStepBytes(sl::MEM::CPU) / sizeof(sl::float4);
  const auto* data = cloud.getPtr<sl::float4>(sl::MEM::CPU);

  std::vector<PointXYZRGB> points;
  points.reserve(width * height);
  for (size_t y = 0; y < height; ++y) {
    for (size_t x = 0; x < width; ++x) {
      const auto& p = data[y * pixels_per_row + x];
      if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z)) {
        continue;
      }
      uint32_t bgra_packed;
      std::memcpy(&bgra_packed, &p.w, sizeof(uint32_t));
      PointXYZRGB pt;
      pt.x = p.x / 1000.0f;
      pt.y = p.y / 1000.0f;
      pt.z = p.z / 1000.0f;
      pt.rgb = bgra_packed & 0x00FFFFFFu;
      points.push_back(pt);
    }
  }

  std::stringstream header;
  header << "VERSION .7\n"
         << "FIELDS x y z rgb\n"
         << "SIZE 4 4 4 4\n"
         << "TYPE F F F U\n"
         << "COUNT 1 1 1 1\n"
         << "WIDTH " << points.size() << "\n"
         << "HEIGHT 1\n"
         << "VIEWPOINT 0 0 0 1 0 0 0\n"
         << "POINTS " << points.size() << "\n"
         << "DATA binary\n";
  const std::string header_str = header.str();

  std::vector<unsigned char> bytes;
  bytes.reserve(header_str.size() + points.size() * sizeof(PointXYZRGB));
  bytes.insert(bytes.end(), header_str.begin(), header_str.end());
  const auto* raw = reinterpret_cast<const unsigned char*>(points.data());
  bytes.insert(bytes.end(), raw, raw + points.size() * sizeof(PointXYZRGB));

  return viam::sdk::Camera::point_cloud{kPcdMimeType, std::move(bytes)};
}

viam::sdk::Camera::properties Zed2i::get_properties() {
  std::lock_guard<std::mutex> lock(grab_mu_);

  const auto info = camera_.getCameraInformation();
  const auto& cal = info.camera_configuration.calibration_parameters.left_cam;

  viam::sdk::Camera::properties out;
  out.supports_pcd = true;
  out.intrinsic_parameters.width_px = static_cast<int>(cal.image_size.width);
  out.intrinsic_parameters.height_px = static_cast<int>(cal.image_size.height);
  out.intrinsic_parameters.focal_x_px = cal.fx;
  out.intrinsic_parameters.focal_y_px = cal.fy;
  out.intrinsic_parameters.center_x_px = cal.cx;
  out.intrinsic_parameters.center_y_px = cal.cy;

  // ZED reports rectified intrinsics (matches retrieveImage's rectified output),
  // so disto is zero in practice. Reorder ZED [k1,k2,p1,p2,k3] -> RDK [k1,k2,k3,p1,p2]
  // in case raw calibration is ever plumbed through here.
  out.distortion_parameters.model = "brown_conrady";
  out.distortion_parameters.parameters = {
      cal.disto[0], cal.disto[1], cal.disto[4], cal.disto[2], cal.disto[3],
  };

  VIAM_RESOURCE_LOG(info) << "properties: " << cal.image_size.width << "x" << cal.image_size.height
                          << " fx=" << cal.fx << " fy=" << cal.fy;
  return out;
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
