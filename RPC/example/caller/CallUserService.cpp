#include <iostream>
#include "RpcApplication.hpp"
#include "RpcControl.hpp"
#include "User.pb.h"
#include "RpcChannel.hpp"


int main(int argc, char **argv)
{
    //初始化ip地址和端口号
    RpcApplication::init(argc, argv);

    //演示调用远程发布的rpc方法login
    ik::UserServiceRpc_Stub stub(new RpcChannel());   // RpcChannel->RpcChannel::callMethod 集中来做所有RPC方法调用的参数序列化和网络发送

    //RPC方法的请求参数
    ik::LoginRequest request;
    request.set_name("zhang san");  
    request.set_password("123456");

    // RPC方法的响应
    ik::LoginResponse response;

    // RPC控制对象， 记录调用过程中的一些状态信息
    RpcControl control;

    //发起rpc方法调用（同步的RPC调用过程 底层调用 RpcChannel::callmethod），等待响应返回
    stub.Login(&control, &request, &response, nullptr);

    // 如果在调用中出现错误
    if(control.Failed())
    {
        std::cout << control.ErrorText() << std:: endl;
    }
    else
    {
        //rpc调用完成，读调用的结果
        if (response.errmsg().error() == 0)
        {
            //没错误
            cout << "rpc login response: " << response.success() << endl;
        }
        else
        {
            //有错误
            cout << "rpc login response errer: " << response.errmsg().error_msg() << endl;
        }
    }
    
    // 演示调用远程发布的RPC方法Register
    ik::RegisterRequest reg_request;
    reg_request.set_id(2000);
    reg_request.set_name("rpc");
    reg_request.set_password("123456");
    ik::RegisterResponse reg_response;

    // 以同步方式发起RPC调用请求，等待返回结果
    stub.Register(&control, &reg_request, &reg_response, nullptr);

    //rpc调用完成，读调用的结果
    if (reg_response.error().error() == 0)
    {
        //没错误
        cout << "rpc login response: " << reg_response.success() << endl;
    }
    else
    {
        //有错误
        cout << "rpc login response errer: " << reg_response.error().error_msg() << endl;
    }


    return 0;
}