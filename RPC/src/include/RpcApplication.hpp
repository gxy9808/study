#pragma once

#include "RpcConfigure.hpp"

// Rpc框架的基础类(单例)， 负责框架的一些初始化操作
class RpcApplication
{
public:
    static void init(int argc, char **argv);

    static RpcApplication &get_instance()   // 获取唯一的一个实例
    {
        static RpcApplication instance;
        return instance;
    }

    static RpcConfigure &get_configure() { return configure_; }

private:
    RpcApplication();
    ~RpcApplication() {}

private:
    RpcApplication(RpcApplication &) = delete;
    RpcApplication(RpcApplication &&) = delete;

private:
    static RpcConfigure configure_;
};