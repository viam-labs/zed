#pragma once

#include <mutex>
#include <sl/Camera.hpp>
#include <string>
#include <vector>
#include <viam/sdk/components/camera.hpp>
#include <viam/sdk/config/resource.hpp>

namespace zed {

class Zed2i final : public viam::sdk::Camera {
 public:
  static const viam::sdk::Model model;
  static std::vector<std::string> validate(viam::sdk::ResourceConfig cfg);

  Zed2i(viam::sdk::Dependencies deps, viam::sdk::ResourceConfig cfg);
  ~Zed2i() override;

  viam::sdk::Camera::image_collection get_images(std::vector<std::string> filter_source_names,
                                                 const viam::sdk::ProtoStruct& extra) override;
  viam::sdk::Camera::point_cloud get_point_cloud(std::string mime_type,
                                                 const viam::sdk::ProtoStruct& extra) override;
  viam::sdk::Camera::properties get_properties() override;
  viam::sdk::ProtoStruct get_status() override;
  std::vector<viam::sdk::GeometryConfig> get_geometries(
      const viam::sdk::ProtoStruct& extra) override;
  viam::sdk::ProtoStruct do_command(const viam::sdk::ProtoStruct& command) override;

 private:
  struct Config {
    std::string serial_number;
  };

  static Config parse_config(const viam::sdk::ResourceConfig& cfg);
  static viam::sdk::Camera::raw_image convert_zed_depth_to_viam(const sl::Mat& depth_mat);
  static viam::sdk::Camera::point_cloud encode_zed_cloud_to_pcd(const sl::Mat& cloud);

  sl::Camera camera_;
  std::mutex grab_mu_;
};

}  // namespace zed
