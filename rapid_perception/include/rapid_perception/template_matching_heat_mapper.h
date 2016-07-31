#ifndef _RAPID_PERCEPTION_TEMPLATE_MATCHING_HEAT_MAPPER_H_
#define _RAPID_PERCEPTION_TEMPLATE_MATCHING_HEAT_MAPPER_H_

#include <string>

#include "Eigen/Core"
#include "pcl/PointIndices.h"
#include "pcl/point_cloud.h"
#include "pcl/point_types.h"

#include "rapid_perception/pose_estimation_heat_mapper.h"

namespace rapid {
namespace perception {
class TemplateMatchingHeatMapper : public PoseEstimationHeatMapper {
 public:
  TemplateMatchingHeatMapper();
  void Compute(pcl::PointIndicesPtr indices, Eigen::VectorXd* importances);
  void set_scene(pcl::PointCloud<pcl::PointXYZRGB>::Ptr scene);
  void set_object(pcl::PointCloud<pcl::PointXYZRGB>::Ptr object);
  void set_sample_ratio(double val);
  void set_max_samples(int val);

 private:
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr scene_;
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr object_;
  Eigen::Vector3d object_center_;
  double sample_ratio_;
  int max_samples_;
};
}  // namespace perception
}  // namespace rapid

#endif  // _RAPID_PERCEPTION_TEMPLATE_MATCHING_HEAT_MAPPER_H_
