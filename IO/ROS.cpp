#include "ROS.h"

#if HAVE_ROS 

#include "Modeling/World.h"
#include "Modeling/Paths.h"
#include "Simulation/WorldSimulation.h"
#include <meshing/PointCloud.h>
#include "Simulation/ControlledSimulator.h"
#include <Timer.h>
#include <ros/ros.h>
#include <ros/time.h>
#include <tf/transform_listener.h>
#include <tf/transform_broadcaster.h>
#include <geometry_msgs/Pose.h>
#include <geometry_msgs/PoseStamped.h>
#include <sensor_msgs/JointState.h>
#include <sensor_msgs/PointCloud2.h>
#include <trajectory_msgs/JointTrajectory.h>

bool IsBigEndian() {
  int n = 1;
  // little endian if true
  if(*(char *)&n == 1) return false;
  return true;
}

bool ROSToKlampt(const geometry_msgs::Point& pt,Vector3& kp)
{
  kp.x = pt.x;
  kp.y = pt.y;
  kp.z = pt.z;
  return true;
}


bool KlamptToROS(const Vector3& kp,geometry_msgs::Point& p)
{
  p.x = kp.x;
  p.y = kp.y;
  p.z = kp.z;
  return true;
}

bool ROSToKlampt(const geometry_msgs::Quaternion& q,Matrix3& kR)
{
  QuaternionRotation kq;
  kq.x = q.x;
  kq.y = q.y;
  kq.z = q.z;
  kq.w = q.w;
  kq.getMatrix(kR);
  return true;
}

bool KlamptToROS(const Matrix3& kR,geometry_msgs::Quaternion& q)
{
  QuaternionRotation kq;
  if(!kq.setMatrix(kR)) return false;
  q.x = kq.x;
  q.y = kq.y;
  q.z = kq.z;
  q.w = kq.w;
  return true;
}

bool ROSToKlampt(const geometry_msgs::Pose& pose,RigidTransform& kT)
{
  ROSToKlampt(pose.position,kT.t);
  ROSToKlampt(pose.orientation,kT.R);
  return true;
}

bool ROSToKlampt(const geometry_msgs::PoseStamped& pose,RigidTransform& kT)
{
  ROSToKlampt(pose.pose.position,kT.t);
  ROSToKlampt(pose.pose.orientation,kT.R);
  return true;
}

bool KlamptToROS(const RigidTransform& kT,geometry_msgs::Pose& pose)
{
  if(!KlamptToROS(kT.t,pose.position)) return false;
  if(!KlamptToROS(kT.R,pose.orientation)) return false;
  return true;
}

bool KlamptToROS(const RigidTransform& kT,geometry_msgs::PoseStamped& pose)
{
  if(!KlamptToROS(kT.t,pose.pose.position)) return false;
  if(!KlamptToROS(kT.R,pose.pose.orientation)) return false;
  return true;
}

bool ROSToKlampt(const sensor_msgs::JointState& js,Robot& krobot)
{
  map<string,int> indices;
  for(size_t i=0;i<krobot.linkNames.size();i++)
    indices[krobot.linkNames[i]] = (int)i;
  for(size_t i=0;i<js.name.size();i++) {
    if(indices.count(js.name[i])==0) {
      fprintf(stderr,"ROS JointState message has incorrect name %s\n",js.name[i].c_str());
      return false;
    }
    if(!js.position.empty()) krobot.q[indices[js.name[i]]] = js.position[i];
    if(!js.velocity.empty()) krobot.dq[indices[js.name[i]]] = js.velocity[i];
  }
  krobot.UpdateFrames();
  return true;
}

bool KlamptToROS(const Robot& krobot,sensor_msgs::JointState& js)
{
  js.name = krobot.linkNames;
  js.position.resize(krobot.linkNames.size());
  js.velocity.resize(krobot.linkNames.size());
  for(size_t i=0;i<krobot.linkNames.size();i++) {
    js.position[i] = krobot.q[i];
    js.velocity[i] = krobot.dq[i];
  }
  return true;
}

bool CommandedKlamptToROS(ControlledRobotSimulator& kcontroller,sensor_msgs::JointState& js)
{
  Config qcmd,vcmd,t;
  kcontroller.GetCommandedConfig(qcmd);
  kcontroller.GetCommandedVelocity(vcmd);
  kcontroller.GetLinkTorques(t);
  Robot& krobot = *kcontroller.robot;
  js.name = krobot.linkNames;
  js.position.resize(krobot.linkNames.size());
  js.velocity.resize(krobot.linkNames.size());
  js.effort.resize(krobot.linkNames.size(),0.0);
  for(size_t i=0;i<krobot.linkNames.size();i++) {
    js.position[i] = qcmd[i];
    js.velocity[i] = vcmd[i];
    js.effort[i] = t[i];
  }
  return true;
}

