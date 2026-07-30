#pragma once

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

  viam::sdk::Camera::image_collection get_images(std::vector<std::string> filter_source_names,
                                                 const viam::sdk::ProtoStruct& extra) override;
  viam::sdk::Camera::point_cloud get_point_cloud(std::string mime_type,
                                                 const viam::sdk::ProtoStruct& extra) override;
  viam::sdk::Camera::properties get_properties() override;
  viam::sdk::ProtoStruct get_status() override;
  std::vector<viam::sdk::GeometryConfig> get_geometries(const viam::sdk::ProtoStruct& extra) override;
  viam::sdk::ProtoStruct do_command(const viam::sdk::ProtoStruct& command) override;
};

}  // namespace zed
