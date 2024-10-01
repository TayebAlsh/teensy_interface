
#ifndef TEENSY_INTERFACE__TEENSY_INTERFACE_COMPONENT_HPP_
#define TEENSY_INTERFACE__TEENSY_INTERFACE_COMPONENT_HPP_

#include <rclcpp/rclcpp.hpp>
#include <boost/fusion/adapted/struct.hpp>

#include "udp_server.hpp"

#include <atl_msgs/msg/depth.hpp>
#include <atl_msgs/msg/leak.hpp>
#include <atl_msgs/msg/servo_feedback.hpp>
#include <atl_msgs/msg/servos_feedback.hpp>
#include <atl_msgs/msg/servos_input.hpp>

#include <sensor_msgs/msg/imu.hpp>

// #include <tf2_ros/transform_listener.h>
#include <tf2_ros/transform_broadcaster.h>
#include "geometry_msgs/msg/transform_stamped.hpp"
#include "tf2/LinearMath/Quaternion.h"
#include "visualization_msgs/msg/marker.hpp"

#include <optional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace atl
{

// Forward declarations
class UDPServer;

struct TeensyUdpParams
{
  std::string teensy_ip{"192.168.2.3"};
  uint16_t send_port{1560};
  uint16_t receive_port{1561};
  uint32_t receive_buffer_size{1024};

  void check_correctness() const
  {
    if (teensy_ip.empty()) {
      throw std::invalid_argument("teensy_ip parameter must not be empty.");
    }
    if (receive_buffer_size == 0) {
      throw std::invalid_argument("receive_buffer_size parameter must be > 0.");
    }
  }
};

struct TeensyInterfaceParams
{
  std::size_t n_servos{2};
  TeensyUdpParams udp;

  void check_correctness() const
  {
    if (n_servos < 1) {
      throw std::invalid_argument("n_servos parameter must be > 0.");
    }
    udp.check_correctness();
  }
};

// Component declaration
class TeensyInterfaceComponent : public rclcpp::Node
{
public:
  explicit TeensyInterfaceComponent(const rclcpp::NodeOptions &);

private:
  // Subscriptions
  rclcpp::Subscription<atl_msgs::msg::ServosInput>::SharedPtr subServosInput_;
  rclcpp::Subscription<atl_msgs::msg::Depth>::SharedPtr depth_subscription_; // Added

  // Publishers
  rclcpp::Publisher<atl_msgs::msg::Depth>::SharedPtr pubDepth_;
  rclcpp::Publisher<sensor_msgs::msg::Imu>::SharedPtr pubImu_;
  rclcpp::Publisher<atl_msgs::msg::Leak>::SharedPtr pubLeak_;
  rclcpp::Publisher<atl_msgs::msg::ServosFeedback>::SharedPtr pubServosFeedback_;
  rclcpp::Publisher<visualization_msgs::msg::Marker>::SharedPtr marker_publisher_; // Added

  // Callbacks
  void subServosInputCb(atl_msgs::msg::ServosInput::SharedPtr && msg);
  void udpCb(const UDPServer::UDPMsg & msg);
  void depthCallback(const atl_msgs::msg::Depth::SharedPtr msg);

  
  // State Variables
  std::unique_ptr<UDPServer> udp_;
  uint64_t t0_;
  std::size_t iter_ = 0;
  bool sync_ = true;
  TeensyInterfaceParams prm_;

  uint32_t task_ = 0;
  uint32_t ctrlMode_ = 0;
  std::mutex msgMtx_;

  //tf
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadBoat_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadBody_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadActuator1_;
  std::unique_ptr<tf2_ros::TransformBroadcaster> tf_broadActuator2_;

  //
  float servoInput1_;
  float servoInput2_;
  float servoInput3_;
  float servoInput4_;
  float servoInput5_;

  // feedback
  float servoFeedback1_;
  float servoFeedback2_;
  float servoFeedback3_;
  float servoFeedback4_;
  float servoFeedback5_;

};

}  // namespace atl

// cppcheck-suppress unknownMacro
BOOST_FUSION_ADAPT_STRUCT(
  atl::TeensyUdpParams,
  teensy_ip,
  send_port,
  receive_port,
  receive_buffer_size
)

// cppcheck-suppress unknownMacro
BOOST_FUSION_ADAPT_STRUCT(
  atl::TeensyInterfaceParams,
  n_servos,
  udp
)

#endif  // TEENSY_INTERFACE__TEENSY_INTERFACE_COMPONENT_HPP_