bool SensedKlamptToROS(ControlledRobotSimulator& kcontroller,sensor_msgs::JointState& js)
{
  Config qcmd,vcmd,t;
  kcontroller.GetSensedConfig(qcmd);
  kcontroller.GetSensedVelocity(vcmd);
  kcontroller.GetLinkTorques(t);
  Robot& krobot = *kcontroller.robot;
  js.name = krobot.linkNames;
  js.position.resize(krobot.linkNames.size());
  js.velocity.resize(krobot.linkNames.size());
  js.effort.resize(krobot.linkNames.size(),0.0);
  for(size_t i=0;i<krobot.linkNames.size();i++) {
    js.position[i] = qcmd[i];
    js.velocity[i] = vcmd[i];
    js.effort[i] = t[i];
  }
  return true;
}

bool ROSToKlampt(const trajectory_msgs::JointTrajectory& traj,LinearPath& kpath)
{
  kpath.times.resize(traj.points.size());
  kpath.milestones.resize(traj.points.size());
  for(size_t i=0;i<traj.points.size();i++) {
    kpath.times[i] = traj.points[i].time_from_start.toSec();
    kpath.milestones[i] = traj.points[i].positions;
  }
  return true;
}

bool KlamptToROS(const LinearPath& kpath,trajectory_msgs::JointTrajectory& traj)
{
  if(kpath.milestones.empty()) {
    traj.joint_names.clear();
    traj.points.clear();
    return true;
  }
  traj.joint_names.resize(kpath.milestones[0].n);
  for(int i=0;i<kpath.milestones[0].n;i++)
    traj.joint_names[i] = '0'+i;
  traj.points.resize(kpath.milestones.size());
  for(size_t i=0;i<kpath.milestones.size();i++) {
    traj.points[i].time_from_start = ros::Duration(kpath.times[i]);
    traj.points[i].positions = kpath.milestones[i];
  }
  return true;
}

bool KlamptToROS(const Robot& robot,const LinearPath& kpath,trajectory_msgs::JointTrajectory& traj)
{
  if(kpath.milestones.empty()) {
    traj.joint_names.clear();
    traj.points.clear();
    return true;
  }
  if((int)robot.linkNames.size() != kpath.milestones[0].size()) {
    fprintf(stderr,"KlamptToROS (LinearPath): path doesn't have same number of milestones as the robot\n");
    return false;
  }
  traj.joint_names = robot.linkNames;
  traj.points.resize(kpath.milestones.size());
  for(size_t i=0;i<kpath.milestones.size();i++) {
    traj.points[i].time_from_start = ros::Duration(kpath.times[i]);
    traj.points[i].positions = kpath.milestones[i];
  }
  return true;
}

bool KlamptToROS(const Robot& robot,const vector<int>& indices,const LinearPath& kpath,trajectory_msgs::JointTrajectory& traj)
{
  if(kpath.milestones.empty()) {
    traj.joint_names.clear();
    traj.points.clear();
    return true;
  }
  if((int)indices.size() != kpath.milestones[0].size()) {
    fprintf(stderr,"KlamptToROS (LinearPath): path doesn't have same number of milestones as the indices\n");
    return false;
  }
  traj.joint_names.resize(indices.size());
  for(size_t i=0;i<indices.size();i++) {
    if(indices[i] < 0 || indices[i] > robot.q.n)  {
      fprintf(stderr,"KlamptToROS (LinearPath): invalid index\n");
      return false;
    }
    traj.joint_names[i] = robot.linkNames[indices[i]];
  }
  traj.points.resize(kpath.milestones.size());
  for(size_t i=0;i<kpath.milestones.size();i++) {
    traj.points[i].time_from_start = ros::Duration(kpath.times[i]);
    traj.points[i].positions = kpath.milestones[i];
  }
  return true;
}

// Swap 2 byte, 16 bit values:
#define Swap2Bytes(val) \
 ( (((val) >> 8) & 0x00FF) | (((val) << 8) & 0xFF00) )
// Swap 4 byte, 32 bit values:

#define Swap4Bytes(val) \
 ( (((val) >> 24) & 0x000000FF) | (((val) >>  8) & 0x0000FF00) | \
   (((val) <<  8) & 0x00FF0000) | (((val) << 24) & 0xFF000000) )

// Swap 8 byte, 64 bit values:
#define Swap8Bytes(val) \
 ( (((val) >> 56) & 0x00000000000000FF) | (((val) >> 40) & 0x000000000000FF00) | \
   (((val) >> 24) & 0x0000000000FF0000) | (((val) >>  8) & 0x00000000FF000000) | \
   (((val) <<  8) & 0x000000FF00000000) | (((val) << 24) & 0x0000FF0000000000) | \
   (((val) << 40) & 0x00FF000000000000) | (((val) << 56) & 0xFF00000000000000) )

