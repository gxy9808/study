# 当前目录的所有源文件名字
aux_source_directory(. SRC_LIST)   

# 添加库
add_library(rpc ${SRC_LIST})

target_link_libraries(rpc pthread muduo_base muduo_net zookeeper_mt)