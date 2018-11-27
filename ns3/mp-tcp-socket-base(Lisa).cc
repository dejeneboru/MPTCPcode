/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Adrian Sai-wah Tam
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Adrian Sai-wah Tam <adrian.sw.tam@gmail.com>
 */

#define NS_LOG_APPEND_CONTEXT \
  if (m_node) { std::clog << Simulator::Now ().GetSeconds () << " [node " << m_node->GetId () << "] "; }

#include "mp-tcp-socket-base.h"


#include "ns3/abort.h"
#include "ns3/node.h"
#include "ns3/inet-socket-address.h"
#include "ns3/log.h"
#include "ns3/ipv4.h"
#include "ns3/ipv4-interface-address.h"
#include "ns3/ipv4-route.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/simulation-singleton.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/string.h"
#include "ns3/tcp-typedefs.h"
#include "ns3/ipv4-end-point.h"
#include "ns3/tcp-header.h"
#include "ns3/rtt-estimator.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/gnuplot.h"
#include "ns3/error-model.h"
#include "time.h"
#include "ns3/point-to-point-channel.h"
#include "ns3/point-to-point-net-device.h"
#include "mp-tcp-typedefs.h"
#include "ns3/tcp-socket-base.h"
#include "mp-tcp-l4-protocol.h"
#include <math.h>
//#include "nstime.h"
#include <vector>
#include <map>
#include <algorithm>
#include <stdlib.h>
#include <stdio.h>        
#include <unistd.h>        
#include <signal.h>        
#include <string.h>        
#include <sys/time.h>    
#include <iostream>
#include <fstream>
#include <iostream>
#include <sys/select.h>
#include <time.h>
#include <pthread.h>

NS_LOG_COMPONENT_DEFINE ("MpTcpSocketBase");

using namespace std;