#define Swap2If(val,cond) (cond ? Swap2Bytes(val) : val)
#define Swap4If(val,cond) (cond ? Swap4Bytes(val) : val)
#define Swap8If(val,cond) (cond ? Swap8Bytes(val) : val)


template <class T> 
void UNPACK(const sensor_msgs::PointField& field,const unsigned char* data,T* out,bool swap_bigendian)
{
  static const int datasizes[] = {1,1,2,2,4,4,4,8};
  int stride = datasizes[field.datatype];
  data += field.offset;
  for(size_t i=0;i<field.count;i++) {
    switch(field.datatype) {
    case sensor_msgs::PointField::INT8:
      out[i] = T(*(char*)data);
      break;
    case sensor_msgs::PointField::UINT8:
      out[i] = T(*data);
      break;
    case sensor_msgs::PointField::INT16:
      out[i] = T(Swap2If(*(short*)data,swap_bigendian));
      break;
    case sensor_msgs::PointField::UINT16:
      out[i] = T(Swap2If(*(unsigned short*)data,swap_bigendian));
      break;
    case sensor_msgs::PointField::INT32:
      out[i] = T(Swap4If(*(int*)data,swap_bigendian));
      break;
    case sensor_msgs::PointField::UINT32:
      out[i] = T(Swap4If(*(unsigned int*)data,swap_bigendian));
      break;
    case sensor_msgs::PointField::FLOAT32:
    {
      int bytes = Swap4If(*(int*)data,swap_bigendian);
      unsigned char* bptr=((unsigned char*)&bytes);
      out[i] = T(*(float*)bptr);
      break;
    }
    case sensor_msgs::PointField::FLOAT64:
    {
      int64_t bytes = Swap8If(*(int64_t*)data,swap_bigendian);
      unsigned char* bptr=((unsigned char*)&bytes);
      out[i] = T(*(double*)bptr);
      break;
    }
    }
    data += stride;
  }
}

bool ROSToKlampt(const sensor_msgs::PointCloud2& pc,Meshing::PointCloud3D& kpc)
{
  int xfield=-1,yfield=-1,zfield=-1;
  int rgbfloat_field=-1;
  int rgbproperty=-1;
  vector<int> fieldmap(pc.fields.size(),-1);
  kpc.points.resize(0);
  kpc.propertyNames.resize(0);
  kpc.properties.resize(0);
  bool swap_bigendian = (IsBigEndian() != pc.is_bigendian);
  for(size_t i=0;i<pc.fields.size();i++) {
    if(pc.fields[i].name == "x") { xfield=(int)i; Assert(pc.fields[i].count==1); }
    else if(pc.fields[i].name == "y") { yfield=(int)i; Assert(pc.fields[i].count==1); }
    else if(pc.fields[i].name == "z") { zfield=(int)i; Assert(pc.fields[i].count==1); }
    else {
      fieldmap[i] = (int)kpc.propertyNames.size();
      if((pc.fields[i].name == "rgb" || pc.fields[i].name == "rgba" ) && pc.fields[i].datatype == sensor_msgs::PointField::FLOAT32) {
        //custom crap for Kinect2 bridge sending UINTs in float format
        rgbfloat_field = (int)i;
        rgbproperty = kpc.propertyNames.size();
        Assert(pc.fields[i].count == 1);
        fieldmap[i] = -1;
      }
      if(pc.fields[i].count==1) kpc.propertyNames.push_back(pc.fields[i].name);
      else {
        for(size_t j=0;j<pc.fields[i].count;j++) {
          char suffix = '0'+j;
          kpc.propertyNames.push_back(pc.fields[i].name+suffix);
        }
      }
    }
  }
  Assert(pc.data.size() >= pc.row_step*pc.height);
  int ofs = 0;
  Vector3 pt(0.0);
  Vector propertyTemp(kpc.propertyNames.size());
  for(unsigned int i=0;i<pc.height;i++) {
    int vofs = ofs;
    for(unsigned int j=0;j<pc.width;j++) {
      if(xfield >=0) UNPACK<Real>(pc.fields[xfield],&pc.data[vofs],&pt.x,swap_bigendian);
      if(yfield >=0) UNPACK<Real>(pc.fields[yfield],&pc.data[vofs],&pt.y,swap_bigendian);
      if(zfield >=0) UNPACK<Real>(pc.fields[zfield],&pc.data[vofs],&pt.z,swap_bigendian);
      if(IsFinite(pt.x) && IsFinite(pt.y) && IsFinite(pt.z)) {
        kpc.points.push_back(pt);
        if(rgbfloat_field >= 0) {
          //hack
          const unsigned char* data = &pc.data[vofs]+pc.fields[rgbfloat_field].offset;
          unsigned int rgb = Swap4If(*((unsigned int*)data),swap_bigendian);
          Real* out = &propertyTemp[rgbproperty];
          *out = Real(rgb);
        }
        int pofs = 0;
        for(size_t k=0;k<pc.fields.size();k++) {
          if(fieldmap[k] < 0) continue;
          UNPACK<Real>(pc.fields[k],&pc.data[vofs],&propertyTemp[pofs],swap_bigendian);
          pofs +=pc.fields[k].count;
        }
        kpc.properties.push_back(propertyTemp);
      }
      vofs += pc.point_step;
    }
    ofs += pc.row_step;
  }
  //printf("Read %d points from ROS\n",kpc.points.size());
  return true;
}

