#include "zed_camera.hpp"

#include <stdexcept>

namespace zed {

const viam::sdk::Model Zed2i::model{"viam", "zed", "zed2i"};

std::vector<std::string> Zed2i::validate(viam::sdk::ResourceConfig /*cfg*/) { return {}; }

Zed2i::Zed2i(viam::sdk::Dependencies /*deps*/, viam::sdk::ResourceConfig cfg)
    : viam::sdk::Camera(cfg.name()) {}

viam::sdk::Camera::image_collection Zed2i::get_images(
    std::vector<std::string> /*filter_source_names*/, const viam::sdk::ProtoStruct& /*extra*/) {
  throw std::runtime_error("get_images not implemented");
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
