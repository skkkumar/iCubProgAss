/* 
 * Copyright (C) 2014 iCub Facility - Istituto Italiano di Tecnologia
 * Author: Sriram Kumar
 * email: sriram.kishore@iit.it
 * website: www.robotcub.org
 * Permission is granted to copy, distribute, and/or modify this program
 * under the terms of the GNU General Public License, version 2 or any
 * later version published by the Free Software Foundation.
 *
 * A copy of the license can be found at
 * http://www.robotcub.org/icub/license/gpl.txt
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General
 * Public License for more details
*/
/*
For icub to look at the blue ball down and push it

Copyright (C) 2010 RobotCub Consortium
 
Author: Sriram Kumar 

CopyPolicy: Released under the terms of the GNU GPL v2.0. 

Scenario : 
To write a simple yarp code that does the following things: 

1. Make iCub look down to the table

2. Detect the blue ball in both images planes

3. Retrieve the ball position in the Cartesian domain

4. Ask iCub to reach for the ball and make it roll!
*/

#include <string>
#include <stdio.h>

#include <yarp/os/all.h>
#include <yarp/dev/all.h>
#include <yarp/sig/all.h>
#include <yarp/math/Math.h>
#include <iostream>

#include <iCub/ctrl/math.h>
YARP_DECLARE_DEVICES(icubmod)
using namespace std;
using namespace yarp::os;
using namespace yarp::dev;
using namespace yarp::sig;
using namespace yarp::math;
using namespace iCub::ctrl;

/***************************************************/
class CtrlModule: public RFModule
{
protected:
  PolyDriver drvArm, drvGaze;
  ICartesianControl *iarm;
  IGazeControl      *igaze;
  
  BufferedPort<ImageOf<PixelRgb> > imgLPortIn,imgRPortIn;
  Port imgLPortOut,imgRPortOut;
  RpcServer rpcPort;
  
  Mutex mutex;
  Vector cogL,cogR;
  Vector xHome, oHome;
  bool okL,okR;
  string side;
  int startup_context_id;
  /***************************************************/
  bool getCOG(ImageOf<PixelRgb> &img, Vector &cog)
  {
    int xMean=0;
    int yMean=0;
    int ct=0;
    
    for (int x=0; x<img.width(); x++)
    {
      for (int y=0; y<img.height(); y++)
      {
	PixelRgb &pixel=img.pixel(x,y);
	if ((pixel.b>5.0*pixel.r) && (pixel.b>5.0*pixel.g))
	{
	  xMean+=x;
	  yMean+=y;
	  ct++;
	}
      }
    }
    
    if (ct>0)
    {
      cog.resize(2);
      cog[0]=xMean/ct;
      cog[1]=yMean/ct;
      return true;
    }
    else
      return false;
  }
  
  /***************************************************/
  Vector retrieveTarget3D(const Vector &cogL, const Vector &cogR)
  {
    // FILL IN THE CODE
    Vector codvct;
    open_gaze_interface(false);
      igaze->get3DPoint(1,cogR,0.4,codvct);
      open_gaze_interface(false);
    return codvct;
  }
  
  /***************************************************/
  void fixate(Vector &x)
  {
    igaze->lookAtFixationPoint(x);
   igaze->setTrackingMode(true);
      x[0] += 0.05;
      x[1] += 0.08;
   
  }
  
  
  /***************************************************/
  Vector computeHandOrientation()
  {
    Vector od;
    od.resize(4);
    od[0]=0.0; od[1]=0.0; od[2]=1.0; od[3]=3.0;	
    return od;
    
  }
  
  /***************************************************/
  // To start different Arm with respect to the use
  Property cartesianMotordevice(string contLocation){
    Property option("(device cartesiancontrollerclient)");
    option.put("remote",("/icubSim/cartesianController/"+contLocation+"").c_str());
    option.put("local",("/cartesian_client/"+contLocation+"").c_str());
    return option;
  }
  