bool KlamptToROS(const Meshing::PointCloud3D& kpc,sensor_msgs::PointCloud2& pc)
{
  pc.is_bigendian = IsBigEndian();
  pc.height = 1;
  pc.width = kpc.points.size();
  pc.point_step = 4*(3+kpc.propertyNames.size());
  pc.row_step = pc.width*pc.point_step;
  pc.fields.resize(3+kpc.propertyNames.size());
  pc.fields[0].name = "x";
  pc.fields[1].name = "y";
  pc.fields[2].name = "z";
  for(size_t i=0;i<kpc.propertyNames.size();i++)
    pc.fields[3+i].name = kpc.propertyNames[i];
  for(size_t i=0;i<pc.fields.size();i++) {
    pc.fields[0].datatype = sensor_msgs::PointField::FLOAT32;
    pc.fields[0].offset = i*4;
    pc.fields[0].count = 1;
  }
  int ofs = 0;
  pc.data.resize(pc.row_step);
  for(size_t i=0;i<kpc.points.size();i++) {
    *(float*)&pc.data[ofs] = kpc.points[i].x; ofs += 4;
    *(float*)&pc.data[ofs] = kpc.points[i].y; ofs += 4;
    *(float*)&pc.data[ofs] = kpc.points[i].z; ofs += 4;
    for(size_t j=0;j<kpc.propertyNames.size();j++) {
      *(float*)&pc.data[ofs] = kpc.properties[i][j]; ofs += 4;
    }
  }
  return true;
}

bool ROSToKlampt(const tf::Transform& T,RigidTransform& kT)
{
  kT.t.set(T.getOrigin().x(),T.getOrigin().y(),T.getOrigin().z());
  Vector3 row1(T.getBasis()[0].x(),T.getBasis()[0].y(),T.getBasis()[0].z());
  Vector3 row2(T.getBasis()[1].x(),T.getBasis()[1].y(),T.getBasis()[1].z());
  Vector3 row3(T.getBasis()[2].x(),T.getBasis()[2].y(),T.getBasis()[2].z());
  kT.R.setRow1(row1);
  kT.R.setRow2(row2);
  kT.R.setRow3(row3);
  return true;
}

bool KlamptToROS(const RigidTransform& kT,tf::Transform& T)
{
  T.getOrigin().setValue(kT.t.x,kT.t.y,kT.t.z);
  Vector3 row;
  kT.R.getRow1(row);
  T.getBasis()[0].setValue(row.x,row.y,row.z);
  kT.R.getRow2(row);
  T.getBasis()[1].setValue(row.x,row.y,row.z);
  kT.R.getRow3(row);
  T.getBasis()[2].setValue(row.x,row.y,row.z);
  return true;
}



SmartPointer<ros::NodeHandle> gRosNh;
int gRosQueueSize = 1;
bool gRosSubscribeError = false;
string gRosSubscribeErrorWhere;

class ROSSubscriberBase
{
public:
  ROSSubscriberBase() : error(false),numMessages(0) {}
  virtual ~ROSSubscriberBase() { unsubscribe(); }
  void unsubscribe() {
    this->topic = "";
    this->numMessages = 0;
    sub = ros::Subscriber();
  }
  virtual void endUpdate() {}

  ros::Subscriber sub;
  string topic;
  bool error;
  std_msgs::Header header;
  int numMessages;
};

class ROSPublisherBase
{
public:
  virtual ~ROSPublisherBase() {}
  ros::Publisher pub;
  string topic;
};

typedef map<string,SmartPointer<ROSSubscriberBase> > SubscriberList;
typedef map<string,SmartPointer<ROSPublisherBase> > PublisherList;
SubscriberList gSubscribers;
PublisherList gPublishers;




template <class Type,class Msg>
class ROSSubscriber : public ROSSubscriberBase
{
public:
  Type& obj;
  Msg msg;
  ROSSubscriber(Type& _obj,const std::string& _topic):obj(_obj) {
    this->topic = _topic;
    sub = gRosNh->subscribe(_topic,gRosQueueSize,&ROSSubscriber<Type,Msg>::callback, this);
  }
  void callback(const Msg& msg) {
    numMessages++;
    header = msg.header;
    //this->msg = msg;
    error = (!ROSToKlampt(msg,obj));
    if(error) {
      gRosSubscribeError = true;
      gRosSubscribeErrorWhere = this->topic;
    }
  }
  virtual void endUpdate() {
    /*
    error = (!ROSToKlampt(msg,obj));
    if(error) {
      gRosSubscribeError = true;
      gRosSubscribeErrorWhere = this->topic;
    }
    */
  }
};


