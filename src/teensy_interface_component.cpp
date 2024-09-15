#include "teensy_interface/teensy_interface_component.hpp"

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_components/register_node_macro.hpp>

#include "udp_server.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <exception>
#include <cerrno>
#include <utility>

using std::string;

constexpr static std::size_t nServos = 5;

namespace atl
{

// ///////////////////
// Interface Component
// ///////////////////
TeensyInterfaceComponent::TeensyInterfaceComponent(const rclcpp::NodeOptions & options)
: Node("teensy_interface", options),
  udp_(std::make_unique<atl::UDPServer>(true))
{


  // Create the subscriptions
  rclcpp::SensorDataQoS inputQoS;
  inputQoS.keep_last(1);
  subServosInput_ = create_subscription<atl_msgs::msg::ServosInput>(
    "servos_input", inputQoS,
    [this](atl_msgs::msg::ServosInput::SharedPtr msg) {
      subServosInputCb(std::move(msg));
    });

  // Create the publishers
  pubDepth_ = create_publisher<atl_msgs::msg::Depth>(
    "depth", rclcpp::SystemDefaultsQoS());

  pubImu_ = create_publisher<sensor_msgs::msg::Imu>(
    "imu", rclcpp::SystemDefaultsQoS());

  pubLeak_ = create_publisher<atl_msgs::msg::Leak>(
    "leak", rclcpp::SystemDefaultsQoS());

  pubServosFeedback_ = create_publisher<atl_msgs::msg::ServosFeedback>(
    "servos_feedback", rclcpp::SystemDefaultsQoS());

  // set up UDP communication
  udp_->init(
    prm_.udp.receive_buffer_size,
    prm_.udp.send_port,
    prm_.udp.teensy_ip,
    prm_.udp.receive_port
  );

  udp_->subscribe(
    [this](const UDPServer::UDPMsg & msg) {
      udpCb(msg);
    });

  tf_broadBoat_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
  tf_broadBody_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
  tf_broadActuator1_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);
  tf_broadActuator2_ = std::make_unique<tf2_ros::TransformBroadcaster>(*this);

  RCLCPP_INFO(get_logger(), "Teensy Interface Node started");
}