  void startiArmR(bool i = true)
  {
    drvArm.view(iarm);
    iarm->storeContext(&startup_context_id);
    if (i);
    iarm->setTrajTime(1.0);
  }
  
  void waitForMotion(string forWhat)
  {
    if (forWhat == "arm"){
     if (iarm->waitMotionDone()) {
      printf("Problem occured in moving the hand to position");
      }
    }
    else if (forWhat == "gaze"){
     if (igaze->waitMotionDone()) {
      printf("Problem occured in moving the head(gaze) to position");
      }
      
    }
  }
  void stopiArmR(bool i = true)
  {
    if (i){
      waitForMotion("arm");
    iarm->stopControl();
    }
    iarm->restoreContext(startup_context_id);
  }
  void enableTorso(){
    Vector newDof, curDof;
    iarm->getDOF(curDof);
    newDof=curDof;
    newDof[0]=1;
    newDof[1]=0;
    newDof[2]=1;        
    int axis=0;
    double min, max;
    iarm->getLimits(axis,&min,&max);
    iarm->setLimits(axis,min,10.0);
    iarm->setDOF(newDof,curDof);
  }
  int approachTargetWithHand(const Vector &x, const Vector &o)
  {
//     iarm->goToPose(x,o);
    iarm->goToPoseSync(x,o);
    waitForMotion("arm");
  }
  
  
  /***************************************************/
  void makeItRoll(Vector &x, const Vector &o)
  {
//     x[0] = x[0] * -1;
    x[1] = x[1] * -1;
//     x[2] = x[2] * -1;
    iarm->goToPoseSync(x,o);
    waitForMotion("arm");
  }
  
  /***************************************************/
  void open_gaze_interface(bool i = true){
    
    drvGaze.view(igaze);
    igaze->storeContext(&startup_context_id);
    if (i){
    igaze->setNeckTrajTime(0.8);
    igaze->setEyesTrajTime(0.4);
    }
  }
  void close_gaze_interface(bool i = true){
    if (i){
//       waitForMotion("gaze");
      igaze->stopControl();
    }
      igaze->restoreContext(startup_context_id);
    
  }
  int move_eye_down(){
    Vector ang(3);
    ang[0]= 0; 
    ang[1]= -40; 
    ang[2]= 0; 
    igaze->lookAtAbsAngles(ang); 
    waitForMotion("gaze");
  }
  
  
  void gaze_ball()
  {
    igaze->lookAtStereoPixels(cogL,cogR);
    waitForMotion("gaze");
  }
  
  void look_down()
  {
    open_gaze_interface();
    move_eye_down();
    gaze_ball();
    close_gaze_interface();
  }
  
  /***************************************************/
  void roll(const Vector &cogL, const Vector &cogR)
  {
    printf("detected cogs = (%s) (%s)\n",
	   cogL.toString(0,0).c_str(),cogR.toString(0,0).c_str());
    
    Vector x=retrieveTarget3D(cogL,cogR);
    printf("retrieved 3D point = (%s)\n",x.toString(3,3).c_str());
    printf("Arm used = (%s)\n",side.c_str()); 
    
    fixate(x);
    printf("fixating at (%s)\n",x.toString(3,3).c_str());
    
    Vector o=computeHandOrientation();
    printf("computed orientation = (%s)\n",o.toString(3,3).c_str());
    startiArmR();
    enableTorso();
    approachTargetWithHand(x,o);
    printf("approached\n");
    
    makeItRoll(x,o);
    printf("roll!\n");
    stopiArmR();
  }
  
  /***************************************************/
  int home_head()
  {
    open_gaze_interface();
    Vector ang(3);
    ang[0]= 0; 
    ang[1]= 0; 
    ang[2]= 0; 
    igaze->lookAtAbsAngles(ang); 
    close_gaze_interface();
  }
  