class ROSTfSubscriber : public ROSSubscriberBase
{
public:
  tf::TransformListener listener;
  map<string,RigidTransform*> transforms;
  ROSTfSubscriber():listener(*gRosNh) { this->topic = "tf"; }
  void update() {
    tf::StampedTransform transform;
    try{
      for(map<string,RigidTransform*>::iterator i=transforms.begin();i!=transforms.end();i++) {
	listener.lookupTransform(i->first, "world",  
				 ros::Time::now(), transform);
	ROSToKlampt(transform,*i->second);
      }
    }
    catch (tf::TransformException ex){
      gRosSubscribeError = true;
      gRosSubscribeErrorWhere = "tf";
    }
  }
};


template <class Type,class Msg>
class ROSPublisher : public ROSPublisherBase
{
public:
  Msg msg;
  ROSPublisher(const std::string& topic) {
    this->topic = topic;
    pub = gRosNh->advertise<Msg>(topic,gRosQueueSize,false);
    msg.header.seq = 0;
  }
  void publish(const Type& obj) {
    //ignore if no subscribers
    if(pub.getNumSubscribers () == 0) return;
    //do conversion if there's a subscriber
    msg.header.stamp = ros::Time::now();
    msg.header.seq++;
    msg.header.frame_id = "0";
    KlamptToROS(obj,msg);
    pub.publish(msg);
  }
};

class ROSTfPublisher : public ROSPublisherBase
{
public:
  tf::TransformBroadcaster broadcaster;
  ROSTfPublisher() { this->topic = "tf"; }
  void send(const string& name,const RigidTransform& T,const char* parent="world") {
    tf::Transform transform;
    KlamptToROS(T,transform);
    broadcaster.sendTransform(tf::StampedTransform(transform, ros::Time::now(), parent, name));
  }
};


bool ROSInit(const char* nodeName)
{
  if(gRosNh) return true;
  int argc = 1;
  char* argv [1]={(char*)"klampt"}; 
  ros::init(argc, &argv[0], nodeName);
  gRosNh = new ros::NodeHandle;
  return true;
}

bool ROSShutdown()
{
  if(gRosNh) {
    ros::shutdown();
    gRosNh = NULL;
  }
  return true;
}

bool ROSSetQueueSize(int size)
{
  if(size <= 0) return false;
  gRosQueueSize = size;
  return true;
}

template<class Type,class Msg>
bool RosSubscribe(Type& obj,const string& topic)
{
  if(!ROSInit()) return false;
  SubscriberList::iterator i=gSubscribers.find(topic); 
  if(i!=gSubscribers.end()) { 
    printf("ROSSubscribe: Unsubscribing old subscriber to topic %s\n",topic.c_str());
    i->second->unsubscribe();
    i->second = NULL;
  }
  ROSSubscriber<Type,Msg>* sub = new ROSSubscriber<Type,Msg>(obj,topic); 
  if(!sub->sub) {
    fprintf(stderr,"ROSSubscribe: Unable to subscribe to topic %s, maybe wrong type\n",topic.c_str());
    return false;
  }
  gSubscribers[topic] = sub; 
  return true; 
}

template<class Type,class PubType,class Msg>
bool RosPublish3(Type& obj,const string& topic)
{
  if(!ROSInit()) return false;
  PublisherList::iterator i=gPublishers.find(topic); 
  PubType* pub;
  if(i==gPublishers.end()) { 
    pub = new PubType(topic); 
    gPublishers[topic] = pub; 
  } 
  else { 
    pub = dynamic_cast<PubType*>((ROSPublisherBase*)i->second);
    if(!pub) return false;
  }
  pub->publish(obj); 
  return true; 
}

template<class Type,class PubType,class Msg>
bool RosPublish2(const Type& obj,const string& topic)
{
  if(!ROSInit()) return false;
  PublisherList::iterator i=gPublishers.find(topic); 
  PubType* pub;
  if(i==gPublishers.end()) { 
    pub = new PubType(topic); 
    gPublishers[topic] = pub; 
  } 
  else { 
    pub = dynamic_cast<PubType*>((ROSPublisherBase*)i->second);
    if(!pub) return false;
  }
  pub->publish(obj); 
  return true; 
}

template<class Type,class Msg>
bool RosPublish(const Type& obj,const string& topic)
{
  return RosPublish2<Type,ROSPublisher<Type,Msg>,Msg>(obj,topic);
}


