#include "http_conn.h"


// http_conn 静态变量需要在外部进行初始化
int http_conn::m_epollfd = -1;     // 所有socket上的事件都被注册到同一个epoll内核事件中，所以设置成静态的
int http_conn::m_user_count = 0;   // 所有的客户数
int http_conn::m_request_cnt = 0;  // 请求次数
sort_timer_lst http_conn::m_timer_lst;  // 定时器链表
// locker http_conn::m_timer_lst_locker;  // 共享对象，没有线程竞争资源，所以不需要互斥

// 网站资源的根目录
//const char* doc_root = "/home/gxy/Linux/webserver/resources";

locker m_lock;
map<string, string> users;

// 载入数据表：将数据库中的用户名和密码载入到服务器的map中来
void http_conn::initmysql_result(connection_pool *connPool)
{
    //先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    //在user表中检索username，passwd数据，浏览器端输入
    if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    //从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    //返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    //返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    //从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;
    }
}

// 文件描述符设置为非阻塞
void set_nonblocking(int fd){

    int old_flag = fcntl(fd, F_GETFL);
    int new_flag = old_flag | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_flag);
}

// 向epoll中添加需要监听的文件描述符
void addfd(int epollfd, int fd, bool one_shot, bool et){
    epoll_event event;
    event.data.fd = fd;
    if(et) {   // // 对所有fd设置边沿触发，但是listen_fd不需要
        event.events = EPOLLIN | EPOLLET |EPOLLRDHUP;  // EPOLLRDHUP事件检测异常
    } else {
        event.events = EPOLLIN |EPOLLRDHUP; // 默认为水平触发
    }

    if(one_shot){  // 防止同一个通信被不同的线程处理
        event.events |= EPOLLONESHOT;
    }
    // 在epoll对象中添加文件描述符
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);

    // 设计文件描述符为非阻塞
    set_nonblocking(fd);
}