namespace ns3 {


NS_OBJECT_ENSURE_REGISTERED (MpTcpSocketBase);

//TypeId
//MpTcpSocketBase::GetTypeId (void)
//{
//  static TypeId tid = TypeId ("ns3::MpTcpSocketBase")
//    .SetParent<TcpSocketBase> ()
//    .AddConstructor<MpTcpSocketBase> ()
//    .AddAttribute ("ReTxThreshold", "Threshold for fast retransmit",
//                    UintegerValue (3),
//                    MakeUintegerAccessor (&MpTcpSocketBase::m_retxThresh),
//                    MakeUintegerChecker<uint32_t> ())
//    .AddTraceSource ("CongestionWindow",
//                     "The TCP connection's congestion window",
//                     MakeTraceSourceAccessor (&MpTcpSocketBase::m_cWnd))
//  ;
//  return tid;
//}

TypeId
MpTcpSocketBase::GetTypeId (void)
{
	 static TypeId tid = TypeId("ns3::MpTcpSocketBase")
			 .SetParent<TcpSocketBase> ()
			 .AddConstructor<MpTcpSocketBase> ()
	 ;
	 return tid;
}

//MpTcpSocketBase::MpTcpSocketBase (void) : m_retxThresh (3), m_inFastRec (false)
//{
//  NS_LOG_FUNCTION (this);
//}

MpTcpSocketBase::MpTcpSocketBase (void)
{

	NS_LOG_FUNCTION(this);
	    //srand (time(NULL));
	    srand (65536);
	    m_state        = CLOSED;
	    m_node         = 0;
	    m_connected    = false;
	    m_stateMachine = new MpTcpStateMachine();

	    // multipath variable
	    m_maxSubFlowNumber = 255;
	    m_minSubFlowNumber = 1;
	    m_subFlowNumber = 0;
	    m_mpState     = MP_NONE;
	    m_mpSendState = MP_NONE;
	    m_mpRecvState = MP_NONE;
	    m_mpEnabled   = false;
	    m_addrAdvertised = false;
	    m_localToken  = 0;
	    m_remoteToken = 0;
	    m_aggregatedBandwidth = 0;
	    m_lastUsedsFlowIdx = 0;
            updateFlag = true;
            isCatch = false;
            filesize2=100000;
            filesize3=200000; 
            filesize4=300000;
            filesize5=400000;  
            filesize6=500000;
            filesize7=600000;  
            filesize8=700000;
            filesize9=800000;  
            filesize10=900000;
            filesize11=200000;  

	    totalCwnd = 0;
	    meanTotalCwnd = 0;

	    // flow control
	    m_rxBufSize  = 0;
	    m_lastRxAck  = 0;

	    m_skipRetxResched = false;
	    m_dupAckCount  = 0;
	    m_delAckCount  = 0;

	    nextTxSequence   = 1;
	    nextRxSequence   = 1;
	    remoteRecvWnd    = 1;
	    unAckedDataCount = 0;

	    m_algoCC = RTT_Compensator;
	    m_distribAlgo = myAlgo2;
	    m_algoPR = NoPR_Algo;

	    nbRejected = 0;
	    nbReceived = 0;
	    unOrdMaxSize  = 0;

	    client = false;
	    server = false;

	    lastRetransmit = 0;
	    frtoStep = Step_1;
	    useFastRecovery = false;
}

//MpTcpSocketBase::MpTcpSocketBase (const MpTcpSocketBase& sock)
//  : TcpSocketBase (sock),
//    m_cWnd (sock.m_cWnd),
//    m_ssThresh (sock.m_ssThresh),
//    m_initialCWnd (sock.m_initialCWnd),
//    m_retxThresh (sock.m_retxThresh),
//    m_inFastRec (false)
//{
//  NS_LOG_FUNCTION (this);
//  NS_LOG_LOGIC ("Invoked the copy constructor");
//}

MpTcpSocketBase::MpTcpSocketBase (Ptr<Node> node)
	: subflows (0), localAddrs (0), remoteAddrs (0)
  {
    NS_LOG_FUNCTION (node->GetId() << this);
	//srand (time(NULL));
	srand (65536);
	m_state        = CLOSED;
	m_node         = node;
	m_connected    = false;
	m_stateMachine = new MpTcpStateMachine();
	m_mptcp = node->GetObject<MpTcpL4Protocol> ();
	NS_ASSERT_MSG(m_mptcp != 0, "node->GetObject<MpTcpL4Protocol> () returned NULL");

	// multipath variable
	m_maxSubFlowNumber = 255;
	m_minSubFlowNumber = 1;
	m_subFlowNumber    = 0;
	m_mpState = MP_NONE;
	m_mpSendState = MP_NONE;
	m_mpRecvState = MP_NONE;
	m_mpEnabled = false;
	m_addrAdvertised = false;
	m_localToken = 0;
	m_remoteToken = 0;
	m_aggregatedBandwidth = 0;
	m_lastUsedsFlowIdx = 0;
        updateFlag = true;
        isCatch = false;
            filesize2=100000;
            filesize3=200000; 
            filesize4=300000;
            filesize5=400000;  
            filesize6=500000;
            filesize7=600000;  
            filesize8=700000;
            filesize9=800000;  
            filesize10=900000;
            filesize11=1000000;  
	totalCwnd = 0;
	meanTotalCwnd = 0;

    nextTxSequence   = 1;
    nextRxSequence   = 1;

	m_skipRetxResched = false;
	m_dupAckCount = 0;
	m_delAckCount = 0;

	// flow control
	m_rxBufSize = 0;
	m_lastRxAck = 0;
    congestionWnd = 1;

	remoteRecvWnd    = 1;
	unAckedDataCount = 0;

	m_algoCC = RTT_Compensator;
	m_distribAlgo = myAlgo2;
	m_algoPR = NoPR_Algo;

	nbRejected = 0;
	nbReceived = 0;
	unOrdMaxSize  = 0;

	client = false;
	server = false;

	lastRetransmit = 0;
	frtoStep = Step_1;
	useFastRecovery = false;

}



//MpTcpSocketBase::~MpTcpSocketBase (void)
//{
//}

MpTcpSocketBase::~MpTcpSocketBase (void)
{
  NS_LOG_FUNCTION (m_node->GetId() << this);
  m_state        = CLOSED;
  m_node         = 0;
  m_mptcp        = 0;
  m_connected    = false;
  delete m_stateMachine;
  delete m_pendingData;
  //subflows.clear();
  for(int i = 0; i < (int) localAddrs.size(); i ++)
    delete localAddrs[i];
  localAddrs.clear();
  for(int i = 0; i < (int) remoteAddrs.size(); i ++)
    delete remoteAddrs[i];
  remoteAddrs.clear();
  delete sendingBuffer;
  delete recvingBuffer;
}

//CHANGE

uint8_t
MpTcpSocketBase::GetMaxSubFlowNumber ()
{
	NS_LOG_FUNCTION_NOARGS ();
    return m_maxSubFlowNumber;
}

void
MpTcpSocketBase::SetMaxSubFlowNumber (uint8_t num)
{
	NS_LOG_FUNCTION_NOARGS ();
    m_maxSubFlowNumber = num;
}

uint8_t
MpTcpSocketBase::GetMinSubFlowNumber ()
{
	NS_LOG_FUNCTION_NOARGS ();
    return m_minSubFlowNumber;
}

void
MpTcpSocketBase::SetMinSubFlowNumber (uint8_t num)
{
	NS_LOG_FUNCTION_NOARGS ();
    m_minSubFlowNumber = num;
}

bool
MpTcpSocketBase::SetLossThreshold(uint8_t sFlowIdx, double lossThreshold)
{
	NS_LOG_FUNCTION_NOARGS ();
    if(sFlowIdx >= subflows.size())
        return false;
    subflows[sFlowIdx]->LostThreshold = lossThreshold;
    return true;
}

void
MpTcpSocketBase::SetPacketReorderAlgo (PacketReorder_t pralgo)
{
	NS_LOG_FUNCTION_NOARGS ();
    m_algoPR = pralgo;
}


Ptr<MpTcpSocketBase>
MpTcpSocketBase::Copy ()
{
	NS_LOG_FUNCTION_NOARGS ();
  return CopyObject<MpTcpSocketBase> (this);
}

void
MpTcpSocketBase::SetNode (Ptr<Node> node)
{
	NS_LOG_FUNCTION_NOARGS ();
    m_node = node;
}

Ptr<Node>
MpTcpSocketBase::GetNode ()
{
	NS_LOG_FUNCTION_NOARGS ();
    return m_node;
}

void
MpTcpSocketBase::SetMpTcp (Ptr<MpTcpL4Protocol> mptcp)
{
	NS_LOG_FUNCTION_NOARGS ();
    m_mptcp = mptcp;
}

uint32_t
MpTcpSocketBase::getL3MTU (Ipv4Address addr)
{
	NS_LOG_FUNCTION_NOARGS() ;

    // return the MTU associated to the layer 3
    Ptr<Ipv4L3Protocol> l3Protocol = m_node->GetObject<Ipv4L3Protocol> ();
    return l3Protocol->GetMtu ( l3Protocol->GetInterfaceForAddress (addr) )-100;
}

uint64_t
MpTcpSocketBase::getBandwidth (Ipv4Address addr)
{
	NS_LOG_FUNCTION_NOARGS ();
    uint64_t bd = 0;
    StringValue uiv;
    std::string name = std::string("DataRate");
    Ptr<Ipv4L3Protocol> l3Protocol = m_node->GetObject<Ipv4L3Protocol> ();
    Ptr<Ipv4Interface> ipv4If = l3Protocol->GetInterface (l3Protocol->GetInterfaceForAddress (addr));
    Ptr< NetDevice > netDevice = ipv4If->GetDevice();
    // if the device is a point to point one, then the data rate can be retrived directly from the device
    // if it's a CSMA one, then you should look at the corresponding channel
    if( netDevice->IsPointToPoint () == true )
    {
        netDevice->GetAttribute(name, (AttributeValue &) uiv);
        // converting the StringValue to a string, then deleting the 'bps' end
        std::string str = uiv.SerializeToString(0);
        std::istringstream iss( str.erase(str.size() - 3) );
        iss >> bd;
    }
    return bd;
}

//CHANGE



/** We initialize m_cWnd from this function, after attributes initialized */

//int
//MpTcpSocketBase::Listen (void)
//{
//  NS_LOG_FUNCTION (this);
//  InitializeCwnd ();
//  return TcpSocketBase::Listen ();
//}

int
MpTcpSocketBase::Listen (void)
{
	NS_LOG_FUNCTION_NOARGS ();
    //NS_LOG_FUNCTION_NOARGS();
    // detect all interfaces on which the node can receive a SYN packet

    MpTcpSubFlow *sFlow = new MpTcpSubFlow();
    sFlow->routeId = (subflows.size() == 0 ? 0:subflows[subflows.size() - 1]->routeId + 1);
    sFlow->sAddr   = m_endPoint->GetLocalAddress ();
    sFlow->sPort   = m_endPoint->GetLocalPort ();
    m_localPort    = m_endPoint->GetLocalPort ();
  //  sFlow->MSS     = getL3MTU (m_endPoint->GetLocalAddress ());
    sFlow->MSS = 536;
    sFlow->bandwidth = getBandwidth (m_endPoint->GetLocalAddress ());

    subflows.insert (subflows.end(), sFlow);
//std::cout<<"i am p "<<" routeId "<<sFlow->routeId<<" subflow.size: "<<subflows.size()<<std::endl;
    if(m_state != CLOSED)
    {
        m_errno = ERROR_INVAL;
        return -1;
    }
    // aT[CLOSED][APP_LISTEN] = SA(LISTEN, NO_ACT)
    // used as a reference when creating subsequent subflows
    m_state = LISTEN;

    ProcessAction(subflows.size() - 1 , ProcessEvent(subflows.size() - 1, APP_LISTEN));
    return 0;
}

/** We initialize m_cWnd from this function, after attributes initialized */
//int
//MpTcpSocketBase::Connect (const Address & address)
//{
//  NS_LOG_FUNCTION (this << address);
//  InitializeCwnd ();
//  return TcpSocketBase::Connect (address);
//}

int
MpTcpSocketBase::Connect (Ipv4Address servAddr, uint16_t servPort)
{
    NS_LOG_FUNCTION (m_node->GetId()<< this << servAddr << servPort );

    MpTcpSubFlow *sFlow = new MpTcpSubFlow();
    sFlow->routeId  = (subflows.size() == 0 ? 0:subflows[subflows.size() - 1]->routeId + 1);

    sFlow->dAddr    = servAddr;
    sFlow->dPort    = servPort;
    m_remoteAddress = servAddr;
    m_remotePort    = servPort;
    Ptr<Ipv4> ipv4  = m_node->GetObject<Ipv4> ();
//read filesize
   /*ifstream infile; 
    infile.open("/home/dawn/ns-3/ns-allinone-3.13/ns-3.13/src/mptcp/model/filesize.txt"); 

    std::string s;
    while(getline(infile,s))
    {
        cout<<s<<endl;
        filesize=(uint32_t)std::atoi(s.c_str());
        isCatch=true;
        //std::cout<<"filesize----"<<filesize<<std::endl;
        ifstream in("/home/dawn/ns-3/ns-allinone-3.13/ns-3.13/src/mptcp/model/filesize.txt"); //源文件读 
        ofstream out( "/home/dawn/ns-3/ns-allinone-3.13/ns-3.13/src/mptcp/model/filesize2.txt" ); //目标文件写
        int count=0;
        while(getline(in,s)){
           if(count==0){count++;continue;}
           out<<s<<endl;
        }
break;
    }
    infile.close();
ifstream in("/home/dawn/ns-3/ns-allinone-3.13/ns-3.13/src/mptcp/model/filesize2.txt"); //源文件读 
    ofstream out( "/home/dawn/ns-3/ns-allinone-3.13/ns-3.13/src/mptcp/model/filesize.txt" ); //目标文件写
        while(getline(in,s)){
           out<<s<<endl;
        }
*/


    if (m_endPoint == 0)
    {// no end point allocated for this socket => try to allocate a new one
        if (Bind() == -1)
        {
            NS_ASSERT (m_endPoint == 0);
            return -1;
        }
        NS_ASSERT (m_endPoint != 0);
    }
    // check if whether the node have a routing protocol
    sFlow->sAddr = m_endPoint->GetLocalAddress ();
    sFlow->sPort = m_endPoint->GetLocalPort ();
   // sFlow->MSS   = getL3MTU(m_endPoint->GetLocalAddress ());
       sFlow->MSS = 536;
    sFlow->bandwidth = getBandwidth(m_endPoint->GetLocalAddress ());
    subflows.insert(subflows.end(), sFlow);
//std::cout<<"i am x "<<" routeId "<<sFlow->routeId<<" subflow.size: "<<subflows.size()<<std::endl;
    if(ipv4->GetRoutingProtocol() == 0)
    {
        NS_FATAL_ERROR("No Ipv4RoutingProtocol in the node");
    }
    else
    {
        Ipv4Header header;
        header.SetDestination (servAddr);
        Socket::SocketErrno errno_;
        Ptr<Ipv4Route> route;
        Ptr<NetDevice> oif (0);
        //uint32_t oif = 0;
        route = ipv4->GetRoutingProtocol ()->RouteOutput(Ptr<Packet> (), header, oif, errno_);
        if (route != 0)
        {
            NS_LOG_LOGIC("Route existe");
            m_endPoint->SetLocalAddress (route->GetSource ());
        }
        else
        {
            NS_LOG_LOGIC ("MpTcpSocketBase"<<m_node->GetId()<<"::Connect():  Route to " << m_remoteAddress << " does not exist");
            NS_LOG_ERROR (errno_);
            m_errno = errno_;
            return -1;
        }
    }
    // Sending SYN packet
    bool success = ProcessAction (subflows.size() - 1, ProcessEvent (subflows.size() - 1,APP_CONNECT) );
    if ( !success )
    {
        return -1;
    }
  return 0;
}


int
MpTcpSocketBase::Connect (Address &address)
{
    NS_LOG_FUNCTION ( this << address );

    // convert the address (Address => InetSocketAddress)
    InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
    m_remoteAddress = transport.GetIpv4();
    m_remotePort    = transport.GetPort();
    return Connect(m_remoteAddress, m_remotePort);
}


//Fast recovery and fast retransmit
void
MpTcpSocketBase::DupAck (const TcpHeader& t, uint32_t count)
{
//std::cout<<"DupAck (const TcpHeader& t, uint32_t count)"<<std::endl;
  NS_LOG_FUNCTION (this << "t " << count);
 if (count == m_retxThresh && !m_inFastRec)
    { // triple duplicate ack triggers fast retransmit (RFC2581, sec.3.2)
      m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
      m_cWnd = m_ssThresh + 3 * m_segmentSize;
      m_inFastRec = true;
      NS_LOG_INFO ("Triple dupack. Reset cwnd to " << m_cWnd << ", ssthresh to " << m_ssThresh);
      DoRetransmit ();
    }
  else if (m_inFastRec)
    { // In fast recovery, inc cwnd for every additional dupack (RFC2581, sec.3.2)
      m_cWnd += m_segmentSize;
      NS_LOG_INFO ("Increased cwnd to " << m_cWnd);
      SendPendingData ();
    };
}


//Change

int
MpTcpSocketBase::Bind ()
{
    //Allocate an endPoint for this socket
    NS_LOG_FUNCTION_NOARGS ();
    client = true;
    m_endPoint = m_mptcp->Allocate();
    return Binding();
}


int
MpTcpSocketBase::Binding (void)
{
    NS_LOG_FUNCTION_NOARGS ();
    if (m_endPoint == 0)
    {
        return -1;
    }
    // set the call backs method


    m_endPoint->SetRxCallback (MakeCallback (&MpTcpSocketBase::ForwardUp, Ptr<MpTcpSocketBase>(this)));
    m_endPoint->SetDestroyCallback (MakeCallback (&MpTcpSocketBase::Destroy, Ptr<MpTcpSocketBase>(this)));

    // set the local parameters
    m_localAddress = m_endPoint->GetLocalAddress();
    m_localPort    = m_endPoint->GetLocalPort();
    return 0;
}

int
MpTcpSocketBase::Bind (const Address &address)
{
  NS_LOG_FUNCTION (m_node->GetId()<<":"<<this<<address);
  server = true;
  if (!InetSocketAddress::IsMatchingType (address))
    {
      m_errno = ERROR_INVAL;
      return -1;
    }
  else
      NS_LOG_DEBUG("MpTcpSocketBase:Bind: Address ( " << address << " ) is valid");
  InetSocketAddress transport = InetSocketAddress::ConvertFrom (address);
  Ipv4Address ipv4 = transport.GetIpv4 ();
  uint16_t port = transport.GetPort ();
  NS_LOG_LOGIC("MpTcpSocketBase:Bind: Ipv4Address: "<< ipv4 << ", Port: " << port);

  if (ipv4 == Ipv4Address::GetAny () && port == 0)
    {
      m_endPoint = m_mptcp->Allocate ();
      NS_LOG_LOGIC ("MpTcpSocketBase "<<this<<" got an endpoint: "<<m_endPoint);
    }
  else if (ipv4 == Ipv4Address::GetAny () && port != 0)
    {
      m_endPoint = m_mptcp->Allocate (port);
      NS_LOG_LOGIC ("MpTcpSocketBase "<<this<<" got an endpoint: "<<m_endPoint);
    }
  else if (ipv4 != Ipv4Address::GetAny () && port == 0)
    {
      m_endPoint = m_mptcp->Allocate (ipv4);
      NS_LOG_LOGIC ("MpTcpSocketBase "<<this<<" got an endpoint: "<<m_endPoint);
    }
  else if (ipv4 != Ipv4Address::GetAny () && port != 0)
    {
      m_endPoint = m_mptcp->Allocate (ipv4, port);
      NS_LOG_LOGIC ("MpTcpSocketBase "<<this<<" got an endpoint: "<<m_endPoint);
    }else
        NS_LOG_DEBUG("MpTcpSocketBase:Bind(@): unable to allocate an end point !");

  return Binding ();
}

bool
MpTcpSocketBase::SendBufferedData ()
{
  NS_LOG_FUNCTION("SendBufferdData"<< Simulator::Now ().GetSeconds () );
  uint8_t sFlowIdx = m_lastUsedsFlowIdx; // i prefer getSubflowToUse (), but this one gives the next one
  Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject<Ipv4L3Protocol> ();

  if ( !ProcessAction (sFlowIdx, ProcessEvent (sFlowIdx, APP_SEND) ))
  {
      return false; // Failed, return zero
  }
  return true;
}

int
MpTcpSocketBase::FillBuffer (uint8_t* buf, uint32_t size)
{
  NS_LOG_FUNCTION( this << size );
  return sendingBuffer->Add(buf, size);
}

//LISA algo
void
MpTcpSocketBase::lisaCwnd(uint32_t sFlowIdx){
   MpTcpSubFlow *sFlow; 
   sFlow = subflows[sFlowIdx];
   if(subflows.size()!=1){
                    uint32_t k;
                    uint32_t oldCwnd=0;
                    for(uint32_t i =0;i<subflows.size();i++){
                        if(oldCwnd < subflows[i]->cwnd) {oldCwnd=subflows[i]->cwnd;k = i;}
                        
                    }                  
std::cout<<"oldCwnd: "<<oldCwnd<<" subflows["<<k<<"].cwnd: "<<subflows[k]->cwnd<<" size: "<<subflows.size()<<std::endl;
                    if(oldCwnd>=2){
                      sFlow->cwnd=1;subflows[k]->cwnd=subflows[k]->cwnd-1;
std::cout<<"1subflows["<<(int)sFlowIdx<<"].cwnd: "<<sFlow->cwnd<<" subflows["<<k<<"].cwnd: "<<subflows[k]->cwnd<<std::endl;
                    }else{
                      sFlow->cwnd=1;
std::cout<<"2subflows["<<(int)sFlowIdx<<"].cwnd: "<<sFlow->cwnd<<" subflows["<<k<<"].cwnd: "<<subflows[k]->cwnd<<std::endl;
                    }
                }
}

bool
MpTcpSocketBase::SendPendingData ()
{
    NS_LOG_INFO("====================================================================================");
  NS_LOG_FUNCTION_NOARGS ();
  uint32_t nOctetsSent = 0;
  MpTcpSubFlow *sFlow;
  while ( ! sendingBuffer->Empty() )
  {
        uint8_t count   = 0;
        uint32_t window = 0;
        while ( count < subflows.size() )
        {
            count ++;
            window = std::min (AvailableWindow (m_lastUsedsFlowIdx), sendingBuffer->PendingData()); // Get available window size
            if( window == 0 )
            {
                // No more available window in the current subflow, try with another one
//std::cout<<(int)m_lastUsedsFlowIdx<<"已经满了《》《》《》《》《》"<<std::endl;
                m_lastUsedsFlowIdx = getSubflowToUse ();
/*std::cout<<"m_lastUsedsFlowIdx----> "<<(int)m_lastUsedsFlowIdx<<"level======"<<sfp[m_lastUsedsFlowIdx].level<<std::endl;*/
            }
            else
            {
                NS_LOG_LOGIC ("MpTcpSocketBase::SendPendingData -> PendingData (" << sendingBuffer->PendingData() << ") Available window ("<<AvailableWindow (m_lastUsedsFlowIdx)<<")");
                break;
            }
        }
        if ( count == subflows.size() && window == 0 )
        {
            // No available window for transmission in all subflows, abort sending
            break;
        }
      if(firstUsed[m_lastUsedsFlowIdx]==0){
          lisaCwnd(m_lastUsedsFlowIdx);
          firstUsed[m_lastUsedsFlowIdx]++;
      }
      sFlow = subflows[m_lastUsedsFlowIdx];
//std::cout<<"正在使用的子流ID: "<<(int)m_lastUsedsFlowIdx<<" subflows.size(): "<<subflows.size()<<std::endl;
//std::cout<<"flow"<<sFlow->dPort-5000<<" sFlowId:"<<(int)m_lastUsedsFlowIdx<<" saddr: "<<sFlow->sAddr<<" daddr: "<<sFlow->dAddr<<" rtt:"<<sFlow->rtt->GetCurrentEstimate().GetSeconds()<<" level: "<<sfp[m_lastUsedsFlowIdx].level<<" ratio: "<<sfp[m_lastUsedsFlowIdx].ratio<<" totalsize: "<<totalsize<<std::endl;
      if(sFlow->state == ESTABLISHED)
      {
        Ipv4Address sAddr   = sFlow->sAddr;
        Ipv4Address dAddr   = sFlow->dAddr;
        uint16_t sPort      = sFlow->sPort;
        uint16_t dPort      = sFlow->dPort;
        uint32_t mss        = sFlow->MSS;
        uint8_t hlen = 5;
        uint8_t olen = 15 ;
        uint8_t plen = 0;

        uint32_t size = std::min (window, mss);  // Send no more than window
//std::cout<<"AvailableWindow: "<<AvailableWindow (m_lastUsedsFlowIdx)<<" cwnd: "<<sFlow->cwnd<<std::endl;
        Ptr<Packet> pkt = sendingBuffer->CreatePacket(size);
        if(pkt == 0)
            break;
        filesize-=size;
//std::cout<<"4 flow: "<<sFlow->dPort-5000<<" size: "<<filesize<<std::endl;
        fs[m_lastUsedsFlowIdx]+=size;
        filesize2-=size;
        filesize3-=size;
        filesize4-=size;
        filesize5-=size;
        filesize6-=size;
        filesize7-=size;
        filesize8-=size;
        filesize9-=size;
        filesize10-=size;
        filesize11-=size;

         if(sFlow->phase==Slow_Start ){
                ss_ss+=size;                
                std::cout<<"has been send data: "<<ss_ss<<std::endl;
        }

        totalsize+=size;
//std::cout<<"dataNum: "<<(int)sFlow->dPort-5000+1<<"  filesize----"<<totalsize<<std::endl;
        NS_LOG_LOGIC ("MpTcpSocketBase SendPendingData on subflow " << (int)m_lastUsedsFlowIdx << " w " << window << " rxwin " << AdvertisedWindowSize () << " CWND "  << sFlow->cwnd << " segsize " << sFlow->MSS << " nextTxSeq " << nextTxSequence << " nextRxSeq " << nextRxSequence << " pktSize " << size);
        uint8_t  flags   = TcpHeader::ACK;

        MpTcpHeader header;
        header.SetSourcePort      (sPort);
        header.SetDestinationPort (dPort);
        header.SetFlags           (flags);
        header.SetSequenceNumber  (SequenceNumber32(sFlow->TxSeqNumber));
      //header.SetSequenceNumber  (sFlow->TxSeqNumber);
        uint32_t temp=sFlow->RxSeqNumber;
        header.SetAckNumber       (SequenceNumber32(temp));
        header.SetWindowSize      (AdvertisedWindowSize());
      // save the seq number of the sent data
        sFlow->AddDSNMapping      (m_lastUsedsFlowIdx, nextTxSequence, size, sFlow->TxSeqNumber, sFlow->RxSeqNumber, pkt->Copy() );
        header.AddOptDSN          (OPT_DSN, nextTxSequence, size, sFlow->TxSeqNumber);

        switch ( m_algoPR )
        {
            case Eifel:
                header.AddOptTT   (OPT_TT, Simulator::Now ().GetMilliSeconds (), 0);
                olen += 17;
                break;
            default:
                break;
        }

        plen = (4 - (olen % 4)) % 4;
        olen = (olen + plen) / 4;
        hlen += olen;
        header.SetLength(hlen);
        header.SetOptionsLength(olen);
        header.SetPaddingLength(plen);

        SetReTxTimeout (m_lastUsedsFlowIdx);

        // simulating loss of acknowledgement in the sender side
        calculateTotalCWND ();


          if( sFlow->LostThreshold > 0.0 && sFlow->LostThreshold < 1.0 )
          {
              //Ptr<RateErrorModel> eModel = CreateObjectWithAttributes<RateErrorModel> ("RanVar", RandomVariableValue (UniformVariable (0., 1.)), "ErrorRate", DoubleValue (sFlow->LostThreshold));
              //if ( ! eModel->IsCorrupt (pkt) )
              if ( rejectPacket(sFlow->LostThreshold) == false )
              {
                 m_mptcp->SendPacket (pkt, header, sAddr, dAddr);
              }else
              {
                  NS_LOG_WARN("sFlowIdx "<<(int) m_lastUsedsFlowIdx<<" -> Packet Droped !");
              }
          }else
          {
              m_mptcp->SendPacket (pkt, header, sAddr, dAddr);
          }

        NS_LOG_WARN (Simulator::Now().GetSeconds() <<" SentSegment -> "<< " m_localToken "<< m_localToken<<" Subflow "<<(int) m_lastUsedsFlowIdx<<" DataTxSeq "<<nextTxSequence<<" SubflowTxSeq "<<sFlow->TxSeqNumber<<" SubflowRxSeq "<< sFlow->RxSeqNumber <<" Data "<< size <<" unAcked Data " << unAckedDataCount <<" octets" );

        // Notify the application of the data being sent
        sFlow->rtt->SentSeq (SequenceNumber32(sFlow->TxSeqNumber), size);           // notify the RTT
        nOctetsSent        += size;                               // Count sent this loop
        nextTxSequence     += size;                // Advance next tx sequence
        sFlow->TxSeqNumber += size;
        sFlow->maxSeqNb    += size;
        unAckedDataCount   += size;
      }
      m_lastUsedsFlowIdx = getSubflowToUse ();
  }
  NS_LOG_LOGIC ("RETURN SendPendingData -> amount data sent = " << nOctetsSent);
  NotifyDataSent( GetTxAvailable() );

  return ( nOctetsSent>0 );
}
/*void printMes(int signo)   
{   
std::cout<<"i am timer>>>>>>>>"<<std::endl;   
}
int 
MpTcpSocketBase::printCwnd(){
    struct itimerval tick;
    
    signal(SIGALRM, printMes);
    memset(&tick, 0, sizeof(tick));

    //Timeout to run first time
    tick.it_value.tv_sec = 1;
    tick.it_value.tv_usec = 0;

    //After first, the Interval time for clock
    tick.it_interval.tv_sec = 1;
    tick.it_interval.tv_usec = 0;

    if(setitimer(ITIMER_REAL, &tick, NULL) < 0)
            printf("Set timer failed!\n");

    //When get a SIGALRM, the main process will enter another loop for pause()
    while(1)
    {
        pause();
    }
    return 0;
}*/
void
MpTcpSocketBase::setDiscreteRatio(uint8_t discreteRatio){
        dr = discreteRatio;
}
void 
MpTcpSocketBase::setFileSize(uint32_t size){
        filesize=size;
}
double 
MpTcpSocketBase::getFileSize(){
        return filesize;
}
void 
MpTcpSocketBase::InitialRtt()
{
   for(uint32_t i = 0;i<subflows.size();i++){
       sfp[i].sFlowIdx =(uint8_t)i;
       sfp[i].level = i;
        switch(i){
           case 0:sfp[i].rtt=0.01;
                  myRtt[i]=0.01;break;
           case 1:sfp[i].rtt=0.02;
                  myRtt[i]=0.02;break;
           case 2:sfp[i].rtt=0.03;
                  myRtt[i]=0.03;break;
           case 3:sfp[i].rtt=0.04;
                  myRtt[i]=0.04;break;
           case 4:sfp[i].rtt=0.06;
                  myRtt[i]=0.06;break;
           case 5:sfp[i].rtt=0.08;
                  myRtt[i]=0.08;break;
           case 6:sfp[i].rtt=0.09;
                  myRtt[i]=0.09;break;
           case 7:sfp[i].rtt=0.1;
                  myRtt[i]=0.1;break;
        }
        
   }
}
void 
MpTcpSocketBase::SortRtt()
{
   for(uint32_t i = 0;i<subflows.size();i++){
    MpTcpSubFlow *sFlowtmp=subflows[i];
    double time = sFlowtmp->rtt->GetCurrentEstimate().GetSeconds();
    sfp[i].sFlowIdx =(uint8_t)i;
    sfp[i].level = i;
    if(time==0){
        switch(i){
           case 0:sfp[i].rtt=0.01;
                  myRtt[i]=0.01;break;
           case 1:sfp[i].rtt=0.02;
                  myRtt[i]=0.02;break;
           case 2:sfp[i].rtt=0.03;
                  myRtt[i]=0.03;break;
           case 3:sfp[i].rtt=0.04;
                  myRtt[i]=0.04;break;
           case 4:sfp[i].rtt=0.06;
                  myRtt[i]=0.06;break;
           case 5:sfp[i].rtt=0.08;
                  myRtt[i]=0.08;break;
           case 6:sfp[i].rtt=0.09;
                  myRtt[i]=0.09;break;
           case 7:sfp[i].rtt=0.1;
                  myRtt[i]=0.1;break;
        }
        
    }else{
        sfp[i].rtt=time;
        myRtt[i]=time;
    }
    
  }
 for(uint32_t i = 0;i<subflows.size();i++){
     
   for (uint32_t j = i ; j <subflows.size(); j++){
     if (myRtt[i] > myRtt[j]) {
       double tmp1 = myRtt[i];
       myRtt[i]=myRtt[j];
       myRtt[j]=tmp1;
      }
   }
  }
for(uint32_t i=0;i<subflows.size();i++){
    //std::cout<<"myrtt["<<i<<"]>>>"<<myRtt[i]<<std::endl;
}
  for (uint32_t i=0; i <subflows.size(); i++){
    for(uint32_t j=0; j <subflows.size(); j++){
      if(myRtt[j]==sfp[i].rtt){
         sfp[i].level=j;
         myRtt[j]=-1;
         break;
      }
    }
  } 
/*for(uint32_t i=0;i<subflows.size();i++){
        std::cout<<"subflowsize="<<subflows.size()<<std::endl;
        std::cout<<"sfp["<<i<<"].id="<<sfp[i].sFlowIdx<<std::endl;
        std::cout<<"sfp["<<i<<"].rtt="<<sfp[i].rtt<<std::endl;
        std::cout<<"sfp["<<i<<"].level="<<sfp[i].level<<std::endl;
}*/
for(uint32_t i=subflows.size();i<10;i++){
        sfp[i].level=-1;
}
}

double
MpTcpSocketBase::fac(uint32_t x){
    double f;
    if(x==0 || x==1)
      f=1;
    else
      f=fac(x-1)*x;
    return f;
}
double
MpTcpSocketBase::ThroughPut(uint32_t i)
{
   int r[10];          //r[]数组存放第j条子流可f以传输的轮数
   double tp;          //存放前i-1条子流的吞吐量
   double p = 0.0001;
   double p0=1;
   int flag=1;         //判断数据传输在第几轮
  
   uint32_t z=0;   
   for(uint32_t k=0;k<subflows.size();k++)
          if(sfp[k].level==(int)i-1){
             z=k;
          }   
   for(uint32_t j=0;j<i;j++)
       for(uint32_t k=0;k<subflows.size();k++){
           if(sfp[k].level==(int)j){
             r[j]=sfp[z].rtt/sfp[k].rtt;
//std::cout<<"subflowsize: "<<subflows.size()<<" sfp["<<z<<"].rtt: "<<sfp[z].rtt<<" sfp["<<k<<"].rtt: "<<sfp[k].rtt<<" r["<<j<<"]: "<<r[j]<<std::endl;
             if(r[j]>50)r[j]=50;
          }
       }
   totalData = 0; 
   for(uint32_t j=0;j<i;j++){
   //吞吐量=n轮传输的数据/n*rtt
      for(uint32_t k=0;k<subflows.size();k++){
          if(sfp[k].level==(int)j){
//std::cout<<"sfp[z].rtt: "<<sfp[z].rtt<<" sfp[k].rtt: "<<sfp[k].rtt<<std::endl;
       tp=tp+TotalData(r[j],sfp[k].sFlowIdx,subflows[sfp[k].sFlowIdx]->cwnd,subflows[sfp[k].sFlowIdx]->ssthresh,p,p0,flag);
//std::cout<<"tp["<<j<<"]: "<<tp<<std::endl;
          }
      }
     
   }
   return tp;
}
void
MpTcpSocketBase::my_calculate_alpha ()
{
    // this method is called whenever a congestion happen in order to regulate the agressivety of subflows
   NS_LOG_FUNCTION_NOARGS ();
   meanTotalCwnd = totalCwnd = alpha = 0;
   double maxi       = 0;
   double sumi       = 0;
   for (uint32_t i = 0; i < subflows.size() ; i++)
   {
       if(sfp[i].level<maxLevel){
        //std::cout<<"useId2{"<<i<<"}: "<<(int)sfp[i].sFlowIdx<<std::endl;
        MpTcpSubFlow * sFlow = subflows[(int)sfp[i].sFlowIdx];
       
       totalCwnd += sFlow->cwnd;
       meanTotalCwnd += sFlow->scwnd;

     Time time = sFlow->rtt->GetCurrentEstimate ();
     double rtt = time.GetSeconds ();
     if (rtt < 0.000001)
       continue;                 // too small

     double tmpi = sFlow->scwnd / (rtt * rtt);
     if (maxi < tmpi)
       maxi = tmpi;

     sumi += sFlow->scwnd / rtt;
     }
   }

   if (!sumi)
     return;
   alpha = meanTotalCwnd * maxi / (sumi * sumi);
   NS_LOG_ERROR ("calculate_alpha: alpha "<<alpha<<" totalCwnd ("<< meanTotalCwnd<<")");
       
       
}

void 
MpTcpSocketBase::ModelTest0Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(2,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop4(){
   std::cout<<"modelThoughPut: "<<TotalData(4,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop5(){
   std::cout<<"modelThoughPut: "<<TotalData(5,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop6(){
   std::cout<<"modelThoughPut: "<<TotalData(6,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop7(){
   std::cout<<"modelThoughPut: "<<TotalData(7,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop8(){
   std::cout<<"modelThoughPut: "<<TotalData(8,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop9(){
   std::cout<<"modelThoughPut: "<<TotalData(9,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop10(){
   std::cout<<"modelThoughPut: "<<TotalData(10,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop12(){
   std::cout<<"modelThoughPut: "<<TotalData(12,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop14(){
   std::cout<<"modelThoughPut: "<<TotalData(14,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop18(){
   std::cout<<"modelThoughPut: "<<TotalData(18,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop20(){
   std::cout<<"modelThoughPut: "<<TotalData(20,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest0Drop21(){
   std::cout<<"modelThoughPut: "<<TotalData(21,0,1,65535,0,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest001Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(3,0,1,65535,0.001,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest002Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(3,0,1,65535,0.001,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest004Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(4,0,1,65535,0.001,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest005Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(5,0,1,65535,0.001,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest010Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(10,0,1,65535,0.001,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest003Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(100,0,1,65535,0.03,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest008Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(100,0,1,65535,0.08,1,1)<<std::endl;
}
void 
MpTcpSocketBase::ModelTest01Drop(){
   std::cout<<"modelThoughPut: "<<TotalData(100,0,1,65535,0.1,1,1)<<std::endl;
}


double  
MpTcpSocketBase::TotalData
(int n,uint32_t sFlowIdx,double cwnd,uint32_t sst,double p,double p0,int flag)
{
double increment=0;
if(n-1==0)return 0;
if(n==1 && flag == 1)return cwnd*1400;
else{
   if(flag==1){
     p0=1;
     totalData=cwnd*1400;
//std::cout<<"totalSize["<<n<<"]： "<<totalData<<" throughPut["<<n<<"]: "<<totalData/0.02<<std::endl;
   }
   
   flag++; 
   double cwnd1;
   double cwnd2;
   double cwnd3;
   uint32_t sst1;
   uint32_t sst2;
   uint32_t sst3;
   double p1,p2 ,p3;         //传输w个包丢失r个的概率
   if(cwnd<4){                //如果cwnd<4，发生了丢包将一定进入超时重传
     if(cwnd*1400<(double)sst)
       cwnd1 = (cwnd)*2;         //慢启动情况
     else {
        if(sFlowChangeFlag==0){
           cwnd1 = cwnd+1;
        }else{
            calculateSmoothedCWND (sFlowIdx);
            my_calculate_alpha();
            increment = std::min( alpha / totalCwnd, 1.0 / cwnd );
//std::cout<<"increment-------"<<increment<<std::endl;
            cwnd1 = cwnd+increment;        //拥塞避免情况
        }
       
     }
       
     cwnd2 = 1;              //超时重传情
     sst1 = sst;          
     sst2 = cwnd*1400/2;
     p1 = p0*pow((1-p),cwnd);
     p2 = p0*(1-pow((1-p),cwnd));
     totalData = totalData+(p1*cwnd1+p2*cwnd2)*1400;
     TotalData(n-1,sFlowIdx,cwnd1,sst1,p,p1,flag);  //递归
     TotalData(n-1,sFlowIdx,cwnd2,sst2,p,p2,flag);
   }else{          
     if(cwnd*1400<(double)sst)
       cwnd1 = (cwnd)*2;         //慢启动情况
     else {
        if(sFlowChangeFlag==0){
           cwnd1 = cwnd+1;
        }else{
            calculateSmoothedCWND (sFlowIdx);
            my_calculate_alpha();
            increment = std::min( alpha / totalCwnd, 1.0 / cwnd );
            cwnd1 = cwnd+increment;        //拥塞避免情况
        }
     }
     cwnd2 = 1;             //超时重传情况
     cwnd3 = cwnd/2;        //快速重传情况      
     sst1 = sst;
     sst2 = cwnd*1400/2;
     sst3 = cwnd*1400/2;     
     uint32_t cwnd0 = floor(cwnd) ;      //取小于等于cwnd的整数
     p1 = p0*pow((1-p),cwnd);///(fac(cwnd0-3) * fac(3))
//std::cout<<"cwnd0======"<<cwnd0<<std::endl;
     p2 = p0*(fac(cwnd0)/(fac(cwnd0-3) * fac(3)))*pow(p,cwnd0-3)*pow(1-p,3);             
     p3 = p0*(1-pow(1-p,cwnd)-(fac(cwnd0)/(fac(cwnd0-3) * fac(3)))*pow(p,cwnd0-3)*pow(1-p,3));
//p0*(1-pow(1-p,cwnd)-(fac(cwnd0)/(fac(cwnd0-3) * fac(3)))*pow(p,cwnd0-3)*pow(1-p,3));
     totalData = totalData+(p1*cwnd1+p2*cwnd2+p3*cwnd3)*1400;
     ts[n-1]+=(p1*cwnd1+p2*cwnd2+p3*cwnd3)*1400;
     //std::cout<<"totalSize["<<n-1<<"]： "<<ts[n-1]<<std::endl;
     TotalData(n-1,sFlowIdx,cwnd1,sst1,p,p1,flag);  //递归
     TotalData(n-1,sFlowIdx,cwnd2,sst2,p,p2,flag);
     TotalData(n-1,sFlowIdx,cwnd3,sst3,p,p3,flag);
   }

   return totalData;}
}
/*
uint8_t 
MpTcpSocketBase::getUseId(double filesize)
{
     uint32_t n;
     uint8_t nextSubFlow = 0;
            SortRtt();
            for(uint32_t i=0;i<subflows.size();i++){
              double tp = ThroughPut(i);
              //std::cout<<"filesize:"<<filesize<<"throughput::"<<tp<<std::endl;                
              if(filesize<tp){
                  //std::cout<<"filesize:-->"<<filesize<<std::endl;
                  n=i+1;
                  updateFlag=false;
                  break;
              }
                n=subflows.size();
            }
            //std::cout<<"n---->"<<n<<std::endl;

            level=(level+1)%n;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
return nextSubFlow;
}
*/
uint8_t
MpTcpSocketBase::getSubflowToUse ()
{
	NS_LOG_FUNCTION_NOARGS ();
    uint8_t nextSubFlow = 0;
    switch ( m_distribAlgo )
    {
        case Round_Robin :
            nextSubFlow = (m_lastUsedsFlowIdx + 1) % subflows.size();
            break;
        case myAlgo2:
        //if(isCatch==false){
           /*if(totalsize<100000){
            if(updateFlag==true){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
std::cout<<"tp["<<i<<"]:"<<tp<<" filesize2: "<<filesize2<<std::endl;
                   if(filesize2<tp){
                      maxLevel=i+1;
                      updateFlag=false;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }

             }
           std::cout<<"maxLevel1---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
            
           }else if(totalsize>100000 and totalsize<200000){
               if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
     std::cout<<"tp["<<i<<"]:"<<tp<<" filesize3: "<<filesize3<<std::endl;
                   if(filesize3<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel2---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else if(totalsize>200000 and totalsize<300000){
               if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
     std::cout<<"tp["<<i<<"]:"<<tp<<" filesize4: "<<filesize4<<std::endl;
                   if(filesize4<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel3---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else if(totalsize>300000 and totalsize<400000){
               if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
     std::cout<<"tp["<<i<<"]:"<<tp<<" filesize5: "<<filesize5<<std::endl;
                   if(filesize5<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel4---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else if(totalsize>400000 and totalsize<500000){
               if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
     std::cout<<"tp["<<i<<"]:"<<tp<<" filesize6: "<<filesize6<<std::endl;
                   if(filesize6<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel5---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else if(totalsize>500000 and totalsize<600000){
               if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
     std::cout<<"tp["<<i<<"]:"<<tp<<" filesize7: "<<filesize7<<std::endl;
                   if(filesize7<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel6---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else if(totalsize>600000 and totalsize<700000){
               if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
     std::cout<<"tp["<<i<<"]:"<<tp<<" filesize8: "<<filesize8<<std::endl;
                   if(filesize8<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel7---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else if(totalsize>700000 and totalsize<800000){
              if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
std::cout<<"tp["<<i<<"]:"<<tp<<" filesize9: "<<filesize9<<std::endl;
                   if(filesize9<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel8---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else if(totalsize>800000 and totalsize<900000){
              if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
std::cout<<"tp["<<i<<"]:"<<tp<<" filesize10: "<<filesize10<<std::endl;
                   if(filesize10<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel9---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else if(totalsize>900000 and totalsize<1000000){
              if(updateFlag==false){
                InitialRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   double tp = ThroughPut(i);
std::cout<<"tp["<<i<<"]:"<<tp<<" filesize11: "<<filesize11<<std::endl;
                   if(filesize11<tp){
                      maxLevel=i+1;
                      updateFlag=true;
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }
            //std::cout<<"n---->"<<n<<std::endl;

             }
           std::cout<<"maxLevel10---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
           }else{
               nextSubFlow = (m_lastUsedsFlowIdx + 1) % subflows.size();
           }
        }else{*/
            if(updateFlag==true){
                InitialRtt();
                for(uint32_t i=1;i<=subflows.size();i++){
                   double tp = ThroughPut(i);
                  //std::cout<<"---filesize---:"<<filesize<<"throughput::"<<tp<<std::endl; 
                   if(filesize<tp){
                  //std::cout<<"filesize:"<<filesize<<"throughput["<<i<<"]: "<<tp<<std::endl; 
                      maxLevel=i;
                      updateFlag=false; 
                      break;
                    }
                 maxLevel=subflows.size();
                 if(maxLevel==8) updateFlag=false;
                 }

             }
           std::cout<<"maxLevel12---->"<<maxLevel<<std::endl;
            level=(level+1)%maxLevel;
                for(int i = 0 ;i<8;i++){
                   if(sfp[i].level==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }
        //}
          break;
        case myAlgo:
                if(totalsize<100000){
                /*SortRtt();
                for(uint32_t i=0;i<subflows.size();i++){
                   if(sfp[i].level==0){
                        m_lastUsedsFlowIdx=sfp[i].sFlowIdx;
                        nextSubFlow=m_lastUsedsFlowIdx % subflows.size();
                   }
                }*/
                nextSubFlow=0;
             }else if(totalsize>100000 and totalsize<400000){
                if(updateFlag==true){
                SortRtt();                
                int z;                
                for(uint8_t i=0;i<subflows.size();i++){
                   if(sfp[i].level==0){
                      z=(int)i;
                   }
                }
                for(uint32_t i=0;i<subflows.size();i++){
                   sfp[i].ratio=sfp[i].rtt/sfp[z].rtt;
                }
                for(uint32_t i=0;i<subflows.size();i++){
                  if((uint8_t)sfp[i].ratio<dr)  
                     usesFlowLevel[i]=sfp[i].level;
                  else 
                     usesFlowLevel[i]=-1;
                }
                for(int i=subflows.size();i<8;i++){usesFlowLevel[i]=-1;}
                for(int i=0;i<8;i++){
                    if(maxLevel<usesFlowLevel[i]){maxLevel=usesFlowLevel[i];}
                }
//std::cout<<"maxLevel: "<<maxLevel<<std::endl;
                }                
                updateFlag=false;

                level=(level+1)%(maxLevel+1);
                for(int i = 0 ;i<8;i++){
                   if(usesFlowLevel[i]==level){
                       m_lastUsedsFlowIdx=(uint8_t)sfp[i].sFlowIdx;    
                       nextSubFlow=m_lastUsedsFlowIdx % subflows.size();                              
                   }
                }

             }else{
                nextSubFlow = (m_lastUsedsFlowIdx + 1) % subflows.size();
             }        
            
        break;
        default:
            break;
    }
    return nextSubFlow;
}

void
MpTcpSocketBase::ReTxTimeout (uint8_t sFlowIdx)
{ // Retransmit timeout
  //NS_LOG_INFO("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++");
  NS_LOG_FUNCTION (this);
  MpTcpSubFlow *sFlow = subflows[sFlowIdx];
  NS_LOG_WARN ("Subflow ("<<(int)sFlowIdx<<") ReTxTimeout Expired at time "<<Simulator::Now ().GetSeconds()<< " unacked packets count is "<<sFlow->mapDSN.size() );

  // If erroneous timeout in closed/timed-wait state, just return
  if (m_state == CLOSED || m_state == TIME_WAIT)
  {
      NS_LOG_WARN ("ReTxTimeout subflow ("<<(int)sFlowIdx<<") current state is "<<m_stateMachine->printState(m_state));
      return;
  }
  reduceCWND (sFlowIdx);
  // Set cWnd to segSize on timeout,  per rfc2581
  // Collapse congestion window (re-enter slowstart)
  /*if(int(sFlow->dPort)-5000==12 and sFlowIdx==0){
        timeOut[12]++;
std::cout<<"flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<"cwnd: "<<sFlow->cwnd<<"  timeOut: "<<timeOut[12]<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
      }*/
for(uint8_t i=0;i<subflows.size();i++){
     if(sFlowIdx==i){
        timeOut[i]++;
        std::cout<<"2 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<"   sFlow->ssthresh:   "<<sFlow->ssthresh<<" cwnd: "<<sFlow->cwnd<<"  timeOut: "<<timeOut[i]<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
     }
}
/*for(uint8_t i=0;i<subflows.size();i++){
     if(sFlowIdx==i){
        timeOut[i]++;
        std::cout<<" sFlowIdx: "<<(int)sFlowIdx<<" timeOut: "<<timeOut[i]<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
     }
}*/
  Retransmit (sFlowIdx);             // Retransmit the packet
  if( m_algoPR == F_RTO )
  {
      sFlow->SpuriousRecovery = false;
      if( (sFlow->phase == RTO_Recovery) && (sFlow->recover >= sFlow->highestAck + 1) )
      {
          sFlow->recover  = sFlow->TxSeqNumber; // highest sequence number transmitted
          sFlow->ackCount = 0;
          frtoStep = Step_4;    // go to step 4 to perform the standard Fast Recovery algorithm
      }else
      {
          frtoStep = Step_2;    // enter step 2 of the F-RTO algorithm
          NS_LOG_WARN("Entering step 2 of the F-RTO algorithm");
      }
      sFlow->phase = RTO_Recovery; // in RTO recovery algorithm, sender do slow start retransmissions
  }
}

void
MpTcpSocketBase::reduceCWND (uint8_t sFlowIdx)
{
	NS_LOG_FUNCTION_NOARGS ();
    MpTcpSubFlow *sFlow = subflows[sFlowIdx];
    double cwnd = sFlow->cwnd;
    calculateTotalCWND ();

    // save current congestion state
    switch ( m_algoPR )
    {
        case D_SACK:
            sFlow->savedCWND = std::max (cwnd, sFlow->savedCWND);
            sFlow->savedSSThresh = std::max(sFlow->ssthresh, sFlow->savedSSThresh);
            break;
        default:
            sFlow->savedCWND = cwnd;
            sFlow->savedSSThresh = sFlow->ssthresh;
            break;
    }


    sFlow->ssthresh = (std::min(remoteRecvWnd, static_cast<uint32_t>(sFlow->cwnd)*sFlow->MSS)) / 2; // Per RFC2581
    sFlow->ssthresh = std::max (sFlow->ssthresh, 2 * sFlow->MSS);

    double gThroughput = getGlobalThroughput();
    uint64_t lDelay = getPathDelay(sFlowIdx);

    switch ( m_algoCC )
    {
        case Uncoupled_TCPs:
            sFlow->cwnd  = std::max (cwnd  / 2, 1.0);
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" MpTcpSocketBase -> "<< " m_localToken "<< m_localToken<<" Subflow "<< (int)sFlowIdx <<": RTT "<< sFlow->rtt->GetCurrentEstimate().GetSeconds() <<" reducing cwnd from " << cwnd << " to "<<sFlow->cwnd <<" Throughput "<< (sFlow->cwnd * sFlow->MSS * 8) / sFlow->rtt->GetCurrentEstimate().GetSeconds()<< " GlobalThroughput "<<gThroughput<< " Efficacity " <<  getConnectionEfficiency() << " delay "<<lDelay << " Uncoupled_TCPs" );
            break;
        case Linked_Increases:
            sFlow->cwnd  = std::max (cwnd  / 2, 1.0);
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" MpTcpSocketBase -> "<< " m_localToken "<< m_localToken<<" Subflow "<< (int)sFlowIdx <<": RTT "<< sFlow->rtt->GetCurrentEstimate().GetSeconds() <<" reducing cwnd from " << cwnd << " to "<<sFlow->cwnd <<" Throughput "<< (sFlow->cwnd * sFlow->MSS * 8) / sFlow->rtt->GetCurrentEstimate().GetSeconds() << " GlobalThroughput "<<gThroughput<< " Efficacity " <<  getConnectionEfficiency() << " delay "<<lDelay <<" alpha "<< alpha << " Linked_Increases");
            break;
        case RTT_Compensator:
            sFlow->cwnd  = std::max (cwnd  / 2, 1.0);
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" MpTcpSocketBase -> "<< " m_localToken "<< m_localToken<<" Subflow "<< (int)sFlowIdx <<": RTT "<< sFlow->rtt->GetCurrentEstimate().GetSeconds() <<" reducing cwnd from " << cwnd << " to "<<sFlow->cwnd <<" Throughput "<< (sFlow->cwnd * sFlow->MSS * 8) / sFlow->rtt->GetCurrentEstimate().GetSeconds() << " GlobalThroughput "<<gThroughput<< " Efficacity " <<  getConnectionEfficiency() << " delay "<<lDelay <<" alpha "<< alpha << " RTT_Compensator");
            break;
        case Fully_Coupled:
            sFlow->cwnd  = std::max (cwnd - totalCwnd / 2, 1.0);
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" MpTcpSocketBase -> "<< " m_localToken "<< m_localToken<<" Subflow "<< (int)sFlowIdx <<": RTT "<< sFlow->rtt->GetCurrentEstimate().GetSeconds() <<" reducing cwnd from " << cwnd << " to "<<sFlow->cwnd <<" Throughput "<< (sFlow->cwnd * sFlow->MSS * 8) / sFlow->rtt->GetCurrentEstimate().GetSeconds() << " GlobalThroughput "<<gThroughput<< " Efficacity " <<  getConnectionEfficiency() << " delay "<<lDelay <<" alpha "<< alpha << " Fully_Coupled");
            break;
        default:
            sFlow->cwnd  = 1;
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" MpTcpSocketBase -> "<< " m_localToken "<< m_localToken<<" Subflow "<< (int)sFlowIdx <<": RTT "<<sFlow->rtt->GetCurrentEstimate().GetSeconds() <<" reducing cwnd from " << cwnd << " to "<<sFlow->cwnd <<" Throughput "<< (sFlow->cwnd * sFlow->MSS * 8) / sFlow->rtt->GetCurrentEstimate().GetSeconds() << " GlobalThroughput "<<gThroughput<< " Efficacity " <<  getConnectionEfficiency() << " delay "<<lDelay << " default");
            break;
    }

    sFlow->phase = Congestion_Avoidance;
    // sFlow->TxSeqNumber = sFlow->highestAck + 1; // Start from highest Ack
    sFlow->rtt->IncreaseMultiplier (); // DoubleValue timeout value for next retx timer
   /* if(int(sFlow->dPort)-5000==12 and (int)sFlowIdx==0){
        std::cout<<"flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
    }*/
std::cout<<"1 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
}

void
MpTcpSocketBase::Retransmit (uint8_t sFlowIdx)
{
  NS_LOG_FUNCTION (this);
  MpTcpSubFlow *sFlow = subflows[sFlowIdx];
  uint8_t flags = TcpHeader::ACK;
  uint8_t hlen = 5;
  uint8_t olen = 15;
  uint8_t plen = 0;
  /*if(int(sFlow->dPort)-5000==12 and int(sFlowIdx)==0){
      retransmit[12]++;
      std::cout<<"flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<"cwnd: "<<sFlow->cwnd<<"  retransmit: "<<retransmit[12]<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
     }*/
/*for(uint8_t i=0;i<subflows.size();i++){
     if(sFlowIdx==i){
        retransmit[i]++;
        std::cout<<"flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<"cwnd: "<<sFlow->cwnd<<"  retransmit: "<<retransmit[i]<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
     }
}*/

  //NS_ASSERT(sFlow->TxSeqNumber == sFlow->RxSeqNumber);
  DSNMapping * ptrDSN = sFlow->GetunAckPkt (AvailableWindow (sFlowIdx));
  if (ptrDSN == 0)
  {
      NS_LOG_WARN ("Retransmit -> no Unacked data !! mapDSN size is "<< sFlow->mapDSN.size() << " max Ack seq n° "<< sFlow->highestAck);
      return;
  }
  // Calculate remaining data for COE check
  Ptr<Packet> pkt = new Packet (ptrDSN->packet, ptrDSN->dataLevelLength);

  NS_LOG_WARN (Simulator::Now().GetSeconds() <<" RetransmitSegment -> "<< " m_localToken "<< m_localToken<<" Subflow "<<(int) sFlowIdx<<" DataSeq "<< ptrDSN->dataSeqNumber <<" SubflowSeq " << ptrDSN->subflowSeqNumber <<" dataLength " << ptrDSN->dataLevelLength << " packet size " << pkt->GetSize() <<" RTO_Timeout" );

  SetReTxTimeout (sFlowIdx);

  //sFlow->rtt->SentSeq (ptrDSN->subflowSeqNumber, ptrDSN->dataLevelLength);
  sFlow->rtt->pktRetransmit (SequenceNumber32(ptrDSN->subflowSeqNumber));

  // And send the packet
  MpTcpHeader mptcpHeader;
  mptcpHeader.SetSequenceNumber  (SequenceNumber32(ptrDSN->subflowSeqNumber));
  mptcpHeader.SetAckNumber       (SequenceNumber32(sFlow->RxSeqNumber));
  mptcpHeader.SetSourcePort      (sFlow->sPort);
  mptcpHeader.SetDestinationPort (sFlow->dPort);
  mptcpHeader.SetFlags           (flags);
  mptcpHeader.SetWindowSize      (AdvertisedWindowSize());

    mptcpHeader.AddOptDSN (OPT_DSN, ptrDSN->dataSeqNumber, ptrDSN->dataLevelLength, ptrDSN->subflowSeqNumber);

    switch ( m_algoPR )
    {
        case Eifel:
            if(ptrDSN->retransmited == false)
            {
                ptrDSN->retransmited = true;
                ptrDSN->tsval = Simulator::Now ().GetMilliSeconds (); // update timestamp value to the current time
            }
            mptcpHeader.AddOptTT  (OPT_TT, ptrDSN->tsval, 0);
            olen += 17;
            break;
        case D_SACK:
            if(ptrDSN->retransmited == false)
            {
                ptrDSN->retransmited = true;
                retransSeg[ptrDSN->dataSeqNumber] = ptrDSN->dataLevelLength;
            }
            break;
        case F_RTO:
            sFlow->ReTxSeqNumber = std::max(sFlow->ReTxSeqNumber, ptrDSN->subflowSeqNumber + ptrDSN->dataLevelLength);
            break;
        default:
            break;
    }

    plen = (4 - (olen % 4)) % 4;
    olen = (olen + plen) / 4;
    hlen += olen;
    mptcpHeader.SetLength(hlen);
    mptcpHeader.SetOptionsLength(olen);
    mptcpHeader.SetPaddingLength(plen);

  m_mptcp->SendPacket (pkt, mptcpHeader, sFlow->sAddr, sFlow->dAddr);
  //delete ptrDSN; // if you want let it you've to uncomment 'mapDSN.erase (it)' in method GetunAckPkt
}

void
MpTcpSocketBase::SetReTxTimeout (uint8_t sFlowIdx)
{
	NS_LOG_FUNCTION_NOARGS ();
    MpTcpSubFlow *sFlow = subflows[sFlowIdx];
    if ( sFlow->retxEvent.IsExpired () )
    {
        Time rto = sFlow->rtt->RetransmitTimeout ();

        	//rto=Seconds(0.2);  //Pablo uc
        NS_LOG_INFO ("Schedule ReTxTimeout subflow ("<<(int)sFlowIdx<<") at time " << Simulator::Now ().GetSeconds () << " after rto ("<<rto.GetSeconds ()<<") at " << (Simulator::Now () + rto).GetSeconds ());
        sFlow->retxEvent = Simulator::Schedule (rto,&MpTcpSocketBase::ReTxTimeout,this, sFlowIdx);
    }
}

bool
MpTcpSocketBase::ProcessAction (uint8_t sFlowIdx, Actions_t a)
{
    //NS_LOG_FUNCTION (this << m_node->GetId()<< sFlowIdx <<m_stateMachine->printAction(a) << a );
    MpTcpSubFlow * sFlow = subflows[sFlowIdx];
    bool result = true;
    switch (a)
    {
        case SYN_TX:
            NS_LOG_LOGIC ("MpTcpSocketBase"<<m_node->GetId()<<" " << this <<" Action: SYN_TX, Subflow: "<<sFlowIdx);
            SendEmptyPacket (sFlowIdx, TcpHeader::SYN);
            break;

        case ACK_TX:
            // this acknowledgement is not part of the handshake process
            NS_LOG_LOGIC ("MpTcpSocketBase " << this <<" Action ACK_TX");
            SendEmptyPacket (sFlowIdx, TcpHeader::ACK);
            break;

        case FIN_TX:
            NS_LOG_LOGIC ("MpTcpSocketBase "<<m_node->GetId()<<" "  << this <<" Action FIN_TX");
            NS_LOG_INFO  ("Number of rejected packet ("<<nbRejected<< ") total received packet (" << nbReceived <<")");
            SendEmptyPacket (sFlowIdx, TcpHeader::FIN);
            break;

        case FIN_ACK_TX:
            NS_LOG_LOGIC ("MpTcpSocketBase "<<m_node->GetId()<<" "  << this <<" Action FIN_ACK_TX");
            NS_LOG_INFO  ("Number of rejected packet ("<<nbRejected<< ") total received packet (" << nbReceived <<")");
            SendEmptyPacket (sFlowIdx, TcpHeader::FIN | TcpHeader::ACK);
            CloseMultipathConnection();
            sFlow->state = CLOSED;
            break;

        case TX_DATA:
            NS_LOG_LOGIC ("MpTcpSocketBase "<<m_node->GetId()<<" "  << this <<" Action TX_DATA");
            result = SendPendingData ();
            break;

        default:
            NS_LOG_LOGIC ("MpTcpSocketBase "<<m_node->GetId()<<": " << this <<" Action: " << m_stateMachine->printAction(a) << " ( " << a << " )" << " not handled");
            break;
    }
    return result;
}

bool
MpTcpSocketBase::ProcessAction (uint8_t sFlowIdx, MpTcpHeader mptcpHeader, Ptr<Packet> pkt, uint32_t dataLen, Actions_t a)
{
	NS_LOG_FUNCTION_NOARGS ();
    //NS_LOG_FUNCTION (this << m_node->GetId()<< sFlowIdx <<m_stateMachine->printAction(a) << a );
    MpTcpSubFlow * sFlow = subflows[sFlowIdx];
    bool result = true;
    uint32_t seq = 0;
    Time t;
    switch (a)
    {
        case ACK_TX_1:
            NS_LOG_LOGIC ("MpTcpSocketBase"<<m_node->GetId()<<" " << this <<" Action: ACK_TX_1");
            // TCP SYN consumes one byte
            if( sFlowIdx == 0)
                seq = 2;
            else
                //seq = 2;
                seq = 1; // because we send only ACK (1 packet)

            NS_LOG_INFO ("initiating RTO for subflow ("<< (int) sFlowIdx <<") with seq "<<sFlow->TxSeqNumber);
            sFlow->rtt->Init( mptcpHeader.GetAckNumber () + SequenceNumber32(seq) ); // initialize next with the next seq number to be sent

            //sFlow->rtt->est =  TimeUnit<1>("1.5s");

            //t=Seconds(1.5);
            sFlow->rtt->SetCurrentEstimate(t);

            sFlow->RxSeqNumber =  (mptcpHeader.GetSequenceNumber()).GetValue () + 1;
            sFlow->highestAck  = std::max ( sFlow->highestAck, (mptcpHeader.GetAckNumber ()).GetValue() - 1 );

            SendEmptyPacket (sFlowIdx, TcpHeader::ACK);
            if(m_addrAdvertised == false)
            {
                AdvertiseAvailableAddresses();
                m_addrAdvertised = true;
            }
            m_aggregatedBandwidth += sFlow->bandwidth;
            // when a single path is established between endpoints then we can say the connection is established
            if(m_state != ESTABLISHED)
                NotifyConnectionSucceeded ();

            m_state = ESTABLISHED;
            sFlow->StartTracing ("CongestionWindow");
            break;

        case SYN_ACK_TX:
            NS_LOG_INFO ("MpTcpSocketBase("<<m_node->GetId()<<") sFlowIdx("<< (int) sFlowIdx <<") Action SYN_ACK_TX");
            // TCP SYN consumes one byte
            sFlow->RxSeqNumber = (mptcpHeader.GetSequenceNumber()).GetValue() + 1 ;
            sFlow->highestAck  = std::max ( sFlow->highestAck, (mptcpHeader.GetAckNumber ()).GetValue() - 1 );
            SendEmptyPacket (sFlowIdx, TcpHeader::SYN | TcpHeader::ACK);
            break;

        case NEW_SEQ_RX:
            NS_LOG_LOGIC ("MpTcpSocketBase::ProcessAction -> " << this <<" Action NEW_SEQ_RX already processed in ProcessHeaderOptions");
            // Process new data received
            break;

        case NEW_ACK:
            // action performed by receiver
            NS_LOG_LOGIC ("MpTcpSocketBase::ProcessAction -> " << this <<" Action NEW_ACK");
            NewACK (sFlowIdx, mptcpHeader, 0);
            break;

        case SERV_NOTIFY:
            // the receiver had received the ACK confirming the establishment of the connection
            NS_LOG_LOGIC ("MpTcpSocketBase  Action SERV_NOTIFY -->  Connected!");
            sFlow->RxSeqNumber = (mptcpHeader.GetSequenceNumber()).GetValue() + 1; // next sequence to receive
            NS_LOG_LOGIC ("MpTcpSocketBase:Serv_Notify next ACK will be = " << sFlow->RxSeqNumber);
            sFlow->highestAck  = std::max ( sFlow->highestAck, (mptcpHeader.GetAckNumber ()).GetValue() - 1 );
            sFlow->connected   = true;
            if(m_connected != true)
                NotifyNewConnectionCreated (this, m_remoteAddress);
            m_connected        = true;
            break;

        default:
            result = ProcessAction ( sFlowIdx, a);
            break;
    }
    return result;
}

DSNMapping*
MpTcpSocketBase::getAckedSegment(uint8_t sFlowIdx, uint32_t ack)
{
	NS_LOG_FUNCTION_NOARGS ();
    MpTcpSubFlow* sFlow = subflows[sFlowIdx];
    DSNMapping* ptrDSN = 0;
    for (list<DSNMapping *>::iterator it = sFlow->mapDSN.begin(); it != sFlow->mapDSN.end(); ++it)
    {
        DSNMapping* dsn = *it;
        if(dsn->subflowSeqNumber + dsn->dataLevelLength == ack)
        {
            ptrDSN = dsn;
            break;
        }
    }
    return ptrDSN;
}

void
MpTcpSocketBase::NewACK (uint8_t sFlowIdx, MpTcpHeader mptcpHeader, TcpOptions *opt)
{
	NS_LOG_FUNCTION_NOARGS ();
    MpTcpSubFlow * sFlow = subflows[sFlowIdx];
    uint32_t ack = (mptcpHeader.GetAckNumber ()).GetValue();
    uint32_t ackedBytes = ack - sFlow->highestAck - 1;
    DSNMapping* ptrDSN = getAckedSegment(sFlowIdx, ack);
//std::cout<<"ID "<<(int)sFlowIdx<<" trans: "<<fs[sFlowIdx]<<std::endl;
fs[sFlowIdx]=0;
    if (m_algoPR == F_RTO)
    {
        uint16_t nbP[] = {389, 211, 457, 277, 367, 479, 233}; // some prime numbers
        double threshold = 0.061 * (((double) (((int) time (NULL)) % nbP[sFlowIdx])) / (double) nbP[sFlowIdx]);
        if(sFlow->nbRecvAck == -1)
            sFlow->nbRecvAck = (rejectPacket(threshold)==true ? 0:-1);
        else
        {
            sFlow->nbRecvAck++;
            if(sFlow->nbRecvAck < sFlow->cwnd)
            {
                return;
            }else
            {
                sFlow->nbRecvAck = -1;
            }
        }
    }
    if( (opt != 0) && (opt->optName == OPT_DSACK) )
    {
        OptDSACK* dsack = (OptDSACK*) opt;
        NS_LOG_WARN (Simulator::Now().GetSeconds() <<" DSACK_option -> Subflow "<<(int)sFlowIdx<<" 1stBlock lowerEdge "<<dsack->blocks[0]<<" upperEdge "<<dsack->blocks[1]<<" / 2ndBlock lowerEdge " << dsack->blocks[3] <<" upperEdge " << dsack->blocks[4] );
        DSNMapping* dsn = getAckedSegment(dsack->blocks[0], dsack->blocks[1]);
        if(ptrDSN != 0)
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" Cumulative_ACK -> "<< " m_localToken "<< m_localToken<<" Subflow "<<(int)sFlowIdx<<" Data_ACK "<<ptrDSN->dataSeqNumber + ptrDSN->dataLevelLength <<" Subflow_ACK "<< ack <<" Data_DSACK "<<dsack->blocks[0]<<" "<<dsack->blocks[1]<<" Subflow_DSACK "<<dsn->subflowSeqNumber<<" "<<dsn->subflowSeqNumber + dsn->dataLevelLength<<" highestAckedData " << sFlow->highestAck<<" maxSequenNumber " << sFlow->maxSeqNb <<" AckedData " << ackedBytes << " unAckedData " << ( sFlow->maxSeqNb - sFlow->highestAck ));
        else
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" Cumulative_ACK -> "<< " m_localToken "<< m_localToken<<" Subflow "<<(int)sFlowIdx<<" Data_ACK ? Subflow_ACK "<< ack <<" Data_DSACK "<<dsack->blocks[0]<<" "<<dsack->blocks[1]<<" Subflow_DSACK "<<dsn->subflowSeqNumber<<" "<<dsn->subflowSeqNumber + dsn->dataLevelLength<<" highestAckedData " << sFlow->highestAck<<" maxSequenNumber " << sFlow->maxSeqNb <<" AckedData " << ackedBytes << " unAckedData " << ( sFlow->maxSeqNb - sFlow->highestAck ));
    }else
    {
        if(ptrDSN != 0)
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" Cumulative_ACK -> "<< " m_localToken "<< m_localToken<<" Subflow "<<(int)sFlowIdx<<" Data_ACK "<<ptrDSN->dataSeqNumber + ptrDSN->dataLevelLength <<" Subflow_ACK "<< ack <<" highestAckedData " << sFlow->highestAck<<" maxSequenNumber " << sFlow->maxSeqNb <<" AckedData " << ackedBytes << " unAckedData " << ( sFlow->maxSeqNb - sFlow->highestAck ));
        else
            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" Cumulative_ACK -> "<< " m_localToken "<< m_localToken<<" Subflow "<<(int)sFlowIdx<<" Data_ACK ? Subflow_ACK "<< ack <<" highestAckedData " << sFlow->highestAck<<" maxSequenNumber " << sFlow->maxSeqNb <<" AckedData " << ackedBytes << " unAckedData " << ( sFlow->maxSeqNb - sFlow->highestAck ));
    }

    if( IsDuplicatedAck(sFlowIdx, mptcpHeader, opt) == false )
    {
        sFlow->retxEvent.Cancel (); //On recieving a "New" ack we restart retransmission timer .. RFC 2988

        sFlow->updateRTT      ((mptcpHeader.GetAckNumber()).GetValue(), Simulator::Now ());
        sFlow->RxSeqNumber  = (mptcpHeader.GetSequenceNumber()).GetValue() + 1;
        sFlow->highestAck   = std::max ( sFlow->highestAck, ack - 1 );
        unAckedDataCount    = ( sFlow->maxSeqNb - sFlow->highestAck ) ;

        if ( unAckedDataCount > 0 )
        {
            Time rto = sFlow->rtt->RetransmitTimeout ();
            NS_LOG_LOGIC ("Schedule ReTxTimeout at " << Simulator::Now ().GetSeconds () << " to expire at " << (Simulator::Now () + rto).GetSeconds () <<" unAcked data "<<unAckedDataCount);
            sFlow->retxEvent = Simulator::Schedule (rto, &MpTcpSocketBase::ReTxTimeout, this, sFlowIdx);
        }
        // you have to move the idxBegin of the sendingBuffer by the amount of newly acked data
        OpenCWND (sFlowIdx, ackedBytes);
        NotifyDataSent ( GetTxAvailable() );
        SendPendingData ();
    }else if( (useFastRecovery == true) || (m_algoPR == F_RTO && frtoStep == Step_4) )
    {
        // remove sequence gap from DNSMap list
        NS_LOG_WARN (Simulator::Now ().GetSeconds () << " Fast Recovery -> duplicated ACK ("<< (mptcpHeader.GetAckNumber ()).GetValue() <<")");
        OpenCWND (sFlowIdx, 0);
        SendPendingData ();
    }
}

Actions_t
MpTcpSocketBase::ProcessEvent (uint8_t sFlowId, Events_t e)
{
	NS_LOG_FUNCTION_NOARGS ();
    //NS_LOG_FUNCTION(this << (int) sFlowId << m_stateMachine->printEvent(e));
    MpTcpSubFlow * sFlow = subflows[sFlowId];
    if( sFlow == 0 )
        return NO_ACT;
    TcpStates_t previous = sFlow->state;
    SA sAct = m_stateMachine->Lookup(sFlow->state, e);
    if( previous == LISTEN && sAct.state == SYN_RCVD && sFlow->connected == true )
        return NO_ACT;

    sFlow->state = sAct.state;
    NS_LOG_LOGIC ("MpTcpSocketBase"<<m_node->GetId()<<":ProcessEvent Moved from subflow "<<(int)sFlowId <<" state " << m_stateMachine->printState(previous) << " to " << m_stateMachine->printState(sFlow->state));

    if (!m_connected && previous == SYN_SENT && sFlow->state == ESTABLISHED)
    {
        // this means the application side has completed its portion of the handshaking
        //Simulator::ScheduleNow(&NotifyConnectionSucceeded(), this);
        m_connected = true;
        m_endPoint->SetPeer (m_remoteAddress, m_remotePort);
    }
    return sAct.action;
}

void
MpTcpSocketBase::SendEmptyPacket (uint8_t sFlowId, uint8_t flags)
{
	NS_LOG_FUNCTION_NOARGS ();
  //NS_LOG_FUNCTION (this << (int) sFlowId << (uint32_t)flags);
  MpTcpSubFlow *sFlow = subflows[sFlowId];
  Ptr<Packet> p = new Packet(0); //Create<Packet> ();
  MpTcpHeader header;
  uint8_t hlen = 0;
  uint8_t olen = 0;

  header.SetSourcePort      (sFlow->sPort);
  header.SetDestinationPort (sFlow->dPort);
  header.SetFlags           (flags);
  header.SetSequenceNumber  (SequenceNumber32(sFlow->TxSeqNumber));
  header.SetAckNumber       (SequenceNumber32(sFlow->RxSeqNumber));
  header.SetWindowSize      (AdvertisedWindowSize());

  if(((sFlow->state == SYN_SENT) || (sFlow->state==SYN_RCVD && m_mpEnabled==true)) && m_mpSendState==MP_NONE)
  {
      m_mpSendState = MP_MPC;
      m_localToken  = rand() % 1000;
      header.AddOptMPC(OPT_MPC, m_localToken);
      olen += 5;
  }

  uint8_t plen = (4 - (olen % 4)) % 4;
  // urgent pointer
  // check sum filed
  olen = (olen + plen) / 4;
  hlen = 5 + olen;
  header.SetLength(hlen);
  header.SetOptionsLength(olen);
  header.SetPaddingLength(plen);

  //SetReTxTimeout (sFlowId);

  m_mptcp->SendPacket (p, header, sFlow->sAddr, sFlow->dAddr);
  //sFlow->rtt->SentSeq (sFlow->TxSeqNumber, 1);           // notify the RTT
  sFlow->TxSeqNumber++;
  sFlow->maxSeqNb++;
  //unAckedDataCount++;
}

void
MpTcpSocketBase::SendAcknowledge (uint8_t sFlowId, uint8_t flags, TcpOptions *opt)
{
	NS_LOG_FUNCTION_NOARGS ();
  //NS_LOG_FUNCTION (this << (int) sFlowId << (uint32_t)flags);
  NS_LOG_INFO ("sending acknowledge segment with option");
  MpTcpSubFlow *sFlow = subflows[sFlowId];
  Ptr<Packet> p = new Packet(0); //Create<Packet> ();
  MpTcpHeader header;
  uint8_t hlen = 0;
  uint8_t olen = 0;

  header.SetSourcePort      (sFlow->sPort);
  header.SetDestinationPort (sFlow->dPort);
  header.SetFlags           (flags);
  header.SetSequenceNumber  (SequenceNumber32(sFlow->TxSeqNumber));
  header.SetAckNumber       (SequenceNumber32(sFlow->RxSeqNumber));
  header.SetWindowSize      (AdvertisedWindowSize());

    switch ( m_algoPR )
    {
        case Eifel:
            header.AddOptTT (OPT_TT, ((OptTimesTamp *) opt)->TSval, ((OptTimesTamp *) opt)->TSecr);
            olen += 17;
            // I've to think about if I can increment or not the sequence control parameters
            sFlow->TxSeqNumber++;
            sFlow->maxSeqNb++;
            break;
        case D_SACK:
            header.AddOptDSACK (OPT_DSACK, (OptDSACK *) opt);
            olen += 33;
            break;
        default:
            break;
    }

  uint8_t plen = (4 - (olen % 4)) % 4;
  // urgent pointer
  // check sum filed
  olen = (olen + plen) / 4;
  hlen = 5 + olen;
  header.SetLength(hlen);
  header.SetOptionsLength(olen);
  header.SetPaddingLength(plen);
  m_mptcp->SendPacket (p, header, sFlow->sAddr, sFlow->dAddr);

}

void
MpTcpSocketBase::allocateSendingBuffer (uint32_t size)
{

    NS_LOG_FUNCTION(this << size);
    sendingBuffer = new DataBuffer(size);
}

void
MpTcpSocketBase::allocateRecvingBuffer (uint32_t size)
{
    NS_LOG_FUNCTION(this << size);
    recvingBuffer = new DataBuffer(size);
}

void
MpTcpSocketBase::SetunOrdBufMaxSize (uint32_t size)
{
	NS_LOG_FUNCTION_NOARGS ();
    unOrdMaxSize = size;
}

uint32_t
MpTcpSocketBase::Recv (uint8_t* buf, uint32_t size)
{
  NS_LOG_FUNCTION (this << size);
  //Null packet means no data to read, and an empty packet indicates EOF
  uint32_t toRead = std::min( recvingBuffer->PendingData() , size);
  return recvingBuffer->Retrieve(buf, toRead);
}

void
//MpTcpSocketBase::ForwardUp (Ptr<Packet> p, Ipv4Address ipv4Remote, uint16_t port, Ptr<Ipv4Interface> inter)

MpTcpSocketBase::ForwardUp (Ptr<Packet> p, Ipv4Header ipv4Head, uint16_t port, Ptr<Ipv4Interface> inter)
{
	NS_LOG_FUNCTION_NOARGS ();

  Ipv4Address ipv4Remote;
  ipv4Remote=ipv4Head.GetSource();

  m_remoteAddress = ipv4Remote; //m_endPoint->GetPeerAddress();

  m_remotePort    = m_endPoint->GetPeerPort();
  m_localAddress  = m_endPoint->GetLocalAddress();

  uint8_t sFlowIdx = LookupByAddrs(m_localAddress, ipv4Remote); //m_endPoint->GetPeerAddress());

  if(! (sFlowIdx < m_maxSubFlowNumber) )
      return;

  MpTcpSubFlow *sFlow = subflows[sFlowIdx];

  MpTcpHeader mptcpHeader;
  p->RemoveHeader (mptcpHeader);

  uint32_t dataLen;   // packet's payload length
  remoteRecvWnd = (uint32_t) mptcpHeader.GetWindowSize (); //update the flow control window

  //Events_t event = SimulationSingleton<TcpStateMachine>::Get ()->FlagsEvent (tcpHeader.GetFlags () );
  sFlow->dAddr = ipv4Remote;//m_endPoint->GetPeerAddress();
  sFlow->dPort = m_endPoint->GetPeerPort();
  // process the options in the header
  Actions_t action = ProcessHeaderOptions(sFlowIdx, p, &dataLen, mptcpHeader);

  //NS_LOG_INFO("MpTcpSocketBase"<<m_node->GetId()<<":ForwardUp Socket " << this << " ( current state " << m_stateMachine->printState( sFlow->state ) << " ) -> processing packet action is " << m_stateMachine->printAction( action ) );

  ProcessAction (sFlowIdx, mptcpHeader, p, dataLen, action);
}

void
MpTcpSocketBase::ProcessMultipathState ()
{
    NS_LOG_FUNCTION_NOARGS();
    switch(m_mpState)
    {
        case MP_ADDR:
            m_mpState = MP_JOIN;
            InitiateSubflows();
            break;
        default:
            break;
    }
}

bool
MpTcpSocketBase::InitiateSubflows ()
{
    NS_LOG_FUNCTION_NOARGS();
    bool result = true;
    for(uint32_t i =0; i < localAddrs.size(); i++)
    for(uint32_t j =i; j < remoteAddrs.size(); j++)
    {
        uint8_t addrID     = localAddrs[i]->addrID;
        Ipv4Address local  = localAddrs[i]->ipv4Addr;
        Ipv4Address remote = remoteAddrs[j]->ipv4Addr;

        // skip already established flows
        if( ((local == m_localAddress) && (remote == m_remoteAddress)) || (!IsThereRoute(local, remote)))
            continue;
NS_LOG_LOGIC ("IsThereRoute -> Route from srcAddr "<< local << " to dstAddr " << remote <<", exist !");
        uint32_t initSeqNb = rand() % 1000 + (sfInitSeqNb.size() +1 ) * 10000;
        sfInitSeqNb[local] = initSeqNb;
        Ptr<Packet> pkt = Create<Packet> ();
        MpTcpHeader header;
        header.SetFlags           (TcpHeader::SYN);//flags);
        header.SetSequenceNumber  (SequenceNumber32(initSeqNb));
        header.SetAckNumber       (SequenceNumber32(subflows[0]->RxSeqNumber));
        // endpoints port number remain unchangeable
        header.SetSourcePort      (m_endPoint->GetLocalPort ());
        header.SetDestinationPort (m_remotePort);

        header.AddOptJOIN         (OPT_JOIN, m_remoteToken, addrID);

        uint8_t olen = 6;
        uint8_t plen = (4 - (olen % 4)) % 4;

        header.SetWindowSize ( AdvertisedWindowSize() );
        // urgent pointer
        // check sum filed
        olen = (olen + plen) / 4;
        uint8_t hlen = 5 + olen;
        header.SetLength(hlen);
        header.SetOptionsLength(olen);
        header.SetPaddingLength(plen);

        //SetReTxTimeout (sFlowIdx);
        m_mptcp->SendPacket (pkt, header, local, remote);
NS_LOG_INFO("MpTcpSocketBase::InitiateSubflows -> (src, dst) = (" << local << ", " << remote << ") JOIN segment successfully sent !");

    }
    return result;
}

void
MpTcpSocketBase::calculateTotalCWND ()
{
	NS_LOG_FUNCTION_NOARGS ();
    totalCwnd = 0;
    for (uint32_t i = 0; i < subflows.size() ; i++)
    {
        totalCwnd += subflows[i]->cwnd;
    }
}

Actions_t
MpTcpSocketBase::ProcessHeaderOptions (uint8_t sFlowIdx, Ptr<Packet> pkt, uint32_t *dataLen, MpTcpHeader mptcpHeader)
{
    // packet is without header, see ForwardUp method
    NS_LOG_FUNCTION(this);
    MpTcpSubFlow * sFlow = subflows[sFlowIdx];
    vector< TcpOptions* > options = mptcpHeader.GetOptions();
    uint8_t flags = mptcpHeader.GetFlags ();

  Actions_t action = ProcessEvent (sFlowIdx, m_stateMachine->FlagsEvent (flags)); //updates the state
  //NS_LOG_INFO("MpTcpSocketBase::ProcessHeaderOptions-> event  ("<< m_stateMachine->printEvent(m_stateMachine->FlagsEvent (flags))<<") => action = "<< m_stateMachine->printAction( action ));
  int length = 0;
  TcpOptions *opt, *opt1;

  bool hasSyn = flags &  TcpHeader::SYN;
  //bool hasFin = flags &  TcpHeader::FIN;
  //bool isAck  = flags == TcpHeader::ACK;
  bool TxAddr = false, TxACK = false, TxAckOpt = false;
  bool initSubFlow = false;

  for(uint32_t j = 0; j < options.size(); j++)
  {
      opt = options[j];

      if ( (opt->optName == OPT_MPC) && hasSyn && (m_mpRecvState == MP_NONE) )
      {
          //NS_LOG_INFO("MpTcpSocketBase:ProcessHeaderOptions -> OPT_MPC received");
          m_mpRecvState = MP_MPC;
          m_mpEnabled   = true;
          m_remoteToken = ((OptMultipathCapable *)opt)->senderToken;
//          initSubFlow = true;

      }else if((opt->optName == OPT_JOIN) && hasSyn)
      {
          OptJoinConnection * optJoin = (OptJoinConnection *) opt;
          if( (m_mpSendState == MP_ADDR) && (m_localToken == optJoin->receiverToken) )
          {
              // Join option is sent over the path (couple of addresses) not already in use
              //NS_LOG_INFO ( "MpTcpSocketBase::ProcessHeaderOptions -> OPT_JOIN received");
             initSubFlow = true;
          }

      }else if((opt->optName == OPT_ADDR) && (m_mpRecvState==MP_MPC))
      {
          // necessary action must be done here
          TxAddr = true;
          MpTcpAddressInfo * addrInfo = new MpTcpAddressInfo();
          addrInfo->addrID   = ((OptAddAddress *)opt)->addrID;
          addrInfo->ipv4Addr = ((OptAddAddress *)opt)->addr;
          remoteAddrs.insert  (remoteAddrs.end(), addrInfo);
          //NS_LOG_INFO ( "MpTcpSocketBase::ProcessHeaderOptions -> remote address " << addrInfo->ipv4Addr );
      }else if(opt->optName == OPT_REMADR)
      {
          length += 2;
      }else if(opt->optName == OPT_TT)
      {
          NS_LOG_INFO ("TCP TimesTamp option");
          if(server == true)
          {
              opt1 = new OptTimesTamp (OPT_TT, Simulator::Now ().GetMilliSeconds (), ((OptTimesTamp *) opt)->TSval);
          }else if(client == true)
          {
              NS_LOG_INFO ("This is a client");
              opt1 = (OptTimesTamp *) opt;
          }
          TxAckOpt = true;
      }else if (opt->optName == OPT_DSACK && client == true)
      {
          NS_LOG_INFO ("Client received DSACK option");
          opt1 = (OptDSACK *) opt;
          TxAckOpt = true;
      }else if(opt->optName == OPT_DSN)
      {
          // data reception so threat it
          OptDataSeqMapping * optDSN = (OptDataSeqMapping *) opt;
          TxACK = true;
          *dataLen = optDSN->dataLevelLength;
          Ptr<Packet> packet = pkt;
          uint32_t pktLen    = *dataLen;

NS_LOG_LOGIC("Multipath Seq N° dataSeqNumber (" << optDSN->dataSeqNumber <<") Seq N° nextRxSequence (" << nextRxSequence<<")   /   Subflow Seq N° RxSeqNumber (" << sFlow->RxSeqNumber << ") Seq N° subflowSeqNumber (" << optDSN->subflowSeqNumber<< ")");

          if ( (m_algoPR == D_SACK) && (optDSN->dataSeqNumber > nextRxSequence) )
          {
              NS_LOG_DEBUG ("Subflow ("<<(int)sFlowIdx<<"): data arrived ("<< optDSN->dataSeqNumber <<") of length ("<< optDSN->dataLevelLength <<") buffered for subsequent reordering !");
              StoreUnOrderedData (new DSNMapping(sFlowIdx, optDSN->dataSeqNumber, optDSN->dataLevelLength, optDSN->subflowSeqNumber,(mptcpHeader.GetAckNumber ()).GetValue(), pkt));
              // send ACK with DSACK only for segment that also create a hole in the subflow level
              //if(optDSN->subflowSeqNumber > sFlow->RxSeqNumber)
              //{
                  NS_LOG_DEBUG ("DSACK option to be created !");
                  opt1 = createOptDSACK (optDSN);
                  TxAckOpt = true;
              //}
              TxACK = false;
          }else if( optDSN->subflowSeqNumber == sFlow->RxSeqNumber )
          {
              if( optDSN->dataSeqNumber == nextRxSequence )
              {
                  // it's ok for this data, send ACK( sFlowSeq + dataLevel) and save data to reception buffer
                  NS_LOG_LOGIC("MpTcpSocketBase::ProcessHeaderOptions -> acknowledgment for data length ("<< optDSN->dataLevelLength <<") to be sent on subflow (" << (int) sFlowIdx << ") remote address (" << sFlow->dAddr<<")");

                  uint32_t amountRead = recvingBuffer->ReadPacket (pkt, pktLen);
                  sFlow->RxSeqNumber += amountRead;
                  nextRxSequence     += amountRead;
                  sFlow->highestAck   = std::max ( sFlow->highestAck, (mptcpHeader.GetAckNumber ()).GetValue() - 1 );
                  // acknowledgement for this segment will be sent because we've already set TxACK
                  ReadUnOrderedData ();
                  NotifyDataRecv    ();

              }else if( optDSN->dataSeqNumber > nextRxSequence )
              {
                  // it's ok for the subflow but not for the connection -> put the data on resequency buffer
                  NS_LOG_DEBUG("Subflow ("<<(int)sFlowIdx<<"): data arrived ("<< optDSN->dataSeqNumber <<") of length ("<< optDSN->dataLevelLength <<") buffered for subsequent reordering !");
                  TxACK = StoreUnOrderedData(new DSNMapping(sFlowIdx, optDSN->dataSeqNumber, optDSN->dataLevelLength, optDSN->subflowSeqNumber, (mptcpHeader.GetAckNumber ()).GetValue(), pkt));

                  // we send an ACK back for the received segment not in sequence
                  TxACK = false;
                  //sFlow->RxSeqNumber += pktLen; // the received data is in sequence of the subflow => ack it's reception
              }else
              {
                  NS_LOG_LOGIC("MpTcpSocketBase::ProcessHeaderOptions -> Subflow ("<<(int)sFlowIdx<<"): data received but duplicated, reject ("<<optDSN->subflowSeqNumber<<")");
              }
          }else if( optDSN->subflowSeqNumber > sFlow->RxSeqNumber )
          {
              NS_LOG_LOGIC("MpTcpSocketBase::ProcessHeaderOptions -> Subflow ("<<(int)sFlowIdx<<"): data arrived ("<<optDSN->subflowSeqNumber<<") causing potontial data lost !");
              TxACK = StoreUnOrderedData(new DSNMapping(sFlowIdx, optDSN->dataSeqNumber, optDSN->dataLevelLength, optDSN->subflowSeqNumber, (mptcpHeader.GetAckNumber ()).GetValue(), pkt));
              //TxACK = false;
          }else
          {
              NS_LOG_LOGIC("MpTcpSocketBase::ProcessHeaderOptions -> Subflow ("<<(int)sFlowIdx<<"): data already received, reject !");
              //action = NO_ACT;
          }
      }
  }

  if(TxAddr==true)
  {
    m_mpRecvState = MP_ADDR;
    sFlow->RxSeqNumber ++;
    sFlow->highestAck++;

    action = NO_ACT;
    if (m_mpSendState!=MP_ADDR)
    {
        AdvertiseAvailableAddresses(); // this is what the receiver has to do
    }
    else if (m_mpSendState==MP_ADDR)
    {
        sFlow->highestAck++; // we add 2 to highestAck because ACK, ADDR segments send it before
        InitiateSubflows();  // this is what the initiator has to do
    }
  }

  if(TxAckOpt == true)
  {
      if(server == true)
      {
          SendAcknowledge (sFlowIdx, TcpHeader::ACK, opt1);
      }
      else if(client == true)
      {
          NewACK (sFlowIdx, mptcpHeader, opt1);
      }
      action = NO_ACT;
  }else if (TxACK == true)
  {

NS_LOG_INFO ( "Recv: Subflow ("<<(int) sFlowIdx <<") TxSeq (" << sFlow->TxSeqNumber <<") RxSeq (" << sFlow->RxSeqNumber <<")\n" );

      SendEmptyPacket (sFlowIdx, TcpHeader::ACK);
      action = NO_ACT;
  }
  return action;
}

OptDSACK*
MpTcpSocketBase::createOptDSACK (OptDataSeqMapping * optDSN)
{
	NS_LOG_FUNCTION_NOARGS ();
    OptDSACK* ptrDSAK = new OptDSACK (OPT_DSACK);
    // Add the received segment which generated the currently prepared acknowledgement
    ptrDSAK->AddfstBlock(optDSN->dataSeqNumber, optDSN->dataSeqNumber + optDSN->dataLevelLength);

    uint64_t upperBound = 0;
    uint64_t lowerBound = 0;
    if ( unOrdered.size() == 1 )
    {
        DSNMapping *ptr1 = (* (unOrdered.begin()));
        lowerBound = ptr1->dataSeqNumber;
        upperBound = ptr1->dataSeqNumber + ptr1->dataLevelLength;
    }
    else
    {
        map<uint64_t, uint32_t> blocs;
        list<DSNMapping *>::iterator current = unOrdered.begin();
        list<DSNMapping *>::iterator next = unOrdered.begin();
        ++next;

        while( next != unOrdered.end() )
        {
            DSNMapping *ptr1 = *current;
            DSNMapping *ptr2 = *next;
            uint32_t  length = 0;
            uint64_t  dsn1   = ptr1->dataSeqNumber;
            uint64_t  dsn2   = ptr2->dataSeqNumber;

            if ( blocs.find ( dsn1 ) != blocs.end() )
            {
                length = blocs [dsn1];
            }
            else
            {
                length       = ptr1->dataLevelLength;
                blocs[dsn1]  = ptr1->dataLevelLength;
            }

            if ( dsn1 + length == dsn2 )
            {
                blocs[dsn1] = blocs[dsn1] + ptr2->dataLevelLength;
            }
            else
            {
                current = next;
            }
            ++next;
        }
        DSNMapping *ptr1 = (* (unOrdered.begin()));
        lowerBound = ptr1->dataSeqNumber;
        upperBound = ptr1->dataSeqNumber + blocs[ptr1->dataSeqNumber];

        NS_LOG_INFO ("createOptDSACK -> blocs size = " << blocs.size() );
    }
    ptrDSAK->AddBlock( lowerBound, upperBound);
    return ptrDSAK;
}

void
MpTcpSocketBase::ReadUnOrderedData ()
{
	NS_LOG_FUNCTION_NOARGS ();
    //NS_LOG_FUNCTION (this);
    list<DSNMapping *>::iterator current = unOrdered.begin();
    list<DSNMapping *>::iterator next = unOrdered.begin();

    // I changed this method, now whenever a segment is readed it get dropped from that list
    while(next != unOrdered.end())
    {
        ++next;
        DSNMapping *ptr   = *current;
        uint32_t sFlowIdx = ptr->subflowIndex;
        MpTcpSubFlow *sFlow = subflows[ sFlowIdx ];
        if ( (ptr->dataSeqNumber <= nextRxSequence) && (ptr->subflowSeqNumber == sFlow->RxSeqNumber) )
        {
            uint32_t amount = recvingBuffer->Add (ptr->packet, ptr->dataLevelLength);

            if(amount == 0)
                break; // reception buffer is full

            sFlow->RxSeqNumber += amount;
            sFlow->highestAck   = std::max ( sFlow->highestAck, ptr->acknowledgement - 1 );
            nextRxSequence     += amount;
            NS_LOG_INFO ("ReadUnOrderedData("<<unOrdered.size()<<") -> in sequence data (" << amount<<") found saved => Acknowledgement ("<<sFlow->RxSeqNumber<<") data seq ("<<ptr->dataSeqNumber<<") sent on subflow ("<< sFlowIdx<<")." );
            /**
             * Send an acumulative acknowledge
             */
            switch( m_algoPR )
            {
                case Eifel:
                    //SendAcknowledge (sFlowIdx, TcpHeader::ACK, new OptTimesTamp (OPT_TT, Simulator::Now ().GetMilliSeconds (), ((OptTimesTamp *) opt)->TSval));
                    break;
                case D_SACK:
                    // don't send an ACK for already acked segment
                    break;
                default:
                    //SendEmptyPacket (sFlowIdx, TcpHeader::ACK);
                    break;
            }
            SendEmptyPacket (sFlowIdx, TcpHeader::ACK);
            NotifyDataRecv ();
            unOrdered.erase( current );
        }
        current = next;
    }
}

uint8_t
MpTcpSocketBase::ProcessOption (TcpOptions *opt)
{
	NS_LOG_FUNCTION_NOARGS ();
    uint8_t originalSFlow = 255;
    if( opt != 0 ) {
    if( opt->optName == OPT_DSACK )
    {
        OptDSACK * dsack = (OptDSACK *) opt;
        // fstLeft = dsack->blocks[0];         fstRight = dsack->blocks[1];
        uint64_t fstLeft = dsack->blocks[0], fstRight = dsack->blocks[1];
        uint64_t sndLeft = dsack->blocks[2], sndRight = dsack->blocks[3];
        NS_LOG_DEBUG("ProcessOption -> sndLeft ("<<sndLeft<<") sndRight ("<<sndRight<<")");
        /**
         * go throw each sent packet which is pending for un ACK, and check if in the received option there is a ACK at the data level
         * we prefer to drop the packet because we will not receive ACK for it
         */

        for (uint8_t i = 0; i < subflows.size(); i++)
        {
            MpTcpSubFlow *sFlow = subflows[i];
            list<DSNMapping *>::iterator current = sFlow->mapDSN.begin();
            list<DSNMapping *>::iterator next    = sFlow->mapDSN.begin();
            while( current != sFlow->mapDSN.end() )
            {
                ++next;
                DSNMapping *ptrDSN = *current;
                if ( ((ptrDSN->dataSeqNumber == fstLeft) && (ptrDSN->dataSeqNumber + ptrDSN->dataLevelLength == fstRight))
                    ||
                     ((ptrDSN->dataSeqNumber >= sndLeft) && (ptrDSN->dataSeqNumber + ptrDSN->dataLevelLength <= sndRight)) )
                {
                    NS_LOG_DEBUG("acked segment with DSACK -> subflowSeqNumber ("<<ptrDSN->subflowSeqNumber<<")");
                    /**
                     * increment the number of ack received for that data level sequence number
                     */
                    if( ackedSeg.find(ptrDSN->dataSeqNumber) != ackedSeg.end() )
                        ackedSeg[ ptrDSN->dataSeqNumber ] = ackedSeg[ ptrDSN->dataSeqNumber ] + 1;
                    else
                        ackedSeg[ ptrDSN->dataSeqNumber ] = 1;
                    /**
                     * By checking the data level sequence number in the received TCP header option
                     * we can deduce that the current segment was correctly received, so must be removed from DSNMapping list
                     */
                    /*
                    next = sFlow->mapDSN.erase( current );
                    delete ptrDSN;
                    */
                }
                current = next;
            }
        }
    }
    }
    return originalSFlow;
}

bool
MpTcpSocketBase::IsDuplicatedAck (uint8_t sFlowIdx, MpTcpHeader l4Header, TcpOptions *opt)
{
	NS_LOG_FUNCTION_NOARGS ();
    //NS_LOG_FUNCTION (this << (int)sFlowIdx << (uint32_t) l4Header.GetAckNumber ());
    uint32_t ack = (l4Header.GetAckNumber ()).GetValue();
    bool duplicated = false;

    //uint8_t originalSFlow =
    ProcessOption (opt);
    MpTcpSubFlow *sFlow = subflows[sFlowIdx];

    if ( ack < sFlow->TxSeqNumber )
    {
        // this acknowledgment don't ack the last sent data, so check to find the duplicated
        list<DSNMapping *>::iterator current = sFlow->mapDSN.begin();
        list<DSNMapping *>::iterator next = sFlow->mapDSN.begin();
        while( current != sFlow->mapDSN.end() )
        {
            ++next;
            DSNMapping *ptrDSN = *current;

            NS_LOG_LOGIC ("IsDupAck -> subflow seq n° ("<< ptrDSN->subflowSeqNumber <<") data length " << ptrDSN->dataLevelLength);
            if ( ptrDSN->subflowSeqNumber + ptrDSN->dataLevelLength <= ack )
            {
                /**
                 * increment the number of ack received for that data level sequence number
                 */
                if( ackedSeg.find(ptrDSN->dataSeqNumber) != ackedSeg.end() )
                    ackedSeg[ ptrDSN->dataSeqNumber ] = ackedSeg[ ptrDSN->dataSeqNumber ] + 1;
                else
                    ackedSeg[ ptrDSN->dataSeqNumber ] = 1;
                /**
                 * this segment was correctly acked, so must be removed from DSNMapping list
                 */
                /*
                next = sFlow->mapDSN.erase( current );
                delete ptrDSN;
                */
            }
            else if ( (ptrDSN->subflowSeqNumber == ack) && (ack - 1 <= sFlow->highestAck) )
            {
            // this may have to be retransmitted
                NS_LOG_INFO("IsDupAck -> duplicated ack for " << ack);
                duplicated = true;
                switch ( m_algoPR )
                {
                    case Eifel:
                        if( (ptrDSN->retransmited == true) && (ptrDSN->tsval > ((OptTimesTamp *) opt)->TSecr) )
                        {
                            // spurious Retransmission
                            NS_LOG_WARN ("A Spurious Retransmission detected ->");

                            //double rtt = sFlow->rtt->est.GetSeconds();
                            double rtt = sFlow->rtt->GetCurrentEstimate().GetSeconds();
                            NS_LOG_WARN (Simulator::Now().GetSeconds() <<" MpTcpSocketBase -> "<< " m_localToken "<< m_localToken<<" Subflow "<< (int)sFlowIdx <<": RTT "<< sFlow->rtt->GetCurrentEstimate().GetSeconds() <<" Moving cwnd from " << sFlow->cwnd << " to " << sFlow->savedCWND << " Throughput "<<(sFlow->cwnd * sFlow->MSS * 8)/rtt<< " GlobalThroughput "<<getGlobalThroughput());
                            sFlow->cwnd = sFlow->savedCWND;
                            sFlow->ssthresh = sFlow->savedSSThresh;
                        }else
                        {
                            DupAck (sFlowIdx, ptrDSN);
                        }
                        break;
                    case D_SACK:
                        //if( (opt != 0) && (IsRetransmitted (((OptDSACK *) opt)->blocks[0], ((OptDSACK *) opt)->blocks[1]) == true) )

                        if(opt != 0)
                        {
                            NS_LOG_DEBUG ("received ACK with DSACK option !");
                            DupDSACK (sFlowIdx, l4Header, (OptDSACK *) opt);
                        }else
                        {
                            NS_LOG_DEBUG ("received ACK without DSACK option !");
                            DupAck (sFlowIdx, ptrDSN);
                        }
                        break;
                    case F_RTO:
                        break;
                    case TCP_DOOR:
                        break;
                    case NoPR_Algo:
                        DupAck (sFlowIdx, ptrDSN);
                        break;
                    default:
                        break;
                }
                break;
            }
            current = next;
        }
        if( (m_algoPR == F_RTO) && (sFlow->phase == RTO_Recovery) )
        {
            sFlow->ackCount ++;
            switch ( frtoStep )
            {
                case Step_2:
                    if ( sFlow->ackCount == 1 )
                    {   // first acknowledgement after a RTO timeout
                        sFlow->recover = sFlow->TxSeqNumber;
                        if( (duplicated == true) || (ack == sFlow->recover) || (ack <= sFlow->ReTxSeqNumber) )
                        {
                    /**
                     * revert to the conventional RTO recovery and continue by
                     * retransmitting unacknowledged data in slow start.
                     * Do not enter step 3 of this algorithm.
                     * The SpuriousRecovery variable remains false
                     */
                            NS_LOG_WARN ("4th dupAck -> Fast Recovery: sending new packet in slow start");
                            frtoStep = Step_4;

                        }else if( (duplicated == false) && (ack < sFlow->recover) )
                        {
                    /**
                     * transmit up to two new (previously unsent) segments
                     * enter step 3 of this algorithm
                     */
                            frtoStep = Step_3;
                            if( (sendingBuffer->Empty() == true) || (std::min (AvailableWindow (sFlowIdx), sendingBuffer->PendingData()) == 0) )
                            {
                                frtoStep = Step_4;
                            }
                        }
                    }else
                    {
                        frtoStep = Step_4;
                    }
                    break;

                case Step_3:
                    if(sFlow->ackCount == 2)
                    {   // second acknowledgement after a RTO timeout
                        if( duplicated == true )
                        {
                            sFlow->cwnd = 3;
                            frtoStep = Step_4;
                        }else if( ack > sFlow->highestAck + 1 )
                        {
                            sFlow->SpuriousRecovery = true;
                            sFlow->recover = sFlow->TxSeqNumber;
                            NS_LOG_WARN ("F-RTO -> A spurious timeout have been detected !");
                        }
                    }else
                    {
                        frtoStep = Step_4;
                    }
                    break;

                default:
                    break;
            }

        }
    }
  
std::cout<<"1 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl; 
  return duplicated;
}

void
MpTcpSocketBase::DupDSACK (uint8_t sFlowIdx, MpTcpHeader mptcpHeader, OptDSACK *dsack)
{
	NS_LOG_FUNCTION_NOARGS ();
    uint64_t leftEdge  = dsack->blocks[0];
    //uint64_t rightEdge = dsack->blocks[1];
    NS_LOG_DEBUG("ackedSeg size = "<<ackedSeg.size());
    MpTcpSubFlow *originalSFlow = subflows[sFlowIdx];
    DSNMapping *notAckedPkt = 0;
    for (uint8_t i = 0; i < subflows.size(); i++)
    {
        MpTcpSubFlow *sFlow = subflows[i];
        list<DSNMapping *>::iterator current = sFlow->mapDSN.begin();
        list<DSNMapping *>::iterator next    = sFlow->mapDSN.begin();

        while( current != sFlow->mapDSN.end() )
        {
            ++next;
            DSNMapping *ptrDSN = *current;

                NS_LOG_DEBUG("ptrDSN->subflowSeqNumber ("<<ptrDSN->subflowSeqNumber<<") sFlow->highestAck ("<<sFlow->highestAck<<")");
            if ( (ackedSeg.find(ptrDSN->dataSeqNumber) == ackedSeg.end()) && (ptrDSN->subflowSeqNumber == sFlow->highestAck + 1) )
            {
                NS_LOG_DEBUG("updating notAckedPkt");
                // that's the first segment not already acked (by DSACK) in the current subflow
                if (notAckedPkt == 0)
                {
                    notAckedPkt = ptrDSN;
                    originalSFlow  = sFlow;
                }else if(notAckedPkt->dataSeqNumber > ptrDSN->dataSeqNumber)
                {
                    if(lastRetransmit == 0)
                    {
                        lastRetransmit = ptrDSN;
                        notAckedPkt    = ptrDSN;
                        originalSFlow  = sFlow;
                    }else if(lastRetransmit->dataSeqNumber < ptrDSN->dataSeqNumber)
                    {
                        lastRetransmit = ptrDSN;
                        notAckedPkt    = ptrDSN;
                        originalSFlow  = sFlow;
                    }
                }
            }
            current = next;
        }
    }

    if( (retransSeg.find(leftEdge) != retransSeg.end()) && (ackedSeg.find(leftEdge) != ackedSeg.end()) && (ackedSeg[leftEdge] > 1) )
    {
  /**
  * if the segment reported in DSACK has been retransmitted and it's acked more than once (duplicated)
  * spurious congestion is detected, set the variables needed to a slow start */
        originalSFlow->phase = DSACK_SS;
        NS_LOG_WARN ("A Spurious Retransmission detected => trigger a slow start to the previous saved cwnd value!");
    }else
    {
        if(notAckedPkt != 0)
            DupAck (originalSFlow->routeId, notAckedPkt);
    }
}

void
MpTcpSocketBase::DupAck (uint8_t sFlowIdx, DSNMapping * ptrDSN)
{
	NS_LOG_FUNCTION_NOARGS ();
    MpTcpSubFlow *sFlow = subflows[ sFlowIdx ];
    ptrDSN->dupAckCount++;
    if ( ptrDSN->dupAckCount == 3 )
    {
        NS_LOG_WARN (Simulator::Now().GetSeconds() <<" DupAck -> Subflow ("<< (int)sFlowIdx <<") 3rd duplicated ACK for segment ("<<ptrDSN->subflowSeqNumber<<")");

        sFlow->rtt->pktRetransmit (SequenceNumber32(ptrDSN->subflowSeqNumber)); // notify the RTT
        //sFlow->rtt->SentSeq (ptrDSN->subflowSeqNumber, ptrDSN->dataLevelLength);
/*for(int i=0;i<100;i++){
           if(int(sFlow->dPort-5000)==i){
               retransmit[i]++;
std::cout<<"data: "<<i<<"  fastRetransmit: "<<retransmit[i]<<std::endl;
           }
        }*/

        for(uint8_t i=0;i<subflows.size();i++){
     if(sFlowIdx==i){
        retransmit[i]++;
        std::cout<<"3 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" retransmit: "<<retransmit[i]<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
     }
}

        reduceCWND (sFlowIdx);
//std::cout<<"Dupack flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
        SetReTxTimeout (sFlowIdx); // reset RTO

        // ptrDSN->dupAckCount   = 0;
        // we retransmit only one lost pkt
        Ptr<Packet> pkt = new Packet (ptrDSN->packet, ptrDSN->dataLevelLength);
        MpTcpHeader header;
        header.SetSourcePort      (sFlow->sPort);
        header.SetDestinationPort (sFlow->dPort);
        header.SetFlags           (TcpHeader::ACK);
        header.SetSequenceNumber  (SequenceNumber32(ptrDSN->subflowSeqNumber));
        header.SetAckNumber       (SequenceNumber32(sFlow->RxSeqNumber));       // for the acknowledgement, we ack the sFlow last received data
        header.SetWindowSize      (AdvertisedWindowSize());
        // save the seq number of the sent data
        uint8_t hlen = 5;
        uint8_t olen = 15;
        uint8_t plen = 0;

        header.AddOptDSN (OPT_DSN, ptrDSN->dataSeqNumber, ptrDSN->dataLevelLength, ptrDSN->subflowSeqNumber);

        NS_LOG_WARN (Simulator::Now().GetSeconds() <<" RetransmitSegment -> "<< " m_localToken "<< m_localToken<<" Subflow "<<(int) sFlowIdx<<" DataSeq "<< ptrDSN->dataSeqNumber <<" SubflowSeq " << ptrDSN->subflowSeqNumber <<" dataLength " << ptrDSN->dataLevelLength << " packet size " << pkt->GetSize() << " 3DupACK");

        switch ( m_algoPR )
        {
            case Eifel:
                if(ptrDSN->retransmited == false)
                {
                    ptrDSN->retransmited = true;
                    ptrDSN->tsval = Simulator::Now ().GetMilliSeconds (); // update timestamp value to the current time
                }
                header.AddOptTT  (OPT_TT, ptrDSN->tsval, 0);
                olen += 17;
                break;
            case D_SACK:
                if(ptrDSN->retransmited == false)
                {
                    ptrDSN->retransmited = true;
                    retransSeg[ptrDSN->dataSeqNumber] = ptrDSN->dataLevelLength;
                }
                break;
            default:
                break;
        }

        plen = (4 - (olen % 4)) % 4;
        olen = (olen + plen) / 4;
        hlen += olen;
        header.SetLength(hlen);
        header.SetOptionsLength(olen);
        header.SetPaddingLength(plen);
        m_mptcp->SendPacket (pkt, header, sFlow->sAddr, sFlow->dAddr);
        // Notify the application of the data being sent

    }else if ( ptrDSN->dupAckCount > 3 )
    {
    }
    NS_LOG_LOGIC ("leaving DupAck");
}

void
MpTcpSocketBase::GenerateRTTPlot ()
{
	NS_LOG_FUNCTION_NOARGS ();
    //NS_LOG_FUNCTION_NOARGS ();

    if ( subflows[0]->measuredRTT.size() == 0)
        return;

    std::ofstream outfile ("rtt-cdf.plt");

    Gnuplot rttGraph = Gnuplot ("rtt-cdf.png", "RTT Cumulative Distribution Function");
    rttGraph.SetLegend("RTT (s)", "CDF");
    rttGraph.SetTerminal ("png");//postscript eps color enh \"Times-BoldItalic\"");
    rttGraph.SetExtra  ("set yrange [0:1.5]");

    for(uint16_t idx = 0; idx < subflows.size(); idx++)
    {
        MpTcpSubFlow * sFlow = subflows[idx];
        Time rtt = sFlow->rtt->GetCurrentEstimate ();
        NS_LOG_LOGIC("saddr = " << sFlow->sAddr << ", dAddr = " << sFlow->dAddr);
        double cdf      = 0.0;
        int    dupCount = 1;
        int    totCount = sFlow->measuredRTT.size();

        if (totCount == 0)
            continue;

        NS_LOG_LOGIC("Estimated RTT for subflow[ "<<idx<<" ] = " << rtt.GetMilliSeconds() << " ms");
        Gnuplot2dDataset dataSet;
        dataSet.SetStyle (Gnuplot2dDataset::LINES_POINTS);
        std::stringstream title;
        title << "Subflow " << idx;
        dataSet.SetTitle (title.str());

        multiset<double>::iterator it = sFlow->measuredRTT.begin();
        //list<double>::iterator it = sFlow->measuredRTT.begin();
        double previous = *it;

        for (it++; it != sFlow->measuredRTT.end(); it++)
        {
            NS_LOG_LOGIC("MpTcpSocketBase::GenerateRTTPlot -> rtt["<<idx<<"] = "<< previous);
            if( previous == *it )
            {
                dupCount++;
            }else
            {
                cdf += (double) dupCount / (double) totCount;
                dataSet.Add (previous, cdf);
                dupCount = 1;
                previous = *it;
            }
        }
        cdf += (double) dupCount / (double) totCount;
        dataSet.Add (previous, cdf);

        rttGraph.AddDataset (dataSet);
    }
    //rttGraph.SetTerminal ("postscript eps color enh \"Times-BoldItalic\"");
    rttGraph.GenerateOutput (outfile);
    outfile.close();
}

bool
MpTcpSocketBase::StoreUnOrderedData (DSNMapping *ptr1)
{
	NS_LOG_FUNCTION_NOARGS ();
    //NS_LOG_FUNCTION (this);
    /**
    * return the statement depending on successfully inserting or not the data
    * if unOrdered buffer can't hold the out of sequence data and currently received
    */
    bool inserted = false;
    for(list<DSNMapping *>::iterator it = unOrdered.begin(); it != unOrdered.end(); ++it)
    {
        DSNMapping *ptr2 = *it;
        if(ptr1->dataSeqNumber == ptr2->dataSeqNumber)
        {
            NS_LOG_INFO ("Data Sequence ("<< ptr1->dataSeqNumber <<") already stored in unOrdered buffer !");
            return false;
        }
        if(ptr1->dataSeqNumber < ptr2->dataSeqNumber)
        {
            unOrdered.insert(it, ptr1);
            inserted = true;
            break;
        }
    }
    if ( !inserted )
        unOrdered.insert (unOrdered.end(), ptr1);

    return true;
}

int
MpTcpSocketBase::Close (void)
{
	NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_LOGIC("MpTcpSocketBase" << m_node->GetId() << "::Close() -> Number of subflows = " << subflows.size());
  // First we check to see if there is any unread rx data
  // Bug number 426 claims we should send reset in this case.

    GenerateRTTPlot();

    NS_LOG_INFO("///////////////////////////////////////////////////////////////////////////////");
    NS_LOG_INFO("Closing subflows");
    for(uint16_t idx = 0; idx < subflows.size(); idx++)
    {
        if( subflows[idx]->state != CLOSED )
        {
            NS_LOG_INFO("Subflow " << idx);
            NS_LOG_INFO("TxSeqNumber = " << subflows[idx]->TxSeqNumber);
            NS_LOG_INFO("RxSeqNumber = " << subflows[idx]->RxSeqNumber);
            NS_LOG_INFO("highestAck  = " << subflows[idx]->highestAck);
            NS_LOG_INFO("maxSeqNb    = " << subflows[idx]->maxSeqNb);
            ProcessAction (idx, ProcessEvent (idx, APP_CLOSE) );
        }
    }
    NS_LOG_INFO("///////////////////////////////////////////////////////////////////////////////");
  return 0;
}

bool
MpTcpSocketBase::CloseMultipathConnection ()
{
    NS_LOG_FUNCTION_NOARGS();
    bool closed  = false;
    uint32_t cpt = 0;
    for(uint32_t i = 0; i < subflows.size(); i++)
    {
        NS_LOG_LOGIC("Subflow (" << i << ") TxSeqNumber (" << subflows[i]->TxSeqNumber << ") RxSeqNumber = " << subflows[i]->RxSeqNumber);
        NS_LOG_LOGIC("highestAck (" << subflows[i]->highestAck << ") maxSeqNb    = " << subflows[i]->maxSeqNb);

        if( subflows[i]->state == CLOSED )
            cpt++;
    }
    if( cpt == subflows.size() )
        NotifyNormalClose();
    return closed;
}

bool
MpTcpSocketBase::isMultipath ()
{
	NS_LOG_FUNCTION_NOARGS ();
    return m_mpEnabled;
}

void
MpTcpSocketBase::AdvertiseAvailableAddresses ()
{

    NS_LOG_FUNCTION_NOARGS();
  if(m_mpEnabled == true)
  {
    // there is at least one subflow
    MpTcpSubFlow * sFlow = subflows[0];
    m_mpSendState = MP_ADDR;
    MpTcpAddressInfo * addrInfo;
    Ptr<Packet> pkt = new Packet(0);//Create<Packet> ();
    MpTcpHeader header;
    header.SetFlags           (TcpHeader::ACK);//flags);
    header.SetSequenceNumber  (SequenceNumber32(sFlow->TxSeqNumber));
    header.SetAckNumber       (SequenceNumber32(sFlow->RxSeqNumber));
    header.SetSourcePort      (m_endPoint->GetLocalPort ());
    header.SetDestinationPort (m_remotePort);

    uint8_t hlen = 0;
    uint8_t olen = 0;

    Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject<Ipv4L3Protocol> ();

    for(uint32_t i = 0; i < ipv4->GetNInterfaces(); i++)
    {
        //Ptr<NetDevice> device = m_node->GetDevice(i);
        Ptr<Ipv4Interface> interface = ipv4->GetInterface(i);
        Ipv4InterfaceAddress interfaceAddr = interface->GetAddress (0);
        // do not consider loopback addresses
        if(interfaceAddr.GetLocal() == Ipv4Address::GetLoopback())
            continue;

        addrInfo = new MpTcpAddressInfo();
        addrInfo->addrID   = i;
        addrInfo->ipv4Addr = interfaceAddr.GetLocal();
        addrInfo->mask     = interfaceAddr.GetMask ();

        //addrInfo->ipv4Addr = Ipv4Address::ConvertFrom(device->GetAddress());
//NS_LOG_INFO("MpTcpSocketBase::AdvertiseAvailableAddresses -> Ipv4 addresse = "<< addrInfo->ipv4Addr);

        header.AddOptADDR(OPT_ADDR, addrInfo->addrID, addrInfo->ipv4Addr);
        olen += 6;
        localAddrs.insert(localAddrs.end(), addrInfo);
    }
    uint8_t plen = (4 - (olen % 4)) % 4;
//NS_LOG_INFO("MpTcpSocketBase::AdvertiseAvailableAddresses -> number of addresses " << localAddrs.size());
    header.SetWindowSize (AdvertisedWindowSize());
    // urgent pointer
    // check sum filed
    olen = (olen + plen) / 4;
    hlen = 5 + olen;
    header.SetLength(hlen);
    header.SetOptionsLength(olen);
    header.SetPaddingLength(plen);

    //SetReTxTimeout (0);

    m_mptcp->SendPacket (pkt, header, m_endPoint->GetLocalAddress (), m_remoteAddress);
    sFlow->TxSeqNumber ++;
    sFlow->maxSeqNb++;
  }
}

uint32_t
MpTcpSocketBase::GetOutputInf (Ipv4Address addr)
{
	NS_LOG_FUNCTION_NOARGS ();
    uint32_t oif = 0;
    Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject<Ipv4L3Protocol> ();
    for(uint32_t i = 0; i < localAddrs.size(); i++)
    {
        MpTcpAddressInfo* inf = localAddrs[i];

        if(addr == inf->ipv4Addr)
        {
            oif = inf->addrID;
            break;
        }
    }

    return oif;
}

bool
MpTcpSocketBase::IsThereRoute (Ipv4Address src, Ipv4Address dst)
{
  //NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_FUNCTION(this << src << dst);
  bool found = false;
  uint32_t interface=0;
  // Look up the source address
  Ptr<Ipv4> ipv4 = m_node->GetObject<Ipv4> ();

  NS_ASSERT (ipv4->GetRoutingProtocol () != 0); //Pablo UC

  Ipv4Header l3Header;
  Socket::SocketErrno errno_;
  Ptr<Ipv4Route> route;


  interface=ipv4->GetInterfaceForAddress(src);      //Pablo UC
  Ptr<NetDevice> oif=ipv4->GetNetDevice(interface); //Pablo UC
                              //Pablo UC


//        Ptr<NetDevice> oif;
//        oif->SetIfIndex(GetOutputInf (src)); //specify non-zero if bound to a source address
        //uint32_t temp=GetOutputInf(src);
        //Ptr<NetDevice> oif(0);
        //if (temp==0)
        //{
        //}else
        	//NS_LOG_UNCOND("oif distinto de cero");
//        Ptr<NetDevice> temp = CreateObject <NetDevice> ();


  l3Header.SetSource (src);
  l3Header.SetDestination (dst);
  route = ipv4->GetRoutingProtocol ()->RouteOutput (Ptr<Packet> (), l3Header, oif, errno_);

  if ((route != 0) && (src == route->GetSource ()))
  {
    NS_LOG_INFO ("IsThereRoute -> Route from srcAddr "<< src << " to dstAddr " << dst << " oit "<<oif<<", exist !");
    found = true;
  }else
    NS_LOG_INFO ("IsThereRoute -> No Route from srcAddr "<< src << " to dstAddr " << dst << " oit "<<oif<<", exist !");

  return found;
}

bool
MpTcpSocketBase::IsLocalAddress (Ipv4Address addr)
{
    NS_LOG_FUNCTION(this << addr);
    bool found = false;
    MpTcpAddressInfo * pAddrInfo;
    for(uint32_t i = 0; i < localAddrs.size(); i++)
    {
        pAddrInfo = localAddrs[i];
        if( pAddrInfo->ipv4Addr == addr)
        {
            found = true;
            break;
        }
    }
    return found;
}

void
MpTcpSocketBase::DetectLocalAddresses ()
{
    NS_LOG_FUNCTION_NOARGS();
    MpTcpAddressInfo * addrInfo;
    Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject<Ipv4L3Protocol> ();

    for(uint32_t i = 0; i < ipv4->GetNInterfaces(); i++)
    {
        //Ptr<NetDevice> device = m_node->GetDevice(i);
        Ptr<Ipv4Interface> interface = ipv4->GetInterface(i);
        Ipv4InterfaceAddress interfaceAddr = interface->GetAddress (0);
        // do not consider loopback addresses
        if( (interfaceAddr.GetLocal() == Ipv4Address::GetLoopback()) || (IsLocalAddress(interfaceAddr.GetLocal())) )
            continue;

        addrInfo = new MpTcpAddressInfo();
        addrInfo->addrID   = i;
        addrInfo->ipv4Addr = interfaceAddr.GetLocal();
        addrInfo->mask     = interfaceAddr.GetMask ();

        localAddrs.insert(localAddrs.end(), addrInfo);
    }
}

uint32_t
MpTcpSocketBase::BytesInFlight ()
{

  NS_LOG_FUNCTION_NOARGS ();
  return unAckedDataCount; //m_highTxMark - m_highestRxAck;
}

uint16_t
MpTcpSocketBase::AdvertisedWindowSize ()
{
    NS_LOG_FUNCTION_NOARGS();
    uint16_t window = 0;
/*
    if( recvingBuffer != 0 )
        window = recvingBuffer->FreeSpaceSize ();
*/
    window = 65535;
    return window;
}

uint32_t
MpTcpSocketBase::AvailableWindow (uint8_t sFlowIdx)
{
  NS_LOG_FUNCTION_NOARGS ();
  MpTcpSubFlow * sFlow = subflows[sFlowIdx];
  uint32_t window   = std::min ( remoteRecvWnd, static_cast<uint32_t> (sFlow->cwnd) ) * sFlow->MSS;
  uint32_t unAcked  = sFlow->maxSeqNb - sFlow->highestAck;
  NS_LOG_LOGIC("Subflow ("<< (int)sFlowIdx <<"): AvailableWindow -> remoteRecvWnd  = " << remoteRecvWnd <<" unAckedDataCnt = " << unAcked <<" CWND in bytes  = " << sFlow->cwnd * sFlow->MSS);
  if (window < unAcked) //DataCount)
  {
      NS_LOG_LOGIC("MpTcpSocketBase::AvailableWindow -> Available Tx window is 0");
      return 0;  // No space available
  }
  return (window - unAcked);//DataCount);       // Amount of window space available
}

uint32_t
MpTcpSocketBase::GetTxAvailable ()
{
    NS_LOG_FUNCTION_NOARGS();
    //NS_LOG_INFO ("sendingBuffer->FreeSpaceSize () == " << sendingBuffer->FreeSpaceSize ());
    return sendingBuffer->FreeSpaceSize ();
}

void
MpTcpSocketBase::SetSourceAddress (Ipv4Address src)
{
    NS_LOG_FUNCTION_NOARGS();
    m_localAddress = src;
    if(m_endPoint != 0)
    {
        m_endPoint->SetLocalAddress(src);
    }
}

Ipv4Address
MpTcpSocketBase::GetSourceAddress ()
{
    NS_LOG_FUNCTION_NOARGS();
    return m_localAddress;
}

uint8_t
MpTcpSocketBase::LookupByAddrs (Ipv4Address src, Ipv4Address dst)
{
    NS_LOG_FUNCTION_NOARGS();
    MpTcpSubFlow *sFlow = 0;
    uint8_t sFlowIdx = m_maxSubFlowNumber;

    if( IsThereRoute (src, dst)==false )
    {
        // there is problem in the stated src (local) address
        for(vector<MpTcpAddressInfo *>::iterator it=localAddrs.begin(); it!=localAddrs.end(); ++it)
        {
            Ipv4Address ipv4Addr = (*it)->ipv4Addr;
            if( IsThereRoute (ipv4Addr, dst)==true )
            {
                src = ipv4Addr;
                m_localAddress  = ipv4Addr;
                break;
            }
        }
    }

    for(uint8_t i = 0; i < subflows.size(); i++)
    {
        sFlow = subflows[i];
        // on address can only participate to a one subflow, so we can find that subflow by unsing the source address or the destination, but the destination address is the correct one, so use it
        if( (sFlow->sAddr==src && sFlow->dAddr==dst) || (sFlow->dAddr==dst) )
        {
            sFlowIdx = i;
            break;
        }
    }

    if(! (sFlowIdx < m_maxSubFlowNumber) )
    {
      if(m_connected == false && subflows.size()==1)
      {
          sFlowIdx = 0;
      }
      else
      {
          if( IsLocalAddress(m_localAddress) )
          {
                sFlowIdx = subflows.size();
                MpTcpSubFlow *sFlow = new MpTcpSubFlow( sfInitSeqNb[m_localAddress] + 1);
                sFlow->routeId   = subflows[subflows.size() - 1]->routeId + 1;
                sFlow->dAddr     = m_remoteAddress;
                sFlow->dPort     = m_remotePort;
                sFlow->sAddr     = m_localAddress;
                sFlow->sPort     = m_localPort;
               // sFlow->MSS       = getL3MTU(m_localAddress);
                sFlow->MSS       = 536;
                sFlow->bandwidth = getBandwidth(m_endPoint->GetLocalAddress ());
                // at its creation, the subflow take the state of the global connection
                if(m_state == LISTEN)
                    sFlow->state = m_state;
                else if(m_state == ESTABLISHED)
                    sFlow->state = SYN_SENT;
                subflows.insert(subflows.end(), sFlow);

//std::cout<<"i am z "<<" routeId "<<sFlow->routeId<<" subflow.size: "<<subflows.size()<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
                NS_LOG_INFO("Node ("<<m_node->GetId()<<") LookupByAddrs -> sFlowIdx " << (int) sFlowIdx <<" created: (src,dst) = (" << sFlow->sAddr << "," << sFlow->dAddr << ")" );
          }else
          {
                NS_LOG_WARN ("MpTcpSocketBase::LookupByAddrs -> sub flow related to (src,dst) = ("<<m_endPoint->GetLocalAddress()<<","<<m_endPoint->GetPeerAddress()<<") not found !");
          }
      }
    }

    NS_LOG_INFO("Node ("<<m_node->GetId()<<") LookupByAddrs -> subflows number = " << subflows.size() <<" (src,dst) = (" << src << "," << dst << ") below to subflow " << (int) sFlowIdx );

    return sFlowIdx;
}

void
MpTcpSocketBase::OpenCWND (uint8_t sFlowIdx, uint32_t ackedBytes)
{
    NS_LOG_FUNCTION(this << (int) sFlowIdx << ackedBytes);
    MpTcpSubFlow * sFlow = subflows[sFlowIdx];
    double   increment = 0;
    double   cwnd      = sFlow->cwnd;
    uint32_t ssthresh  = sFlow->ssthresh;
    uint32_t segSize   = sFlow->MSS;
    bool     normalCC  = true;
//std::cout<<"sFlow->ssthresh: "<<sFlow->ssthresh<<std::endl;
//std::cout<<"open sFlow->phase: "<<sFlow->phase<<std::endl;
    if ( sFlow->phase == DSACK_SS )
    {
//std::cout<<"sflow->phase=DSACK_SS"<<std::endl;
        if( cwnd + 1 < sFlow->savedCWND )
        {std::cout<<"4 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<"segSize"<<segSize<<"ssthresh"<<ssthresh<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
            increment = 1;
            normalCC  = false;
            NS_LOG_WARN ("Subflow ("<< (int) sFlowIdx <<") Congestion Control (DSACK Slow Start) increment is 1 to reach "<< sFlow->savedCWND );
        }else
        {
            NS_LOG_WARN ("End of DSACK phase in subflow ("<< (int) sFlowIdx <<") Congestion Control (DSACK Slow Start) reached "<< sFlow->savedCWND );
            sFlow->savedCWND = 0;
            sFlow->phase = Congestion_Avoidance;
        }
    }else if( (sFlow->phase == RTO_Recovery) && (cwnd * segSize < ssthresh) )
    {std::cout<<"4 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<"segSize"<<segSize<<"ssthresh"<<ssthresh<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
        increment = 1;
        normalCC  = false;
        NS_LOG_WARN (Simulator::Now().GetSeconds() <<" Subflow ("<< (int) sFlowIdx <<") Congestion Control (Slow Start Recovery) increment is 1 current cwnd "<< cwnd );
    }
    if (normalCC == true)
    {
    if( cwnd * segSize < ssthresh )
    {std::cout<<"4 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<"segSize"<<segSize<<"ssthresh"<<ssthresh<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
        increment = 1;
        NS_LOG_ERROR ("Congestion Control (Slow Start) increment is 1");
    }else if( totalCwnd != 0 )
    {std::cout<<"5 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<"segSize"<<segSize<<"ssthresh"<<ssthresh<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
        switch ( m_algoCC )
        {
            case RTT_Compensator:
                //increment = std::min( alpha * ackedBytes / totalCwnd, (double) ackedBytes / cwnd );
                calculateSmoothedCWND (sFlowIdx);
                calculate_alpha();
                increment = std::min( alpha / totalCwnd, 1.0 / cwnd );
                NS_LOG_ERROR ("Congestion Control (RTT_Compensator): alpha "<<alpha<<" ackedBytes (" << ackedBytes << ") totalCwnd ("<< totalCwnd<<") -> increment is "<<increment);
                break;

            case Linked_Increases:
                calculateSmoothedCWND (sFlowIdx);
                calculate_alpha();
                increment = alpha / totalCwnd;
                NS_LOG_ERROR ("Subflow "<<(int)sFlowIdx<<" Congestion Control (Linked_Increases): alpha "<<alpha<<" increment is "<<increment<<" ssthresh "<< ssthresh << " cwnd "<<cwnd );
                break;

            case Uncoupled_TCPs:
                increment = 1.0 / cwnd;
                NS_LOG_ERROR ("Subflow "<<(int)sFlowIdx<<" Congestion Control (Uncoupled_TCPs) increment is "<<increment<<" ssthresh "<< ssthresh << " cwnd "<<cwnd);
                break;

            case Fully_Coupled :
                increment = 1.0 / totalCwnd;
                NS_LOG_ERROR ("Subflow "<<(int)sFlowIdx<<" Congestion Control (Fully_Coupled) increment is "<<increment<<" ssthresh "<< ssthresh << " cwnd "<<cwnd);
                break;

            default :
                increment = 1.0 / cwnd;
                break;
        }
    }else
    {std::cout<<"5 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<"segSize"<<segSize<<"ssthresh"<<ssthresh<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
        increment = 1 / cwnd;
        NS_LOG_ERROR ("Congestion Control (totalCwnd == 0) increment is "<<increment);
    }
    }
    if (totalCwnd + increment <= remoteRecvWnd)
        sFlow->cwnd += increment;
    double rtt = sFlow->rtt->GetCurrentEstimate().GetSeconds();
    NS_LOG_WARN (Simulator::Now().GetSeconds() <<" MpTcpSocketBase -> "<< " m_localToken "
    		<< m_localToken<<" Subflow "<< (int)sFlowIdx <<": RTT "<< sFlow->rtt->GetCurrentEstimate().GetSeconds()
    		<<" Moving cwnd from " << cwnd << " to " << sFlow->cwnd <<" Throughput "<<(sFlow->cwnd * sFlow->MSS * 8)/rtt
    		<< " GlobalThroughput "<<getGlobalThroughput()<< " Efficacity " <<  getConnectionEfficiency() << " delay "
    		<<getPathDelay(sFlowIdx)<<" alpha "<< alpha <<" Sum CWND ("<< totalCwnd <<")");
     /*if(int(sFlow->dPort-5000)==12 and (int)sFlowIdx==0){
        std::cout<<"flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
    }*/
std::cout<<"1 flow: "<<int(sFlow->dPort)-5000<<" sFlowIdx: "<<(int)sFlowIdx<<" cwnd: "<<sFlow->cwnd<<" seconds: "<<Simulator::Now().GetSeconds()<<std::endl;
}

void
MpTcpSocketBase::calculate_alpha ()
{
    // this method is called whenever a congestion happen in order to regulate the agressivety of subflows
   NS_LOG_FUNCTION_NOARGS ();
   meanTotalCwnd = totalCwnd = alpha = 0;
   double maxi       = 0;
   double sumi       = 0;

   for (uint32_t i = 0; i < subflows.size() ; i++)
   {
       MpTcpSubFlow * sFlow = subflows[i];

       totalCwnd += sFlow->cwnd;
       meanTotalCwnd += sFlow->scwnd;

     /* use smmothed RTT */
     Time time = sFlow->rtt->GetCurrentEstimate ();
     double rtt = time.GetSeconds ();
     if (rtt < 0.000001)
       continue;                 // too small

     double tmpi = sFlow->scwnd / (rtt * rtt);
     if (maxi < tmpi)
       maxi = tmpi;

     sumi += sFlow->scwnd / rtt;
   }
   if (!sumi)
     return;
   alpha = meanTotalCwnd * maxi / (sumi * sumi);
   NS_LOG_ERROR ("calculate_alpha: alpha "<<alpha<<" totalCwnd ("<< meanTotalCwnd<<")");
}

void
MpTcpSocketBase::calculateSmoothedCWND (uint8_t sFlowIdx)
{
	NS_LOG_FUNCTION_NOARGS ();
    MpTcpSubFlow *sFlow = subflows [sFlowIdx];
    if (sFlow->scwnd < 1)
        sFlow->scwnd = sFlow->cwnd;
    else
        sFlow->scwnd = sFlow->scwnd * 0.875 + sFlow->cwnd * 0.125;
}

void
MpTcpSocketBase::Destroy (void)
{
    NS_LOG_FUNCTION_NOARGS();
}

MpTcpSubFlow *
MpTcpSocketBase::GetSubflow (uint8_t sFlowIdx)
{
	NS_LOG_FUNCTION_NOARGS ();
    return subflows [sFlowIdx];
}

void
MpTcpSocketBase::SetCongestionCtrlAlgo (CongestionCtrl_t ccalgo)
{
	NS_LOG_FUNCTION_NOARGS ();
    m_algoCC = ccalgo;
}

void
MpTcpSocketBase::SetDataDistribAlgo (DataDistribAlgo_t ddalgo)
{
	NS_LOG_FUNCTION_NOARGS ();
    m_distribAlgo = ddalgo;
}

bool
MpTcpSocketBase::rejectPacket(double threshold)
{
    NS_LOG_FUNCTION_NOARGS();

    bool reject = false;
    double probability = (double) (rand() % 1013) / 1013.0;
    NS_LOG_INFO("rejectPacket -> probability == " << probability);
    if( probability < threshold )
        reject = true;

    return reject;

}

double
MpTcpSocketBase::getPathDelay(uint8_t idxPath)
{
	NS_LOG_FUNCTION_NOARGS ();
    TimeValue delay;
    Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject<Ipv4L3Protocol> ();
    // interface 0 is the loopback interface
    Ptr<Ipv4Interface> interface = ipv4->GetInterface(idxPath + 1);
    Ipv4InterfaceAddress interfaceAddr = interface->GetAddress (0);
    // do not consider loopback addresses
    if(interfaceAddr.GetLocal() == Ipv4Address::GetLoopback())
        return 0.0;
    Ptr<NetDevice> netDev =  interface->GetDevice();
    Ptr<Channel> P2Plink  =  netDev->GetChannel();
    Ptr<PointToPointChannel> temp = CreateObject <PointToPointChannel> ();

    if (P2Plink->GetInstanceTypeId() ==  temp->GetInstanceTypeId())
    	P2Plink->GetAttribute(string("Delay"), delay);
    else
    	delay=Time(0);
    return delay.Get().GetSeconds();
}

uint64_t
MpTcpSocketBase::getPathBandwidth(uint8_t idxPath)
{
	NS_LOG_FUNCTION_NOARGS ();
    StringValue str;
    Ptr<Ipv4L3Protocol> ipv4 = m_node->GetObject<Ipv4L3Protocol> ();
    // interface 0 is the loopback interface
    Ptr<Ipv4Interface> interface = ipv4->GetInterface(idxPath + 1);
    Ipv4InterfaceAddress interfaceAddr = interface->GetAddress (0);
    // do not consider loopback addresses
    if(interfaceAddr.GetLocal() == Ipv4Address::GetLoopback())
        return 0.0;
    Ptr<NetDevice> netDev =  interface->GetDevice();

    if( netDev->IsPointToPoint() == true )
    {
       netDev->GetAttribute(string("DataRate"), str);
    }else
    {
        //Ptr<Channel> link  =  netDev->GetChannel();
        //link->GetAttribute(string("DataRate"), str);
        str=string("0");
    }

    DataRate bandwidth (str.Get());
    return bandwidth.GetBitRate ();
}

double
MpTcpSocketBase::getGlobalThroughput()
{
	NS_LOG_FUNCTION_NOARGS ();
    double gThroughput = 0;
    for(uint32_t i=0; i< subflows.size(); i++)
    {
        MpTcpSubFlow* sFlow = subflows[i];
        gThroughput += (sFlow->cwnd * sFlow->MSS * 8) / sFlow->rtt->GetCurrentEstimate().GetSeconds();
    }
    return gThroughput;
}

double
MpTcpSocketBase::getConnectionEfficiency()
{
	NS_LOG_FUNCTION_NOARGS ();
    double gThroughput =0.0;
    uint64_t gBandwidth = 0;
    for(uint32_t i = 0; i < subflows.size(); i++)
    {
        MpTcpSubFlow* sFlow = subflows[i];
        gThroughput += (sFlow->cwnd * sFlow->MSS * 8) / sFlow->rtt->GetCurrentEstimate().GetSeconds();

        gBandwidth += getPathBandwidth(i);
    }
    return gThroughput / gBandwidth;
}

DSNMapping*
MpTcpSocketBase::getAckedSegment(uint64_t lEdge, uint64_t rEdge)
{
	NS_LOG_FUNCTION_NOARGS ();
    for(uint8_t i = 0; i < subflows.size(); i++)
    {
        MpTcpSubFlow* sFlow = subflows[i];
        for (list<DSNMapping *>::iterator it = sFlow->mapDSN.begin(); it != sFlow->mapDSN.end(); ++it)
        {
            DSNMapping* dsn = *it;
            if(dsn->dataSeqNumber == lEdge && dsn->dataSeqNumber + dsn->dataLevelLength == rEdge)
            {
                return dsn;
            }
        }
    }
    return 0;
}

//Clases Añadidas por ser Virtuales en tcp-socket-base

Ptr<TcpSocketBase> MpTcpSocketBase::Fork (void)
{
	NS_LOG_FUNCTION_NOARGS ();

  return CopyObject<MpTcpSocketBase> (this);
}

void MpTcpSocketBase::SetSSThresh (uint32_t threshold)
{
	NS_LOG_FUNCTION_NOARGS ();
  m_ssThresh = threshold;
}

uint32_t MpTcpSocketBase::GetSSThresh (void) const
{
	NS_LOG_FUNCTION_NOARGS ();
  return m_ssThresh;
}

void
MpTcpSocketBase::SetInitialCwnd (uint32_t cwnd)
{
	NS_LOG_FUNCTION_NOARGS ();
  NS_ABORT_MSG_UNLESS (m_state == CLOSED, "TcpNewReno::SetInitialCwnd() cannot change initial cwnd after connection started.");
  m_initialCWnd = cwnd;
}

uint32_t
MpTcpSocketBase::GetInitialCwnd (void) const
{
	NS_LOG_FUNCTION_NOARGS ();
  return m_initialCWnd;
}



/*
bool
MpTcpSocketBase::IsRetransmitted (uint64_t leftEdge, uint64_t rightEdge)
{
    bool retransmitted = false;

    for (uint8_t i = 0; i < subflows.size(); i++)
    {
        MpTcpSubFlow *sFlow = subflows[i];
        list<DSNMapping *>::iterator current = sFlow->mapDSN.begin();
        list<DSNMapping *>::iterator next = sFlow->mapDSN.begin();
        while( current != sFlow->mapDSN.end() )
        {
            ++next;
            DSNMapping *ptrDSN = *current;
            if ( (ptrDSN->dataSeqNumber >= leftEdge) && (ptrDSN->dataSeqNumber + ptrDSN->dataLevelLength <= rightEdge) )
            {
                // By checking the data level sequence number in the received TCP header option
                // we can find if the segment has already been retransmitted or not
                retransmitted = ptrDSN->retransmited;
            }
            if ( retransmitted == true )
            {
                NS_LOG_WARN("Segement between seq n°"<< leftEdge <<" and "<< rightEdge <<" retransmitted !");
                break;
            }
            current = next;
        }
    }
    return retransmitted;
}
*/




}//namespace ns3








// Retransmit timeout
//void MpTcpSocketBase::Retransmit (void)
//{
//  NS_LOG_FUNCTION (this);
//  NS_LOG_LOGIC (this << " ReTxTimeout Expired at time " << Simulator::Now ().GetSeconds ());
//  m_inFastRec = false;
//
//  // If erroneous timeout in closed/timed-wait state, just return
//  if (m_state == CLOSED || m_state == TIME_WAIT) return;
//  // If all data are received (non-closing socket and nothing to send), just return
//  if (m_state <= ESTABLISHED && m_txBuffer.HeadSequence () >= m_highTxMark) return;
//
//  // According to RFC2581 sec.3.1, upon RTO, ssthresh is set to half of flight
//  // size and cwnd is set to 1*MSS, then the lost packet is retransmitted and
//  // TCP back to slow start
//  m_ssThresh = std::max (2 * m_segmentSize, BytesInFlight () / 2);
//  m_cWnd = m_segmentSize;
//  m_nextTxSequence = m_txBuffer.HeadSequence (); // Restart from highest Ack
//  NS_LOG_INFO ("RTO. Reset cwnd to " << m_cWnd <<
//               ", ssthresh to " << m_ssThresh << ", restart from seqnum " << m_nextTxSequence);
//  m_rtt->IncreaseMultiplier ();             // Double the next RTO
//  DoRetransmit ();                          // Retransmit the packet
//}
//
//
//void
//MpTcpSocketBase::InitializeCwnd (void)
//{
//  /*
//   * Initialize congestion window, default to 1 MSS (RFC2001, sec.1) and must
//   * not be larger than 2 MSS (RFC2581, sec.3.1). Both m_initiaCWnd and
//   * m_segmentSize are set by the attribute system in ns3::TcpSocket.
//   */
//  m_cWnd = m_initialCWnd * m_segmentSize;
//}
//
//} // namespace ns3