bool ROSPublishTransforms(const RobotWorld& world,const char* frameprefix)
{
  if(!ROSInit()) return false;
  string prefix = frameprefix;
  ROSTfPublisher* tf;
  if(gPublishers.count("tf")==0) {
    tf = new ROSTfPublisher();
    gPublishers["tf"] = tf;
  }
  else {
    tf = dynamic_cast<ROSTfPublisher*>((ROSPublisherBase*)gPublishers["tf"]);
    if(tf==NULL) return false;
  }
  for(size_t i=0;i<world.rigidObjects.size();i++)
    tf->send(prefix+"/"+world.rigidObjects[i].name,world.rigidObjects[i].object->T);
  for(size_t i=0;i<world.robots.size();i++) {
    for(size_t j=0;j<world.robots[i].robot->links.size();j++) {
      tf->send(prefix+"/"+world.robots[i].name+"/"+world.robots[i].robot->linkNames[j],world.robots[i].robot->links[i].T_World);
    }
  }
  return true;
}

bool ROSPublishTransforms(const WorldSimulation& sim,const char* frameprefix)
{
  if(!ROSInit()) return false;
  string prefix = frameprefix;
  ROSTfPublisher* tf;
  if(gPublishers.count("tf")==0) {
    tf = new ROSTfPublisher();
    gPublishers["tf"] = tf;
  }
  else {
    tf = dynamic_cast<ROSTfPublisher*>((ROSPublisherBase*)gPublishers["tf"]);
    if(tf==NULL) return false;
  }
  for(size_t i=0;i<sim.world->rigidObjects.size();i++) {
    RigidTransform T;
    sim.odesim.object(i)->GetTransform(T);
    tf->send(prefix+"/"+sim.world->rigidObjects[i].name,T);
  }
  for(size_t i=0;i<sim.world->robots.size();i++) {
    for(size_t j=0;j<sim.world->robots[i].robot->links.size();j++) {
      RigidTransform T;
      sim.odesim.robot(i)->GetLinkTransform(j,T);
      tf->send(prefix+"/"+sim.world->robots[i].name+"/"+sim.world->robots[i].robot->linkNames[j],T);
    }
  }
  return true;
}

bool ROSPublishTransforms(const Robot& robot,const char* frameprefix)
{
  if(!ROSInit()) return false;
  string prefix = frameprefix;
  ROSTfPublisher* tf;
  if(gPublishers.count("tf")==0) {
    tf = new ROSTfPublisher();
    gPublishers["tf"] = tf;
  }
  else {
    tf = dynamic_cast<ROSTfPublisher*>((ROSPublisherBase*)gPublishers["tf"]);
    if(tf==NULL) return false;
  }
  for(size_t j=0;j<robot.links.size();j++) 
    tf->send(prefix+"/"+robot.linkNames[j],robot.links[j].T_World);
  return true;
}

bool ROSPublishTransform(const RigidTransform& T,const char* frame)
{
  ROSTfPublisher* tf;
  if(gPublishers.count("tf")==0) {
    tf = new ROSTfPublisher();
    gPublishers["tf"] = tf;
  }
  else {
    tf = dynamic_cast<ROSTfPublisher*>((ROSPublisherBase*)gPublishers["tf"]);
    if(tf==NULL) return false;
  }
  tf->send(frame,T);
  return true;
}

bool ROSPublishPose(const RigidTransform& T,const char* topic)
{
  return RosPublish<RigidTransform,geometry_msgs::PoseStamped>(T,topic);
}

bool ROSPublishJointState(const Robot& robot,const char* topic)
{
  return RosPublish<Robot,sensor_msgs::JointState>(robot,topic);
}

bool ROSPublishPointCloud(const Meshing::PointCloud3D& pc,const char* topic)
{
  return RosPublish<Meshing::PointCloud3D,sensor_msgs::PointCloud2>(pc,topic);
}

bool ROSPublishTrajectory(const LinearPath& path,const char* topic) 
{
  return RosPublish<LinearPath,trajectory_msgs::JointTrajectory>(path,topic);
}

bool ROSPublishTrajectory(const Robot& robot,const LinearPath& path,const char* topic)
{
  if(!ROSInit()) return false;
  PublisherList::iterator i=gPublishers.find(topic); 
  ROSPublisher<LinearPath,trajectory_msgs::JointTrajectory>* pub;
  if(i==gPublishers.end()) { 
    pub = new ROSPublisher<LinearPath,trajectory_msgs::JointTrajectory>(topic); 
    gPublishers[topic] = pub; 
  } 
  else { 
    pub = dynamic_cast<ROSPublisher<LinearPath,trajectory_msgs::JointTrajectory>*>((ROSPublisherBase*)i->second);
    if(!pub) return false;
  }
  //ignore if no subscribers
  if(pub->pub.getNumSubscribers () == 0) return true;
  //do conversion if there's a subscriber
  pub->msg.header.stamp = ros::Time::now();
  pub->msg.header.seq++;
  pub->msg.header.frame_id = "0";
  KlamptToROS(robot,path,pub->msg);
  pub->pub.publish(pub->msg);
  return true; 
}