//////////////////
// UDP Msg Receive
//////////////////
void TeensyInterfaceComponent::udpCb(const UDPServer::UDPMsg & msg)
{
  // Lin_acc(3) + Ang_vel(3) + Quat(4) + depth(1) + temp(1) + leak(1)
  constexpr std::size_t msgLen = (3+3+4+1+1+1) * 4;

  if (msg.data.size() != msgLen) {
    RCLCPP_ERROR(
      get_logger(), "Teensy UDP message configured to be of length %lu, received %lu.",
      msgLen, msg.data.size());
    throw std::runtime_error("Teensy UDP message has incorrect size.");
  }

  std::size_t offset = 0;

  auto read_float = [&msg, &offset]() {
    float value;
    std::memcpy(&value, msg.data.data() + offset, sizeof(float));
    offset += sizeof(float);
    return value;
  };

  // Read and print linear acceleration
  float lin_acc[3];
  for (int i = 0; i < 3; ++i) {
    lin_acc[i] = read_float();
  }
  RCLCPP_INFO(get_logger(), "Linear Acceleration: x=%.2f, y=%.2f, z=%.2f", lin_acc[0], lin_acc[1], lin_acc[2]);

  // Read and print angular velocity
  float ang_vel[3];
  for (int i = 0; i < 3; ++i) {
    ang_vel[i] = read_float();
  }
  RCLCPP_INFO(get_logger(), "Angular Velocity: x=%.2f, y=%.2f, z=%.2f", ang_vel[0], ang_vel[1], ang_vel[2]);

  // Read and print quaternion
  float quat[4];
  for (int i = 0; i < 4; ++i) {
    quat[i] = read_float();
  }
  RCLCPP_INFO(get_logger(), "Quaternion: x=%.2f, y=%.2f, z=%.2f, w=%.2f", quat[0], quat[1], quat[2], quat[3]);

  // Read and print depth
  float depth = read_float();
  RCLCPP_INFO(get_logger(), "Depth: %.2f", depth);

  // Read and print temperature
  float temp = read_float();
  RCLCPP_INFO(get_logger(), "Temperature: %.2f", temp);

  // Read and print leak
  float leak = read_float();
  RCLCPP_INFO(get_logger(), "Leak: %.2f", leak);

  // Read and print additional data
  float additional_data[5];
  for (int i = 0; i < 5; ++i) {
    additional_data[i] = read_float();
  }
  RCLCPP_INFO(get_logger(), "Additional Data: d1=%.2f, d2=%.2f, d3=%.2f, d4=%.2f, d5=%.2f", additional_data[0], additional_data[1], additional_data[2], additional_data[3], additional_data[4]);

  std::size_t oft = 0;
  const auto tNow = now();

  ///////////
  // IMU Data
  sensor_msgs::msg::Imu imuMsg;
  imuMsg.header.stamp = tNow;
  // imuMsg.status - to do in the future.
  imuMsg.linear_acceleration.x = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.linear_acceleration.y = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.linear_acceleration.z = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.angular_velocity.x = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.angular_velocity.y = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.angular_velocity.z = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.orientation.x = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.orientation.y = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.orientation.z = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  imuMsg.orientation.w = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  pubImu_->publish(std::move(imuMsg));

  ////////////////////////
  // Depth and Temperature
  atl_msgs::msg::Depth depthMsg;
  depthMsg.header.stamp = tNow;
  // depthMsg.status = -1;
  depthMsg.depth = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  // depthMsg.temperature = 112.9;
  depthMsg.temperature = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  pubDepth_->publish(std::move(depthMsg));

  //////////////
  // Leak sensor
  atl_msgs::msg::Leak leakMsg;
  leakMsg.header.stamp = tNow;
  leakMsg.leak = (*(reinterpret_cast<const float *>(msg.data.data() + oft)));
  oft += 4;
  pubLeak_->publish(std::move(leakMsg));


  /////////////
  // Transforms
  geometry_msgs::msg::TransformStamped t_boat;
  geometry_msgs::msg::TransformStamped t_body;
  geometry_msgs::msg::TransformStamped t_act_w;
  geometry_msgs::msg::TransformStamped t_act1;
  geometry_msgs::msg::TransformStamped t_act2;
  geometry_msgs::msg::TransformStamped t_act3;

  // --------- boat
  t_boat.header.stamp = tNow;
  t_boat.header.frame_id = "world";
  t_boat.child_frame_id = "boat";
  // Boat Translation
  t_boat.transform.translation.x = 0.0;
  t_boat.transform.translation.y = 0.0;
  t_boat.transform.translation.z = 0.25;
  // Boat actuator Rotation
  tf2::Quaternion q0;
  q0.setRPY(0, 0, 0);
  t_boat.transform.rotation.x = q0.x();
  t_boat.transform.rotation.y = q0.y();
  t_boat.transform.rotation.z = q0.z();
  t_boat.transform.rotation.w = q0.w();
  // Send the transformation
  tf_broadBoat_->sendTransform(t_boat);

  // --------- Paravane
  t_body.header.stamp = tNow;
  t_body.header.frame_id = "boat";
  t_body.child_frame_id = "paravane";
  // Paravane Translation
  t_body.transform.translation.x = -5.0;
  t_body.transform.translation.y = 0.0;
  t_body.transform.translation.z = -depthMsg.depth;
  // Paravane Rotation
  t_body.transform.rotation.x = imuMsg.orientation.x;
  t_body.transform.rotation.y = imuMsg.orientation.y;
  t_body.transform.rotation.z = imuMsg.orientation.z;
  t_body.transform.rotation.w = imuMsg.orientation.w;
  // Send the transformation
  tf_broadBody_->sendTransform(t_body);

// --------- Main Wing actuator
  t_act_w.header.stamp = tNow;
  t_act_w.header.frame_id = "paravane";
  t_act_w.child_frame_id = "main_wing";
  // Main Wing actuator Translation
  t_act_w.transform.translation.x = 0.1;
  t_act_w.transform.translation.y = 0.0;
  t_act_w.transform.translation.z = 0.0;
  // Main Wing actuator Rotation
  tf2::Quaternion qw;
  qw.setRPY(0, servoInput1_, 0);
  t_act_w.transform.rotation.x = qw.x();
  t_act_w.transform.rotation.y = qw.y();
  t_act_w.transform.rotation.z = qw.z();
  t_act_w.transform.rotation.w = qw.w();
  // Send the transformation
  tf_broadBody_->sendTransform(t_act_w);

  // --------- bottom tail actuator
  t_act1.header.stamp = tNow;
  t_act1.header.frame_id = "paravane";
  t_act1.child_frame_id = "actuator1";
  // Actuator Translation
  t_act1.transform.translation.x = -0.2;
  t_act1.transform.translation.y = 0.0;
  t_act1.transform.translation.z = 0.05;
  // Actuator Rotation
  tf2::Quaternion q1;
  q1.setRPY(0, 0, servoFeedback2_);
  t_act1.transform.rotation.x = q1.x();
  t_act1.transform.rotation.y = q1.y();
  t_act1.transform.rotation.z = q1.z();
  t_act1.transform.rotation.w = q1.w();
  // Send the transformation
  tf_broadBody_->sendTransform(t_act1);


  // --------- left tail actuator
  t_act2.header.stamp = tNow;
  t_act2.header.frame_id = "paravane";
  t_act2.child_frame_id = "actuator2";
  // Actuator Translation
  t_act2.transform.translation.x = -0.2;
  t_act2.transform.translation.y = 0.02;
  t_act2.transform.translation.z = -0.035;
  // Actuator Rotation
  tf2::Quaternion q2;
  q2.setRPY(0, 0, servoFeedback3_);
  t_act2.transform.rotation.x = q2.x();
  t_act2.transform.rotation.y = q2.y();
  t_act2.transform.rotation.z = q2.z();
  t_act2.transform.rotation.w = q2.w();
  // Send the transformation
  tf_broadBody_->sendTransform(t_act2);


    // --------- right tail actuator
  t_act3.header.stamp = tNow;
  t_act3.header.frame_id = "paravane";
  t_act3.child_frame_id = "actuator3";
  // Actuator Translation
  t_act3.transform.translation.x = -0.2;
  t_act3.transform.translation.y = 0.02;
  t_act3.transform.translation.z = -0.035;
  // Actuator Rotation
  tf2::Quaternion q3;
  q3.setRPY(0, 0, servoFeedback4_);
  t_act3.transform.rotation.x = q3.x();
  t_act3.transform.rotation.y = q3.y();
  t_act3.transform.rotation.z = q3.z();
  t_act3.transform.rotation.w = q3.w();
  // Send the transformation
  tf_broadBody_->sendTransform(t_act3);

  iter_++;
}


