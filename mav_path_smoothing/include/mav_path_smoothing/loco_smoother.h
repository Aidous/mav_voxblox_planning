#ifndef MAV_PATH_SMOOTHING_LOCO_SMOOTHER_H_
#define MAV_PATH_SMOOTHING_LOCO_SMOOTHER_H_

#include <loco_planner/loco.h>

#include "mav_path_smoothing/polynomial_smoother.h"

namespace mav_planning {

class LocoSmoother : public PolynomialSmoother {
 public:
  LocoSmoother();
  virtual ~LocoSmoother() {}

  virtual void setParametersFromRos(const ros::NodeHandle& nh);

  virtual bool getTrajectoryBetweenWaypoints(
      const mav_msgs::EigenTrajectoryPoint::Vector& waypoints,
      mav_trajectory_generation::Trajectory* trajectory) const;

  // Special case for num_waypoints = 2 (splits trajectory into num_segments).
  virtual bool getTrajectoryBetweenTwoPoints(
      const mav_msgs::EigenTrajectoryPoint& start,
      const mav_msgs::EigenTrajectoryPoint& goal,
      mav_trajectory_generation::Trajectory* trajectory) const;

  virtual bool getPathBetweenTwoPoints(
      const mav_msgs::EigenTrajectoryPoint& start,
      const mav_msgs::EigenTrajectoryPoint& goal,
      mav_msgs::EigenTrajectoryPoint::Vector* path) const;

  // Parameters...
  bool getResampleTrajectory() const { return resample_trajectory_; }
  void setResampleTrajectory(bool resample_trajectory) {
    resample_trajectory_ = resample_trajectory;
  }
  bool getResampleVisibility() const { return resample_visibility_; }
  void setResampleVisibility(bool resample_visibility) {
    resample_visibility_ = resample_visibility;
  }
  int getNumSegments() const { return num_segments_; }
  void setNumSegments(int num_segments) { num_segments_ = num_segments; }
  // Controls whether waypoints are added as soft costs in the LOCO problem.
  bool getAddWaypoints() const { return add_waypoints_; }
  void setAddWaypoints(bool add_waypoints) { add_waypoints_ = add_waypoints; }

 protected:
void resampleWaypointsFromVisibilityGraph(
    const mav_msgs::EigenTrajectoryPoint::Vector& waypoints,
    mav_msgs::EigenTrajectoryPoint::Vector* waypoints_out) const;

  bool resample_trajectory_;
  bool resample_visibility_;
  int num_segments_;
  bool add_waypoints_;
};

}  // namespace mav_planning

#endif  // MAV_PATH_SMOOTHING_LOCO_SMOOTHER_H_