// 从epoll中移除监听的文件描述符
void removefd(int epollfd,  int fd){

    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 修改文件描述符, 重置socket上EPOLLONESHOT事件,以确保下一次可读时，EPOLLIN事件能被触发
void modfd(int epollfd, int fd, int ev){
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    
    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

// 初始化新接收的连接
void http_conn::init(int sockfd, const sockaddr_in & addr,char *root, int TRIGMode,
                     int close_log, string user, string passwd, string sqlname) {

    m_sockfd = sockfd;
    m_address = addr;

    // 设置端口复用
    int reuse = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    // 添加到epoll对象中
    addfd(m_epollfd, sockfd, true, ET);
    m_user_count ++; // 总用户数+1

    //char ip[20] = "";
    //const char* str = inet_ntop(AF_INET, &addr.sin_addr.s_addr, ip, sizeof(ip));

    m_doc_root = root;  // 网页资源的根目录
    m_TRIGMode = TRIGMode;
    m_close_log = close_log;

    // 当前http连接的用户名、密码、数据库名初始化
    strcpy(sql_user, user.c_str());   
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    init();

    // 创建定时器
    util_timer* new_timer = new util_timer;
    new_timer->user_data = this;                 // 绑定定期器中的用户http数据为当前http连接对象
    time_t curr_time = time(NULL);               // 当前时间
    new_timer->expire = curr_time + 3* TIMESLOT; // 设置超时时间
    this->timer = new_timer;                     // 绑定用户http数据的定时器
    m_timer_lst.add_timer(new_timer);            // 将定时器添加到链表timer_lst中

}

// 初始化其余的连接信息
void http_conn::init() {

    mysql = NULL;
    bytes_to_send = 0;    // 初始化要发送数据的字节数为0
    bytes_have_send = 0;  // 初始化已发送数据的字节数为0

    m_check_state = CHECK_STATE_REQUESTLINE;  // 初始化状态为解析请求首行
    m_linger = false;       // 默认不保持链接  Connection : keep-alive保持连接

    m_method = GET; // 默认请求方式为GET
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    m_checked_index = 0;  // 初始化当前正在解析的位置
    m_start_line = 0;   // 初始化当前正在解析的行的位置
    m_read_idx = 0;  // 初始化下一个要读取数据的位置
    m_write_idx = 0;
    
    cgi = 0;
    
    bzero(m_read_buf, READ_BUFFER_SIZE); // 清空读缓冲区数据
    bzero(m_write_buf, READ_BUFFER_SIZE); // 清空写缓冲区数据
    bzero(m_real_file, FILENAME_LEN);  // 清空目标文件路径数据
} 

// 关闭连接
void http_conn::close_conn() {

    if(m_sockfd!= -1) {
        removefd(m_epollfd, m_sockfd);    // 移除epoll检测,关闭套接字
        m_sockfd = -1;
        m_user_count--; // 关闭一个连接，客户总数量减1
        LOG_INFO("close fd %d", m_sockfd);
    }
    
}

// （读）循环读取客户数据，直到无数据可读或者对方关闭连接
bool http_conn::read() {
    
    // 更新超时时间
    if(timer) {
        time_t curr_time = time(NULL);
        timer->expire = curr_time + 3 * TIMESLOT;
        m_timer_lst.adjust_timer(timer);  // 调整该定时任务变化时对应的定时器在链表中的位置
        LOG_INFO("%s", "adjust timer once");
    }

    // 要读取位置的索引大于缓冲区大小，缓冲区已满
    if(m_read_idx >= READ_BUFFER_SIZE) 
        return false;

    // 读取到的字节
    int bytes_read = 0;
    while (true)    // m_sock_fd已设置非阻塞
    {   
        //从套接字接收数据，存储在m_read_buf缓冲区
        // 从m_read_buf + m_read_idx索引出开始保存数据，大小是READ_BUFFER_SIZE - m_read_idx
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, 
                            READ_BUFFER_SIZE - m_read_idx, 0);
        if(bytes_read == -1) {
            // 非阻塞ET模式下，需要一次性将数据读完
            if(errno == EAGAIN || errno == EWOULDBLOCK) {
                break;
            }
            return false;
        } else if(bytes_read == 0) {  // 没有数据可以读取
            return false;
        }
        m_read_idx += bytes_read; // 更新缓冲区中下一次要读取位置的索引
    }
    ++m_request_cnt;
    return true;
}

// 主状态机，解析请求
http_conn::HTTP_CODE http_conn::process_read(){

    LINE_STATUS line_status = LINE_OK;  // 初始化从状态机的状态
    HTTP_CODE ret = NO_REQUEST;         // HTTP请求解析结果

    char * text = 0; // 获取的一行数据
    // 解析到了一行完整的数据，或者解析到请求体也是完整的数据  （此处抄错了bug1）
    while (((m_check_state == CHECK_STATE_CONTENT) && (line_status == LINE_OK))
        ||((line_status = parse_line()) == LINE_OK))
    // new1: 在解析请求行、请求头部中，每一行都是\r\n作为结束，仅用从状态机的状态line_status=parse_line())==LINE_OK语句即可分析是否解析完整一行
    // new2: 在消息体中，末尾没有任何字符，不能使用从状态机的状态，需用主状态机的状态作为循环入口条件
    //       当消息体解析完时，主状态机会继续为CHECK_STATE_CONTENT，会继续进入循环，所以解析完消息体改变行状态，跳出循环 完成解析任务
    {
        // 获取一行数据
        text = get_line();

        m_start_line = m_checked_index; // 更新下一要解析的行数据的起始位置
        
        LOG_INFO("%s", text);

        // 判断当前状态
        switch(m_check_state) {
            case CHECK_STATE_REQUESTLINE:
                // 解析请求行
                ret = parse_request_line(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                }
                break;

            case CHECK_STATE_HEADER:
                // 解析请求头部
                ret = parse_headers(text);
                if(ret == BAD_REQUEST){
                    return BAD_REQUEST;
                } else if(ret == GET_REQUEST) {   // 完整解析GET请求后，跳转到报文响应函数
                    return do_request();          // 解析具体的请求信息
                }
                break;
            case CHECK_STATE_CONTENT:
                // 解析请求体
                ret = parse_content(text);
                if(ret == GET_REQUEST)  //完整解析POST请求后，跳转到报文响应函数
                    return do_request();
                // 失败则改变行的读取状态
                line_status = LINE_OPEN;  //解析完消息体即完成报文解析，避免再次进入循环，更新line_status
                break;
            default:
                return INTERNAL_ERROR; // 返回内部错误
        }
    }
    return NO_REQUEST; 
}

// 解析HTTP请求行，获得请求方法、目标URL、HTTP版本
http_conn::HTTP_CODE http_conn::parse_request_line(char * text){

    // eg :GET /index.html HTTP/1.1
    m_url = strpbrk(text, " \t"); // 找到text中含有" \t"中字符的位置返回索引
    
    // 如果没有空格或\t，则报文格式有误
    if (!m_url) { 
        return BAD_REQUEST;
    }
    // GET\0/index.html HTTP/1.1
    *m_url++ = '\0';  // 将该位置改为\0，用于将前面数据取出

    // 确定method
    char * method = text;  // method = GET, \0为字符串结束符
    if(strcasecmp(method, "GET")==0) {  // 忽略大小写比较字符串
        m_method = GET;
    } 
    else if(strcasecmp(method,"POST")==0)
    {
        m_method=POST;
        cgi=1;
    }else {
        return BAD_REQUEST;
    }

    //   m_url:   /index.html HTTP/1.1
    // m_url已经跳过了第一个空格或\t字符，不知道后面是否还有，让m_url继续跳过空格和\t字符，指向请求资源的第一个字符
    m_url+=strspn(m_url," \t");   // strspn(s1,s2)：返回s1第一个不包含s2中字符下标
    
    // 判断HTTP版本号
    m_version = strpbrk(m_url, " \t");
    if(!m_version) {
        return BAD_REQUEST;
    }
    //   /index.html\0HTTP/1.1
    *m_version++ = '\0';  // ====  *m_version = '\0';  m_version++;
    if(strcasecmp(m_version,"HTTP/1.1") != 0){   // 项目中仅支持HTTP1.1
        return BAD_REQUEST;
    }
    // 报文请求资源中可能会带有http://，如 http://192.168.1.1:10000/index.html
    if(strncasecmp(m_url, "http://", 7) == 0){
        // 如果是这种情况，m_url向前移动7个字符
        m_url +=7;  // 192.168.1.1:10000/index.html
        m_url = strchr(m_url, '/');  // strchr:返回'/'在m_url中第一次出现的位置 /index.html
    }
    // 可能为https的情况
    //同样增加https情况
    if(strncasecmp(m_url,"https://",8)==0)
    {
        m_url+=8;
        m_url=strchr(m_url,'/');
    }
    // url后面为空 或 不带有 / ， 为错误请求
    if(!m_url || m_url[0] != '/') {
        return BAD_REQUEST;
    }
    // 当url为/时，显示欢迎界面
    if(strlen(m_url)==1)
        strcat(m_url,"judge.html");

    // 请求行处理完毕，主状态机状态变为解析请求头
    m_check_state = CHECK_STATE_HEADER;  

    return NO_REQUEST;  // 没有解析完整，只有检查完整的请求后才会返回 GET_REQUEST
}

// 解析HTTP请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char * text){
    
    // 判断是空行还是请求头；如果遇到空行，表示头部字段解析完毕
    if(text[0] == '\0') {
        // 如果HTTP请求有请求体，则要继续读取m_content_length字节的消息体
        // POST请求含有请求体，需转移到消息体处理状态
        if(m_content_length!=0) {
            m_check_state = CHECK_STATE_CONTENT; // 主状态机转移到CHECK_STATE_CONTENT状态
            return NO_REQUEST;
        }
        // 否则content_length=0说明我们已经得到了一个完整的HTTP请求
        return GET_REQUEST;
    } 
    // 解析请求头部连接字段 Connection
    else if(strncasecmp(text, "Connection:", 11) == 0) {
       
        text += 11;
        //跳过空格和\t字符
        text += strspn(text, " \t");  // strspn返回text中包含空格或\t的字符数目，即让下标跳过空格
        if(strcasecmp(text, "keep-alive") == 0) {
            //如果是长连接，则将linger标志设置为true
            m_linger = true;  
        }
    } 
    // 解析请求头部内容长度字段 Content-Length
    else if(strncasecmp(text, "Content-Length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");   // 让text下标跳过空格或\t
        m_content_length = atol(text); // 获取请求体的消息长度（字符串转整数）
    } 
    // 解析请求头部HOST字段
    else if (strncasecmp( text, "Host:", 5) == 0) {
        
        text += 5;
        text += strspn(text, " \t");
        m_host = text; // 获取主机名字段
    } else {
        LOG_INFO("oop!unknow header: %s", text);
    }
    return NO_REQUEST;
}