// ///////////////
// UDP Msg Forward
// ///////////////
void TeensyInterfaceComponent::subServosInputCb(atl_msgs::msg::ServosInput::SharedPtr && msg)
{
  std::lock_guard lck(msgMtx_);

  // forward message through UDP
  std::vector<uint8_t> u((nServos + 2) * 4); 
  std::size_t oft = 0;

  // Timestamp
  const uint64_t time_ns = now().nanoseconds();
  const uint32_t time_ms = static_cast<uint32_t>((time_ns - t0_) / 1000000U);
  memcpy(u.data() + oft, &time_ms, 4);
  oft += 4;

  // Servo Inputs
  servoInput1_ = msg-> inputs[0].delta;
  memcpy(u.data() + oft, &servoInput1_, 4);
  oft += 4;

  servoInput2_ = msg-> inputs[1].delta;
  memcpy(u.data() + oft, &servoInput2_, 4);
  oft += 4;

  servoInput3_ = msg-> inputs[2].delta;
  memcpy(u.data() + oft, &servoInput3_, 4);
  oft += 4;

  servoInput4_ = msg-> inputs[3].delta;
  memcpy(u.data() + oft, &servoInput4_, 4);
  oft += 4;

  servoInput5_ = 0.0;
  memcpy(u.data() + oft, &servoInput5_, 4);
  oft += 4;

  // Sync byte
  const uint32_t sync = std::exchange(sync_, false);
  memcpy(u.data() + oft, &sync, 4);
  oft += 4;


  udp_->sendMsg(u);
}

}  // namespace atl

RCLCPP_COMPONENTS_REGISTER_NODE(atl::TeensyInterfaceComponent)