  int home_rightArm()
  {
    startiArmR();
    enableTorso();
    iarm->goToPoseSync(xHome,oHome);
//     startiArmR();
    stopiArmR();
  }
  
  void GetHomePose(){

    startiArmR(false);
    iarm->getPose(xHome,oHome);
    stopiArmR(false);
  }
  
  void home()
  {
    home_rightArm();
    home_head();
  }
  
  /***************************************************/ 
  
public:
  /***************************************************/
  bool configure(ResourceFinder &rf)
  {
    // FILL IN THE CODE
    
    imgLPortIn.open("/imgL:i");
    imgRPortIn.open("/imgR:i");
    
    imgLPortOut.open("/imgL:o");
    imgRPortOut.open("/imgR:o");
    
    rpcPort.open("/service");
    attach(rpcPort);
    
    string name=rf.find("name").asString().c_str();

     Property option;
     option = cartesianMotordevice("right_arm");

     if (!drvArm.open(option))
      return false;

    Property optGaze("(device gazecontrollerclient)");
    optGaze.put("remote","/iKinGazeCtrl");
    optGaze.put("local",("/"+name+"/gaze").c_str());
    if (!drvGaze.open(optGaze))
      return false;
    GetHomePose();
    return true;
  }
  
  /***************************************************/
  bool interruptModule()
  {
    imgLPortIn.interrupt();
    imgRPortIn.interrupt();
    return true;
  }
  
  /***************************************************/
  bool close()
  {
    drvArm.close();
    drvGaze.close();
    imgLPortIn.close();
    imgRPortIn.close();
    imgLPortOut.close();
    imgRPortOut.close();
    rpcPort.close();
    return true;
  }
  
  /***************************************************/
  bool respond(const Bottle &command, Bottle &reply)
  {
    string cmd=command.get(0).asString().c_str();
    if (cmd=="look_down")
    {
      look_down();
      reply.addString("Yep! I'm looking down now!");
      return true;
    }
    else if (cmd=="roll")
    {
      if (cogL[0] || cogR[0])
      {
	roll(cogL,cogR);
	reply.addString("Yeah! I've made it roll like a charm!");
      }
      else
	reply.addString("I don't see any object!");
      
      return true;
    }
    else if (cmd=="home")
    {
      home();
      reply.addString("I've got the hard work done! Going home.");
      return true;
    }
    else
      return RFModule::respond(command,reply);
  }
  
  /***************************************************/
  double getPeriod()
  {
    return 0.0;     // sync upon incoming images
  }
  
  /***************************************************/
  bool updateModule()
  {
    // get fresh images
    ImageOf<PixelRgb> *imgL=imgLPortIn.read();
    ImageOf<PixelRgb> *imgR=imgRPortIn.read();
    
    // interrupt sequence detected
    if ((imgL==NULL) || (imgR==NULL))
      return false;
    
    // compute the center-of-mass of pixels of our color
    mutex.lock();
    okL=getCOG(*imgL,cogL);
    okR=getCOG(*imgR,cogR);
    mutex.unlock();
    
    PixelRgb color;
    color.r=255; color.g=0; color.b=0;
    
    if (okL)
      draw::addCircle(*imgL,color,(int)cogL[0],(int)cogL[1],5);
    
    if (okR)
      draw::addCircle(*imgR,color,(int)cogR[0],(int)cogR[1],5);
    
    imgLPortOut.write(*imgL);
    imgRPortOut.write(*imgR);
    
    return true; 
  }
};


/***************************************************/
int main(int argc, char *argv[])
{   
  
  
  Network yarp;
  if (!yarp.checkNetwork())
    return -1;
  
  CtrlModule mod;
  YARP_REGISTER_DEVICES(icubmod)
  ResourceFinder rf;
  rf.setVerbose(true);
  rf.setDefault("name","tracker");
  //     rf.setDefault("period","0.02");
  
  rf.configure(argc,argv);
  return mod.runModule(rf);
}