bool ROSPublishTrajectory(const Robot& robot,const vector<int>& indices,const LinearPath& path,const char* topic)
{
  if(!ROSInit()) return false;
  PublisherList::iterator i=gPublishers.find(topic); 
  ROSPublisher<LinearPath,trajectory_msgs::JointTrajectory>* pub;
  if(i==gPublishers.end()) { 
    pub = new ROSPublisher<LinearPath,trajectory_msgs::JointTrajectory>(topic); 
    gPublishers[topic] = pub; 
  } 
  else { 
    pub = dynamic_cast<ROSPublisher<LinearPath,trajectory_msgs::JointTrajectory>*>((ROSPublisherBase*)i->second);
    if(!pub) return false;
  }
  //ignore if no subscribers
  if(pub->pub.getNumSubscribers () == 0) return true;
  //do conversion if there's a subscriber
  pub->msg.header.stamp = ros::Time::now();
  pub->msg.header.seq++;
  pub->msg.header.frame_id = "0";
  KlamptToROS(robot,indices,path,pub->msg);
  pub->pub.publish(pub->msg);
  return true; 
}


class ROSCommandedPublisher : public ROSPublisherBase
{
public:
  typedef sensor_msgs::JointState Msg;
  Msg msg;
  ROSCommandedPublisher(const std::string& topic) {
    this->topic = topic;
    pub = gRosNh->advertise<Msg>(topic,gRosQueueSize,false);
    msg.header.seq = 0;
  }
  void publish(ControlledRobotSimulator& obj) {
    //ignore if no subscribers
    if(pub.getNumSubscribers () == 0) return;
    //do conversion if there's a subscriber
    msg.header.stamp = ros::Time::now();
    msg.header.seq++;
    msg.header.frame_id = "0";
    CommandedKlamptToROS(obj,msg);
    pub.publish(msg);
  }
};

class ROSSensedPublisher : public ROSPublisherBase
{
public:
  typedef sensor_msgs::JointState Msg;
  Msg msg;
  ROSSensedPublisher(const std::string& topic) {
    this->topic = topic;
    pub = gRosNh->advertise<Msg>(topic,gRosQueueSize,false);
    msg.header.seq = 0;
  }
  void publish(ControlledRobotSimulator& obj) {
    //ignore if no subscribers
    if(pub.getNumSubscribers () == 0) return;
    //do conversion if there's a subscriber
    msg.header.stamp = ros::Time::now();
    msg.header.seq++;
    msg.header.frame_id = "0";
    SensedKlamptToROS(obj,msg);
    pub.publish(msg);
  }
};
bool ROSPublishCommandedJointState(ControlledRobotSimulator& robot,const char* topic)
{
  return RosPublish3<ControlledRobotSimulator,ROSCommandedPublisher,sensor_msgs::JointState>(robot,topic);
}

bool ROSPublishSensedJointState(ControlledRobotSimulator& robot,const char* topic)
{
  return RosPublish3<ControlledRobotSimulator,ROSSensedPublisher,sensor_msgs::JointState>(robot,topic);
}

bool ROSSubscribeTransforms(RobotWorld& world,const char* frameprefix);
bool ROSSubscribeTransforms(Robot& robot,const char* frameprefix);
bool ROSSubscribeTransform(RigidTransform& T,const char* frameprefix);
bool ROSSubscribePose(RigidTransform& T,const char* topic)
{
  //TODO: handle un-stamped messages?
  return RosSubscribe<RigidTransform,geometry_msgs::PoseStamped>(T,topic);
}
bool ROSSubscribeJointState(Robot& robot,const char* topic)
{
  return RosSubscribe<Robot,sensor_msgs::JointState>(robot,topic);
}
bool ROSSubscribePointCloud(Meshing::PointCloud3D& pc,const char* topic)
{
  printf("ROSSubscribePointCLoud %s\n",topic);
  return RosSubscribe<Meshing::PointCloud3D,sensor_msgs::PointCloud2>(pc,topic);
}

bool ROSSubscribeTrajectory(LinearPath& path,const char* topic) 
{
  return RosSubscribe<LinearPath,trajectory_msgs::JointTrajectory>(path,topic);
}

bool ROSSubscribeUpdate()
{
  if(gSubscribers.empty() && gPublishers.empty()) return false; 
  //Timer timer;
  for(SubscriberList::iterator i=gSubscribers.begin();i!=gSubscribers.end();i++)
    i->second->numMessages = 0;
  gRosSubscribeError = false;
  ros::spinOnce();
  //TODO: tf listener is running in background, do we want a delay?
  if(gSubscribers.count("tf") != 0) {
    ROSTfSubscriber* tf=dynamic_cast<ROSTfSubscriber*>((ROSSubscriberBase*)gSubscribers["tf"]);
    if(tf != NULL) tf->update();
  }
  bool updated = false;
  for(SubscriberList::iterator i=gSubscribers.begin();i!=gSubscribers.end();i++)
    if(i->second->numMessages > 0) {
      //printf("%d updates to %s\n",i->second->numMessages,i->second->topic.c_str());
      updated = true;
      i->second->endUpdate();
    }
  //printf("ROS Update in time %gs\n",timer.ElapsedTime());
  if(gRosSubscribeError) {
    fprintf(stderr,"ROS: Error converting topic %s to Klampt format\n",gRosSubscribeErrorWhere.c_str());
    return false;
  }
  return updated;
}