// 解析HTTP请求的消息体，判断是否被完整的读入了
http_conn::HTTP_CODE http_conn::parse_content(char * text){
    
    // 读到的数据长度 大于 已解析长度（请求行+头部+空行）+请求体长度
    if( m_read_idx >= (m_content_length + m_checked_index)) {
        // 数据被完整读取
        text[m_content_length] = '\0';

        // 为避免将用户名和密码直接暴露在URL中，用POST请求将用户名和密码封装在消息体中
        m_string = text;  // POST请求体为输入的用户名和密码
        return GET_REQUEST;

    }
    return NO_REQUEST;
}

// 解析一行，判断依据\r\n
http_conn::LINE_STATUS http_conn::parse_line(){

    char temp;
    // 检测索引要小于已读索引
    for ( ; m_checked_index < m_read_idx; ++m_checked_index)
    {
        temp = m_read_buf[m_checked_index];  // 获取当前检测的字符
        if(temp == '\r') {
            if((m_checked_index + 1) == m_read_idx) {  // 读取不完整
                return LINE_OPEN;
            } else if( m_read_buf[m_checked_index + 1] == '\n') { // 读取到完整的一行
                // 将行末尾变为字符串结束符，以获取该行数据
                m_read_buf[m_checked_index++] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if(temp == '\n') {
            if((m_checked_index > 1) && (m_read_buf[m_checked_index-1] == '\r')) {
                m_read_buf[m_checked_index-1] = '\0';
                m_read_buf[m_checked_index++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }
    }
    //并没有找到\r\n，需要继续接收
    return LINE_OPEN;   
}


// 当得到一个完整、正确的HTTP请求时，分析目标文件的属性，
// 如果目标文件存在、对所有用户可读，且不是目录，则使用mmap将其
// 映射到内存地址m_file_address处，并告诉调用者获取文件成功 
http_conn::HTTP_CODE http_conn::do_request() {

    //将初始化的m_real_file赋值为网站根目录   "/home/nowcoder/webserver/resources"
    strcpy(m_real_file, m_doc_root); // 客户请求目标的文件路径 = root+ url，先拷贝网站资源的根目录root
    int len = strlen(m_doc_root);
    // 通过m_url定位/所在位置
    const char *p = strrchr(m_url, '/');  // strrchr返回字符串中最后一次出现字符'\'的位置
    // 登录和注册校验
    if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
    {

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        // 拼接m_real_file目录
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        //将用户名和密码提取出来
        //user=123&passwd=123
        char name[100], password[100];
        int i;
        //以&为分隔符，前面的为用户名
        for (i = 5; m_string[i] != '&'; ++i)
            name[i - 5] = m_string[i];
        name[i - 5] = '\0';   // 字符串末尾添加 "\0"
        //以&为分隔符，后面的是密码
        int j = 0;
        for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
            password[j] = m_string[i];
        password[j] = '\0';
        // 根据/后的第一个字符判断是登录还是注册校验
        if (*(p + 1) == '3')
        {
            //如果是注册，先检测数据库中是否有重名
            char *sql_insert = (char *)malloc(sizeof(char) * 200);  // sql插入语句
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, password);
            strcat(sql_insert, "')");
            printf("%s",sql_insert);
            // 判断map中能否找到重复的用户名
            if (users.find(name) == users.end())
            {
                m_lock.lock();   // 向数据库中插入数据时，需要通过锁来同步数据 
                int res = mysql_query(mysql, sql_insert); // SQL查询数据，成功返回0
                users.insert(pair<string, string>(name, password));
                m_lock.unlock();
                
                if (!res)  //校验成功，跳转登录页面
                {
                    cout << "cgiError1:" << endl;
                    strcpy(m_url, "/log.html");
                }    
                else      // 校验失败，跳转注册失败页面 
                {
                    cout << "cgiError2:" << endl;
                    strcpy(m_url, "/registerError.html");
                }    
            }
            else
            {
                cout << "cgiError3:" << endl;
                strcpy(m_url, "/registerError.html");
            }
                
        }
        //如果是登录，直接判断
        //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        else if (*(p + 1) == '2')
        {
            if (users.find(name) != users.end() && users[name] == password)
                strcpy(m_url, "/welcome.html");
            else
                strcpy(m_url, "/logError.html");
        }
    }
    // 如果请求资源为/0，跳转注册页面，GET
    if (*(p + 1) == '0')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        //将网站目录和/register.html进行拼接，更新到m_real_file中
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // 如果请求资源为/1，跳转登录页面，GET
    else if (*(p + 1) == '1')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/log.html");
        //将网站目录和/log.html进行拼接，更新到m_real_file中
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // 如果请求资源为/5，显示图片页面，POST
    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        //将网站目录和/picture.html进行拼接，更新到m_real_file中
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // 如果请求资源为/6，显示视频页面，POST
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        // 将网站目录和目标文件拼接
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // 如果请求资源为/7，显示关注页面，POST
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        // 以上情况都不是，则直接将url与网站目录拼接，更新至m_real_file中
        // 拼接目录：把请求文件名url添加到请求目标的文件路径real_file后面,得到 /home/nowcoder/webserver/resource/index.html
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1); 

    // 通过stat获取m_real_file文件的相关的状态信息，-1失败，0成功 成功则将信息更新到m_file_stat结构体
    if(stat(m_real_file, &m_file_stat) <0) {
        return NO_RESOURCE;    // 失败返回NO_RESOURCE状态，表示资源不存在
    }
    // 判断文件访问权限, 是否可读，不可读则返回FORBIDDEN_REQUEST状态
    if(!(m_file_stat.st_mode & S_IROTH)) {
        return FORBIDDEN_REQUEST;
    }

    // 判断是否是目录， 如果是目录，则返回BAD_REQUEST，表示请求报文有误
    if (S_ISDIR(m_file_stat.st_mode)) {
        return BAD_REQUEST;
    }

    // 以只读方式打开文件，通过mmap将该文件映射到内存中
    int fd = open(m_real_file, O_RDONLY);
    // 创建内存映射(把文件内容映射到内存中，用地址记录该内容，用以发给客户端)
    m_file_address = (char*)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    // 避免文件描述符的浪费和占用
    close(fd);
    // 表示请求文件存在，且可以访问
    return FILE_REQUEST;
}


// 对内存映射区执行munmap操作
void http_conn::unmap() {
    if(m_file_address)  // 释放资源
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

// （写）给客户端发送HTTP响应
bool http_conn::write() {

    int temp = 0;

    if(timer) {             // 更新超时时间
        time_t curr_time = time( NULL );
        timer->expire = curr_time + 3 * TIMESLOT;
        m_timer_lst.adjust_timer( timer );
        LOG_INFO("%s", "adjust timer once");
    }
    
    // 要发送的数据长度为0,表示响应报文为空
    if(bytes_to_send == 0) {
        // 响应结束， 重新注册监听事件
        modfd(m_epollfd, m_sockfd, EPOLLIN); // 修改m_sockfd，重新添加one_shot，继续监听下一次读事件
        init();   // 初始化连接信息
        return true;
    }

    while(1) {
        // 分散写（将两块内存上的数据写入），将响应报文的状态行、消息头、空行和响应正文发送给浏览器端
        temp = writev(m_sockfd, m_iv, m_iv_count);
        // 判断缓冲区是否满了
        if (temp <= -1) {
            // 如果TCP写缓冲没有空间，则等待下一轮EPOLLOUT事件，虽然在此期间，
            // 服务器无法立即接收到同一客户的下一个请求，但可以保证连接的完整性。
            if(errno == EAGAIN) {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            // 如果发送失败，但不是缓冲区问题，取消映射
            unmap();
            return false;
        }
        // 更新已发送数据的长度和还未发送数据的长度
        bytes_have_send += temp;  
        bytes_to_send -= temp;

        // 判断第一块内存的数据（响应头）是否发送完
        if (bytes_have_send >= m_iv[0].iov_len) 
        {
            m_iv[0].iov_len = 0; // 第一块内存未发送数据的长度置为0
            // 更新第二块还未发送数据的起始位置，（第二款内存已发送数据的长度=已发送数据的总长度 - 缓冲区数据的长度）
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx); 
            m_iv[1].iov_len = bytes_to_send;   // 更新第二块内存未发送数据的长度，等于总的未发送数据的长度
        }
        else  // 第一块内存的数据未发送完
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;  // 更新第一块内存还未发送数据的起始位置
            m_iv[0].iov_len = m_iv[0].iov_len - temp;          // 更新第一块内存未发送数据的长度
        }
        // 数据全部发送出去
        if (bytes_to_send <= 0)
        {
            // 没有数据要发送了
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);  // 重置监听事件

            // 如果浏览器的请求为长连接
            if (m_linger)  
            {
                init();  //重新初始化HTTP对象
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}


// 往写缓冲中写入待发送的数据
bool http_conn::add_response(const char* format, ... ) { // 含有可选参数

    // 写入数据超出写缓冲区大小
    if(m_write_idx >= WRITE_BUFFER_SIZE) {
        return false;
    }

    va_list arg_list; // 定义一个指向个数可变的参数列表指针
    va_start(arg_list, format);  // 使参数列表指针arg_list指向函数参数列表中的第一个可选参数 (参数format是位于第一个可选参数之前的固定参数)

    // 函数vsnprintf：int vsnprintf (char * sbuf, size_t n, const char * format, va_list arg );
    // 将可变参数格式化输出到一个字符数组。sbuf：用于缓存格式化字符串结果的字符数组；参数n：限定最多打印到缓冲区的字符个数为n-1（结果的末尾追加\0）；format：格式化参数，限定字符串；arg：可变长度参数列表
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list); // 返回打印到写缓冲区的字符个数
    if(len >= ( WRITE_BUFFER_SIZE - 1 - m_write_idx )) {
        return false;
    }
    m_write_idx += len; // 更新写缓冲区已写入数据的位置
    va_end(arg_list);   // 清空参数列表，并置参数指针arg_list无效

    LOG_INFO("request:%s", m_write_buf);

    return true;
}

// 在HTTP响应中添加状态行
bool http_conn::add_status_line( int status, const char* title ) {
    return add_response( "%s %d %s\r\n", "HTTP/1.1", status, title);
}

// 在HTTP响应中添加消息头
bool http_conn::add_headers(int content_len) {
    // 消息头包含下面几部分
    add_content_length(content_len);  // 消息体长度
    add_content_type();               // 消息体数据类型
    add_linger();                     // HTTP连接状态
    add_blank_line();                 // 空行
    return true;
}

// 在HTTP响应中添加消息体（响应报文）长度 Content-Length
bool http_conn::add_content_length(int content_len) {
    return add_response("Content-Length: %d\r\n", content_len);
}

// 在HTTP响应中添加HTTP连接状态
bool http_conn::add_linger()
{
    return add_response("Connection: %s\r\n", (m_linger == true) ? "keep-alive" : "close");
}

// 在HTTP响应中添加空行
bool http_conn::add_blank_line() {
    return add_response("%s", "\r\n");
}

// 在HTTP响应中添加消息体内容
bool http_conn::add_content(const char* content) {
    return add_response("%s", content);
}

// 在HTTP响应中添加消息体的数据格式
bool http_conn::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

// 根据服务器处理HTTP请求的结果，生成要发送给客户端的HTTP响应
bool http_conn::process_write(HTTP_CODE ret) {
    
    // 根据对http请求解析结果，返回http响应，依次添加消息行、消息头和消息体
    switch (ret)  
    {
    case INTERNAL_ERROR:  //内部错误，500
        add_status_line(500, error_500_title);  // 消息行
        add_headers(strlen(error_500_form));    // 消息头
        if(!add_content(error_500_form)) {      // 消息体
            return false;
        }
        break;
    
    case BAD_REQUEST:  // 报文语法有误，400
        add_status_line(400, error_400_title);
        add_headers(strlen( error_400_form));
        if(!add_content(error_400_form)) {
            return false;
        }
        break;
    
    case NO_RESOURCE:  // 没有资源，404
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form));
        if (!add_content(error_404_form)) {
            return false;
        }
        break;
    case FORBIDDEN_REQUEST:  //资源没有访问权限，403
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form));
        if (!add_content(error_403_form)) {
            return false;
        }
        break;
    case FILE_REQUEST:  // 文件存在 文件请求
        add_status_line(200, ok_200_title);
        //如果请求的资源存在
        if(m_file_stat.st_size!=0)
        {
            add_headers(m_file_stat.st_size);
            // EMlog(LOGLEVEL_DEBUG, "<<<<<<< %s", m_file_address);

            // 两块内存分别存储写缓冲区的内容（状态行、消息头部）和目标文件映射的内容（消息体）
            // 第一个iovec指针指向响应报文缓冲区，长度指向m_write_idx
            m_iv[0].iov_base = m_write_buf;         // 第一块内存的起始位置
            m_iv[0].iov_len  = m_write_idx;         // 第一块内存的长度
            // 第二个iovec指针指向mmap返回的文件指针，长度指向文件大小
            m_iv[1].iov_base = m_file_address;      // 第二块内存的起始位置
            m_iv[1].iov_len  = m_file_stat.st_size; // 第二块内存的长度
            m_iv_count = 2;  // 两块内存

            // 总的要发送的数据长度等于写缓冲区的数据（响应头的大小）加目标文件数据的大小
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            //如果请求的资源大小为0，则返回空白html文件
            const char* ok_string="<html><body></body></html>";
            add_headers(strlen(ok_string));
            if(!add_content(ok_string))
                return false;
        }
        
    default:
        return false;
    }
    // 除FILE_REQUEST状态外，其余状态只申请一个iovec，指向响应报文缓冲区 即
    // 如果没有文件请求，只需用一块内存发送写缓冲区的数据（包含状态行、消息头部、消息体）
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len  = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;  // 要发送的数据长度
    return true;
}


// 由线程池中的工作线程调用，这是处理HTTP请求的入口函数
void http_conn::process() {

    LOG_INFO("%s","parse request, create response");
    // 解析HTTP请求
    HTTP_CODE read_ret = process_read();
    if(read_ret == NO_REQUEST) {   //NO_REQUEST，表示请求不完整，需要继续接收请求数据
        modfd(m_epollfd, m_sockfd, EPOLLIN);  // 修改m_sockfd，重新添加one_shot，继续监听读事件
        return;
    }

    // 生成HTTP响应
    bool write_ret = process_write(read_ret);
    if(!write_ret){
        close_conn();
        if(timer) m_timer_lst.del_timer(timer);  // 移除其对应的定时器
    }
    // 注册并监听写事件
    modfd(m_epollfd, m_sockfd, EPOLLOUT);  // 监测写事件，重置one_shot
}

