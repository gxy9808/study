#include <iostream>
#include <string>
#include "User.pb.h"
#include "RpcApplication.hpp"
#include "RpcProvider.hpp"
#include "RpcLogger.hpp"

using namespace std;
/*
* UserService 是一个本地服务，提供了两个进程内的本地方法，login和getfriendlist
*/
class UserService : public ik::UserServiceRpc //使用在rpc服务发布端
{
public:
    bool Login(string name, string password)
    {
        cout << "doing local service:login: ";
        cout << "name: " << name << " password: " << password << endl;
        return true;
    }

    bool Register(uint32_t id, string name, string password)
    {
        cout << "doing local service:register: ";
        cout << "id:" << id << "name: " << name << " password: " << password << endl;
        return true;
    }

    // 重写基类UserServiceRpc的虚函数，供框架调用
    // 1. caller -->  Login(LoginRequest) -> muduo -> callee接收，反序列化
    // 2. callee -->  Login(LoginRequest) -> 交给重写的Login方法
    void Login(::google::protobuf::RpcController *controller,   // UserServiceRpc需要调用的Login远程方法
               const ::ik::LoginRequest *request,
               ::ik::LoginResponse *response,
               ::google::protobuf::Closure *done)
    {
        //框架给业务上报了请求参数 request，业务获取相应数据做本地业务 （protobuf已经将请求反序列化为LoginRequest对象）
        string name = request->name();
        string password = request->password();

        //本地业务
        bool login_result = Login(name, password);

        //把响应给调用方返回  （错误码、错误消息、返回值）
        ik::ErrorMsg *errmsg = response->mutable_errmsg();
        errmsg->set_error(0);
        errmsg->set_error_msg("");
        response->set_success(login_result);  // 把response填充好，剩余交给框架做

        //执行回调操作   执行响应对象数据的序列化和网络发送（由框架完成）
        done->Run();
    }

    void Register(::google::protobuf::RpcController *controller,
                  const ::ik::RegisterRequest *request,
                  ::ik::RegisterResponse *response,
                  ::google::protobuf::Closure *done)
    {
        //框架给业务上报了请求参数 request，业务获取相应数据做本地业务
        uint32_t id = request->id();
        string name = request->name();
        string password = request->password();

        //本地业务
        bool login_result = Register(id, name, password);

        //把响应给调用方返回
        ik::ErrorMsg *errmsg = response->mutable_error();
        errmsg->set_error(0);
        errmsg->set_error_msg("");
        response->set_success(login_result);

        //执行回调操作
        done->Run();
    }
};

int main(int argc, char **argv)
{
    RPC_LOG_INFO("HELLO");
    RPC_LOG_ERROR("%s,%s,%d", __FILE__, __FUNCTION__, __LINE__);
    //调用框架的初始化操作
    RpcApplication::init(argc, argv);

    //框架服务提供provider
    RpcProvider provide;   // 框架中发布RPC网络服务的对象
    provide.notify_service(new UserService());  // 发布服务，把UserService对象发布到rpc节点上
    
    // 启动一个RPC服务发布节点
    provide.run();  // Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    return 0;
}