#include <stdio.h>
#include <functional>
#include <thread>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QTimer>
#include <QVBoxLayout>

#include <geometry_msgs/Twist.h>
#include <mav_planning_msgs/PlannerService.h>
#include <rviz/visualization_manager.h>
#include <std_srvs/Empty.h>

#include "mav_planning_rviz/edit_button.h"
#include "mav_planning_rviz/planning_panel.h"
#include "mav_planning_rviz/pose_widget.h"

namespace mav_planning_rviz {

PlanningPanel::PlanningPanel(QWidget* parent)
    : rviz::Panel(parent), nh_(ros::NodeHandle()), interactive_markers_(nh_) {
  createLayout();
}

void PlanningPanel::onInitialize() {
  interactive_markers_.initialize();
  interactive_markers_.setPoseUpdatedCallback(
      std::bind(&PlanningPanel::updateInteractiveMarkerPose, this,
                std::placeholders::_1));

  interactive_markers_.setFrameId(vis_manager_->getFixedFrame().toStdString());
  // Initialize all the markers.
  for (const auto& kv : pose_widget_map_) {
    mav_msgs::EigenTrajectoryPoint pose;
    kv.second->getPose(&pose);
    interactive_markers_.enableMarker(kv.first, pose);
  }
}

void PlanningPanel::createLayout() {
  QHBoxLayout* topic_layout = new QHBoxLayout;
  // Input the namespace.
  topic_layout->addWidget(new QLabel("Namespace:"));
  namespace_editor_ = new QLineEdit;
  topic_layout->addWidget(namespace_editor_);
  topic_layout->addWidget(new QLabel("Planner name:"));
  planner_name_editor_ = new QLineEdit;
  topic_layout->addWidget(planner_name_editor_);

  // Start and goal poses.
  QGridLayout* start_goal_layout = new QGridLayout;

  // Minimums...
  start_goal_layout->setColumnMinimumWidth(0, 50);
  start_goal_layout->setColumnMinimumWidth(1, 245);
  start_goal_layout->setColumnMinimumWidth(2, 80);
  start_goal_layout->setRowMinimumHeight(0, 55);
  start_goal_layout->setRowMinimumHeight(1, 55);
  start_goal_layout->setColumnStretch(0, 1);
  start_goal_layout->setColumnStretch(1, 9);
  start_goal_layout->setColumnStretch(2, 3);

  start_pose_widget_ = new PoseWidget("start");
  goal_pose_widget_ = new PoseWidget("goal");
  EditButton* start_edit_button = new EditButton("start");
  EditButton* goal_edit_button = new EditButton("goal");
  registerPoseWidget(start_pose_widget_);
  registerPoseWidget(goal_pose_widget_);
  registerEditButton(start_edit_button);
  registerEditButton(goal_edit_button);

  start_goal_layout->addWidget(new QLabel("Start:"), 0, 0, Qt::AlignTop);
  start_goal_layout->addWidget(start_pose_widget_, 0, 1);
  start_goal_layout->addWidget(start_edit_button, 0, 2);
  start_goal_layout->addWidget(new QLabel("Goal:"), 1, 0, Qt::AlignTop);
  start_goal_layout->addWidget(goal_pose_widget_, 1, 1);
  start_goal_layout->addWidget(goal_edit_button, 1, 2);

  // Planner services and publications.
  QHBoxLayout* service_layout = new QHBoxLayout;
  planner_service_button_ = new QPushButton("Planner Service");
  publish_path_button_ = new QPushButton("Publish Path");
  service_layout->addWidget(planner_service_button_);
  service_layout->addWidget(publish_path_button_);

  // First the names, then the start/goal, then service buttons.
  QVBoxLayout* layout = new QVBoxLayout;
  layout->addLayout(topic_layout);
  layout->addLayout(start_goal_layout);
  layout->addLayout(service_layout);
  setLayout(layout);

  // Hook up connections.
  connect(namespace_editor_, SIGNAL(editingFinished()), this,
          SLOT(updateNamespace()));
  connect(planner_name_editor_, SIGNAL(editingFinished()), this,
          SLOT(updatePlannerName()));
  connect(planner_service_button_, SIGNAL(released()), this,
          SLOT(callPlannerService()));
  connect(publish_path_button_, SIGNAL(released()), this,
          SLOT(callPublishPath()));
}

void PlanningPanel::updateNamespace() {
  setNamespace(namespace_editor_->text());
}

// Set the topic name we are publishing to.
void PlanningPanel::setNamespace(const QString& new_namespace) {
  // Only take action if the name has changed.
  if (new_namespace != namespace_) {
    namespace_ = new_namespace;
    Q_EMIT configChanged();
  }
}

void PlanningPanel::updatePlannerName() {
  setPlannerName(planner_name_editor_->text());
}

// Set the topic name we are publishing to.
void PlanningPanel::setPlannerName(const QString& new_planner_name) {
  // Only take action if the name has changed.
  if (new_planner_name != planner_name_) {
    planner_name_ = new_planner_name;
    Q_EMIT configChanged();
  }
}

void PlanningPanel::startEditing(const std::string& id) {
  // Make sure nothing else is being edited.
  if (!currently_editing_.empty()) {
    auto search = edit_button_map_.find(currently_editing_);
    if (search != edit_button_map_.end()) {
      search->second->finishEditing();
    }
  }
  currently_editing_ = id;
  // Get the current pose:
  auto search = pose_widget_map_.find(currently_editing_);
  if (search == pose_widget_map_.end()) {
    return;
  }
  // Update fixed frame (may have changed since last time):
  interactive_markers_.setFrameId(vis_manager_->getFixedFrame().toStdString());
  mav_msgs::EigenTrajectoryPoint pose;
  search->second->getPose(&pose);
  interactive_markers_.enableSetPoseMarker(pose);
  interactive_markers_.disableMarker(id);
}

void PlanningPanel::finishEditing(const std::string& id) {
  if (currently_editing_ == id) {
    currently_editing_.clear();
    interactive_markers_.disableSetPoseMarker();
  }
  auto search = pose_widget_map_.find(id);
  if (search == pose_widget_map_.end()) {
    return;
  }
  ros::spinOnce();
  mav_msgs::EigenTrajectoryPoint pose;
  search->second->getPose(&pose);
  interactive_markers_.enableMarker(id, pose);
}

void PlanningPanel::registerPoseWidget(PoseWidget* widget) {
  pose_widget_map_[widget->id()] = widget;
  connect(widget, SIGNAL(poseUpdated(const std::string&,
                                     mav_msgs::EigenTrajectoryPoint&)),
          this, SLOT(widgetPoseUpdated(const std::string&,
                                       mav_msgs::EigenTrajectoryPoint&)));
}

void PlanningPanel::registerEditButton(EditButton* button) {
  edit_button_map_[button->id()] = button;
  connect(button, SIGNAL(startedEditing(const std::string&)), this,
          SLOT(startEditing(const std::string&)));
  connect(button, SIGNAL(finishedEditing(const std::string&)), this,
          SLOT(finishEditing(const std::string&)));
}

// Save all configuration data from this panel to the given
// Config object.  It is important here that you call save()
// on the parent class so the class id and panel name get saved.
void PlanningPanel::save(rviz::Config config) const {
  rviz::Panel::save(config);
  config.mapSetValue("namespace", namespace_);
  config.mapSetValue("planner_name", planner_name_);
}

// Load all configuration data for this panel from the given Config object.
void PlanningPanel::load(const rviz::Config& config) {
  rviz::Panel::load(config);
  QString topic;
  if (config.mapGetString("namespace", &namespace_)) {
    namespace_editor_->setText(namespace_);
  }
  if (config.mapGetString("planner_name", &planner_name_)) {
    planner_name_editor_->setText(planner_name_);
  }
}

void PlanningPanel::updateInteractiveMarkerPose(
    const mav_msgs::EigenTrajectoryPoint& pose) {
  if (currently_editing_.empty()) {
    return;
  }
  auto search = pose_widget_map_.find(currently_editing_);
  if (search == pose_widget_map_.end()) {
    return;
  }
  search->second->setPose(pose);
}

void PlanningPanel::widgetPoseUpdated(const std::string& id,
                                      mav_msgs::EigenTrajectoryPoint& pose) {
  if (currently_editing_ == id) {
    interactive_markers_.setPose(pose);
  }
  interactive_markers_.updateMarkerPose(id, pose);
}

void PlanningPanel::callPlannerService() {
  std::string service_name =
      namespace_.toStdString() + "/" + planner_name_.toStdString() + "/plan";
  mav_msgs::EigenTrajectoryPoint start_point, goal_point;

  start_pose_widget_->getPose(&start_point);
  goal_pose_widget_->getPose(&goal_point);

  std::thread t([service_name, start_point, goal_point] {
    mav_planning_msgs::PlannerService req;
    mav_msgs::msgPoseStampedFromEigenTrajectoryPoint(start_point,
                                                     &req.request.start_pose);
    mav_msgs::msgPoseStampedFromEigenTrajectoryPoint(goal_point,
                                                     &req.request.goal_pose);

    try {
      ROS_INFO_STREAM("Service name: " << service_name);
      if (!ros::service::call(service_name, req)) {
        ROS_WARN_STREAM("Couldn't call service: " << service_name);
      }
    } catch (const std::exception& e) {
      ROS_ERROR_STREAM("Service Exception: " << e.what());
    }
  });
  t.detach();
}

void PlanningPanel::callPublishPath() {
  std_srvs::Empty req;
  std::string service_name = namespace_.toStdString() + "/" +
                             planner_name_.toStdString() + "/publish_path";
  try {
    if (!ros::service::call(service_name, req)) {
      ROS_WARN_STREAM("Couldn't call service: " << service_name);
    }
  } catch (const std::exception& e) {
    ROS_ERROR_STREAM("Service Exception: " << e.what());
  }
}

}  // end namespace mav_planning_rviz

// Tell pluginlib about this class.  Every class which should be
// loadable by pluginlib::ClassLoader must have these two lines
// compiled in its .cpp file, outside of any namespace scope.
#include <pluginlib/class_list_macros.h>
PLUGINLIB_EXPORT_CLASS(mav_planning_rviz::PlanningPanel, rviz::Panel)