bool ROSDetach(const char* topic)
{
  if(gSubscribers.count(topic) != 0) {
    gSubscribers[topic] = NULL;
    gSubscribers.erase(gSubscribers.find(topic));
    return true;
  }
  fprintf(stderr,"ROSDetach: topic %s not published/subscribed\n",topic);
  return false;
}

int ROSNumSubscribedTopics() { return (int)gSubscribers.size(); }
int ROSNumPublishedTopics() { return (int)gPublishers.size(); }

bool ROSIsConnected(const char* topic) {
  if(gSubscribers.count(topic) != 0) {
    return gSubscribers[topic]->sub.getNumPublishers() >  0;
  }
  else if(gPublishers.count(topic) != 0) {
    return gPublishers[topic]->pub.getNumSubscribers() >  0;
  }
  return false;
}

std::string ROSFrame(const char* topic)
{
  if(gSubscribers.count(topic) != 0) {
    return gSubscribers[topic]->header.frame_id;
  }
  return "";
}

bool ROSWaitForUpdate(const char* topic,double timeout)
{
  if(gSubscribers.count(topic) == 0) return false;
  ROSSubscriberBase* s = gSubscribers[topic];
  int oldNumMessages = s->numMessages;
  Timer timer;
  while(timer.ElapsedTime() < timeout) {
    ros::spinOnce();
    ros::Duration(Min(timeout-timer.ElapsedTime(),0.001)).sleep();
    if(s->numMessages > oldNumMessages) return true;
  }
  return false;
}

bool ROSHadUpdate(const char* topic)
{
  if(gSubscribers.count(topic) == 0) return false;
  ROSSubscriberBase* s = gSubscribers[topic];
  return s->numMessages > 0;
}

#else

#include "Modeling/World.h"
bool ROSInit(const char* nodename) { fprintf(stderr,"ROSInit(): Klamp't was not built with ROS support\n"); return false; }
bool ROSShutdown() { return false; }
bool ROSSetQueueSize(int size) { return false; }
bool ROSPublishTransforms(const RobotWorld& world,const char* frameprefix) { return false; }
bool ROSPublishTransforms(const WorldSimulation& sim,const char* frameprefix) { return false; }
bool ROSPublishTransforms(const Robot& robot,const char* frameprefix) { return false; }
bool ROSPublishTransform(const RigidTransform& T,const char* frame) { return false; }
bool ROSPublishPose(const RigidTransform& T,const char* topic) { return false; }
bool ROSPublishJointState(const Robot& robot,const char* topic) { return false; }
bool ROSPublishPointCloud(const Meshing::PointCloud3D& pc,const char* topic) { return false; }
bool ROSPublishTrajectory(const LinearPath& T,const char* topic) { return false; }
bool ROSPublishTrajectory(const Robot& robot,const LinearPath& path,const char* topic) { return false; }
bool ROSPublishTrajectory(const Robot& robot,const vector<int>& indices,const LinearPath& path,const char* topic) { return false; }
bool ROSPublishCommandedJointState(ControlledRobotSimulator& robot,const char* topic) { return false; }
bool ROSPublishSensedJointState(ControlledRobotSimulator& robot,const char* topic) { return false; }
bool ROSSubscribeTransforms(RobotWorld& world,const char* frameprefix) { return false; }
bool ROSSubscribeTransforms(Robot& robot,const char* frameprefix) { return false; }
bool ROSSubscribeTransform(RigidTransform& T,const char* frameprefix) { return false; }
bool ROSSubscribePose(RigidTransform& T,const char* topic) { return false; }
bool ROSSubscribeJointState(Robot& robot,const char* topic) { return false; }
bool ROSSubscribePointCloud(Meshing::PointCloud3D& pc,const char* topic)  { return false; }
bool ROSSubscribeTrajectory(LinearPath& T,const char* topic) { return false; }
bool ROSSubscribeTrajectory(Robot& robot,LinearPath& path,const char* topic) { return false; }
bool ROSSubscribeUpdate() { return false; }
bool ROSDetach(const char* topic) { return false; }
int ROSNumSubscribedTopics() { return 0; }
int ROSNumPublishedTopics() { return 0; }
bool ROSIsConnected(const char* topic) { return false; }
std::string ROSFrame(const char* topic) { return ""; }
bool ROSWaitForUpdate(const char* topic,double timeout) { return false; }
bool ROSHadUpdate(const char* topic) { return false; }

#endif //HAVE_ROS

