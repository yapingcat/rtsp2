rtsp2
===========

### 介绍   
rtsp协议栈解析库，当前只解析rtsp(rfc2326)协议，主要是基于[http-parser](https://github.com/yapingcat/http-parser.git)开发(***http-parser 是在nodejs/http-parser的基础上增加了rtsp协议的支持,具体代码修改参见commit***),如果想要开发一个完整的rtsp服务端或者客户
端，你还需要一个网络库(libuv,asio ...) 和一个rtp打包解包的库,

### 支持的rtsp特性

- client/server (支持客户端和服务端协议栈解析)
- play/record (推流和拉流模式)
- basic/digest Authorization
- sdp解析
- transport  
   1. rtp over rtsp(rtsp interleaved)
   2. udp
   3. multicast
- range
   1. npt
   2. utc
   3. 不支持smpte
- 支持rtsp服务端发送request到客户端(目前支持TEARDOWN,OPTIONS,ANNOUNCE,GET_PARAMETER,SET_PARAMETER 5种信令)
- 

### Build
```bash
git clone https://github.com/yapingcat/rtsp2.git
cd rtsp2
git submodule update --init
mkdir cmake-build
cd cmake-build
cmake ..

#编译release版本
make 或者 make release
#编译debug 版本
make debug 
#编译example
make example
#打开sanitizer
make asan
```

### 调用流程  
  
下面举几个例子简单说明rtsp2的使用流程,更具体的使用请参见example下的例子，exmaple目录下有分别使用libuv和asio作为网络库开发的rtsp客户端和服务端例子

1. rtsp拉流客户端
```c++

//step 1
rtsp2::Client client(url); //根据rtsp url 创建一个客户端

//设置各个回调函数,作为拉流客户端，一般下面几个回调函数是必须要设置的
client.setOptionsCB(...);     
client.setDescribeCB(...);   
client.setSetupCB(...); 
client.setPlayCB(...);
client.setTearDownCB(...);
client.setRtpCB(...);
client.setOutputCB(...);      //协议栈输出回调,一般在此回调中，把数据发送到网络中去
client.setAuthFailedCB(...);  //rtsp鉴权错误回调

//step 2
//发送optons,开始rtsp回话
client.optons();

//在option回调中 处理你的业务逻辑
//如果ok，可以发送describe信令，接下来在decribe回调中处理你的业务逻辑
//setup play 也遵循以上的规则
void OnOption(rtsp2::Client& client, const rtsp2::RtspResponse & res)
{  
   if(res.statusCode() != rtsp2::RtspResponse::RTSP_OK)
   {
      //do something
   }
   else
   {
      //do something
      
      .....

      client.describe();
   }
   
}


//step 3
//
//buf是从网络中接收到的数据
//调用input方法，送入协议栈解析
client.input(buf,len)

//如果rtp传输方式是rtp over rtsp的会触发 OnRtp 回调
//media和 sdp中的"m="对应，用于判断是哪个meida的rtp包 一般是"video" 或者 "audio"
void OnRtp(rtsp2::Client& client,const std::string &media, int isRtcp, const uint8_t *, std::size_t len)
{
}

```
2. rtsp推流客户端

```c++
//推流客户端和拉流客户端，流程基本上差不多
//区别在于需要设置一下Record 和 Announce两个回调，play和describe则不需要
rtsp2Client.setOptionsCB();
rtsp2Client.setAnnounceCB();  
rtsp2Client.setSetupCB(); 
rtsp2Client.setRecordCB();
rtsp2Client.setTearDownCB();
rtsp2Client.setRtpCB();
rtsp2Client.setOutputCB();
rtsp2Client.setAuthFailedCB();

//一般在option回调中,发送Announce信令,Announce携带的sdp，需要调用方构建
//如下
void OnOption(rtsp2::Client& client, const rtsp2::RtspResponse & res)
{
   std::string sdp = "v=0\r\n"
                     "o=- 0 0 IN IP4 0.0.0.0\r\n"
                     "c=IN IP4 0.0.0.0\r\n"
                     "t=0 0\r\n"
                     "a=control:*\r\n"
                     "m=video 0 RTP/AVP 96\r\n"
                     "a=rtpmap:96 H264/90000\r\n"
                     "a=control:trackID=0\r\n";   
   client.announce(sdp);
}

//如果你的传输通道是rtp over rtsp
//rtp打包完之后需要调用以下接口
rtsp2Client.pushInterleavedBinaryData(interleaved,pkg,length);
```

3. rtsp 拉流服务端
```c++

// step1 你需要定一个rtsp服务端回话类，继承ServerHandle
class Session : public rtsp2::ServerHandle

// step2 根据你的需求,实现对应的虚函数
// 服务端需要创建session id, 必须要实现createSessionId()接口
// 必须实现 send(const std::string& msg)接口,在这个接口中你可以把rtsp/rtcp/rtp数据发向网络
// 你是一个拉流服务端，所以一般情况下你需要实现如下几个处理信令的接口
// handleOption handleDescribe handleSetup handlePlay handlePause handleTearDown handleGetParmeters
// 如果你对rtcp包感兴趣，你需要实现handleRtp，用于处理rtcp包
// 如果你需要鉴权认证 那么 1. needAuth() 返回 true 2.getAuthParam() 返回鉴权需要的认证信息，用户名密码 realm等
// 服务端也可以发送request到客户端,调用 sendRtspMessage即可
// 如果是rtp over rtsp这个模式,需要调用sendRtpRtcp接口封装interleaved 4字节头
class Session : public rtsp2::ServerHandle
{
protected:
   void handleOption(const RtspRequest&,RtspResponse& )
   {
      //你需要设置public 字段
      res[RtspMessage::Public] = "....." 
   }

   void handleDescribe(const RtspRequest&,RtspResponse& res)
   {
      //设置你的sdp信息
      res[RtspMessage::ContentType] = "application/sdp";
      std::string sdp = "v=0\r\n"
                        "o=- 0 0 IN IP4 0.0.0.0\r\n"
                        "c=IN IP4 0.0.0.0\r\n"
                        "t=0 0\r\n"
                        "a=control:*\r\n"
                        "m=video 0 RTP/AVP 96\r\n"
                        "a=rtpmap:96 H264/90000\r\n"
                        "a=control:trackID=0\r\n";   
      res.addBody(sdp);
   }

   void handleSetup(const RtspRequest&,TransportV1& , RtspResponse&)
   {
      //协商传输通道，设置通道参数
      ....set transport parameter
      //如果不支持request携带的通道参数
      //设置RTSP_Unsupported_Transport 状态码
      res.setStatusCodeAndReason(RtspResponse::RTSP_Unsupported_Transport);
   }

   void handlePlay(const RtspRequest&,RtspResponse&)
   {
      //可以开始推流了
   }

   void handlePause(const RtspRequest&,RtspResponse&)
   {

   }

   void handleTearDown(const RtspRequest&,RtspResponse&)
   {
      //回话结束
   }

   void handleGetParmeters(const RtspRequest&,RtspResponse&)
   {
      //GetParmeters方法一般用于保活
   }

   void handleRtp(int channel,const uint8_t, std::size_t)
   {
      //一般你接收到是rtcp rr包,

   }

   void send(const std::string& msg)
   {
      //send msg to network
   }

   

private:
   std::string createSessionId()
   {
      //生成session id
      //第一次setup请求到来时会调用该方法
   }

   //如果你需要鉴权
   bool needAuth() { return true;}
   Authenticate getAuthParam() 
   { 
      Authenticate auth;
      auth.username = "test";
      auth.password = "123456";
      auth.scheme = Authenticate::AUTH_BASIC;
      auth.realm = "rtsp2-server";
      //auth.nonce = Authenticate::createNonce();
      return auth;
   }

   
}


```
4. rtsp推流服务端
```c++
//推流服务端方式和拉流服务端大体一致
//下面只说明不同之处
//信令上你需要实现handleAnnounce handleRecord接口
class PushServer : public rtsp2::ServerHandle
{
protected:
    void handleAnnounce(const RtspRequest&req,RtspResponse& res)
    {
        //一般你需要解析sdp
        auto sdptxt = req.body();
        sdp_ = parser(sdptxt);
    }
    
    void handleSetup(const RtspRequest&req,TransportV1& transport, RtspResponse& res)
    {
       //协商传输通道,如果是 rtp over rtsp
       //你需要保存 transport, 然后在handleRtp接口判断 是属于哪个"m="描述媒体的 rtp/rtcp包
    }

    void handleRecord(const RtspRequest&req,RtspResponse& res)
    {
        //如果需要，你可以处理RtpInfo字段
        if(req.hasField(RtspMessage::RTPInfo))
        {
            RtspRtpInfo info;
            info.parse(req[RtspMessage::RTPInfo]);
        }
    }
}


```

### 下一步计划

- 支持rtsp2.0(rfc7826)
- 移植部分rtsp2.0的特性到rtsp1.0中


