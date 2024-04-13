#include <signal.h>
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h> 	
#include <bits/stdc++.h>


class NODE
{
public:
    NODE()
    {
        pid = 0;
        status = "";
        link = nullptr;
    }
    NODE(pid_t p, std::vector<std::string> c, std::string s)
    {
        pid = p;
        cmd = c;
        status = s;
        link = nullptr;
    }
    pid_t pid;
    std::vector<std::string> cmd;
    std::string status;
    NODE* link;
};

//全局变量声明
NODE* head = nullptr;
pid_t cur_pid = 0; //当前前台运行的进程的pid
int is_sigz = 0; //是否按下ctrlz
//函数声明
void execute_external_command(std::vector<char*>& c_args);
NODE* get_node_bypid(pid_t p);
NODE* get_node_byindex(int n);
void add_node(pid_t p, std::vector<std::string> c, std::string s);
void remove_node(pid_t p);
void show_node();
void redirect(std::vector<std::string> args);
void pipel(std::vector<std::string> args);
void execute_external_command(std::vector<char*>& c_args);
void execute_internal_command(const std::string& command, const std::vector<std::string>& args);
void ctrl_z();
void ctrl_c();
void sign_handle(int sig, siginfo_t *sip, void *vp);
void setup_signal_handler();
std::vector<std::string> get_line();
int trans(std::vector<std::string> args, std::vector<char*>& c_args);

//通过pid获得NODE
NODE* get_node_bypid(pid_t p)
{
    NODE* temp = head;
    while (temp != nullptr)
    {
        if (temp->pid == p)
        {
            return temp;
        }
        temp = temp->link;
    }
    return nullptr;
}
//通过index获得NODE
NODE* get_node_byindex(int n)
{
    NODE* temp = head;
    for (int i = 1; temp != nullptr; i++)
    {
        if (i == n)
        {
            return temp;
        }
        temp = temp->link;
    }
    return nullptr;
}
//添加NODE
void add_node(pid_t p, std::vector<std::string> c, std::string s)
{
    NODE* node = new NODE(p, c, s);
    if (head == nullptr)
    {
        head = node;
    }
    else
    {
        NODE* temp = head;
        while (temp->link != nullptr)
        {
            temp = temp->link;
        }
        temp->link = node;
    }
}
//删除NODE
void remove_node(pid_t p)
{
    //Ctrlz也会产生信号调用此函数，需要判断
    if (is_sigz == 0)
    {
        //pid_t p = sip->si_pid;
        //std::cout << "remove_node: " << p << "\n";
        NODE* temp = head;
        NODE* pre = nullptr;
        while (temp != nullptr)
        {
            if (temp->pid == p)
            {
                if (pre == nullptr)
                {
                    head = temp->link;
                }
                else
                {
                    pre->link = temp->link;
                }
                waitpid(p, NULL, 0);
                delete temp;
                return;
            }
            pre = temp;
            temp = temp->link;
        }
    }
    else
    {
        is_sigz = 0;
    }
    return;
}

//显示所有后台进程
void show_node()
{
    NODE* temp = head;
    for (int i = 1; temp != nullptr; i++)
    {
        std::cout << "[" << i << "] " << temp->pid << " " << temp->status << "\t";
        for (const auto& c : temp->cmd)
        {
            std::cout << c << " ";
        }
        std::cout << std::endl;
        temp = temp->link;
    }
}
//重定向命令
void redirect(std::vector<std::string> args)
{
    //本shell不考虑同时出现重定向输入和输出的情况
    //除非使用管道，否则视为非法
    std::string filename; //重定向文件
    std::vector<std::string> temp; //命令
    int fd_in, fd_out, is_in = 0, is_out = 0; //重定向端口和标识
    //解析命令
    for (std::string arg : args)
    {
        //错误检测
        if (((arg == ">" || arg == ">>") && is_out != 0) ||
            (arg == "<" && is_in != 0))
        {
            std::cout << "Error command !\n";
            return;
        }


        if (arg == ">")
        {
            is_out = 1;
        }
        else if (arg == ">>")
        {
            is_out = 2;
        }
        else if (arg == "<")
        {
            is_in = 1;
        }
        else if (is_out == 0 && is_in == 0)
        {
            temp.push_back(arg);
        }
        else if ((is_out != 0 || is_in != 0) && filename.empty())
        {
            filename = arg;
            break;
        }
        
    }

    //debug
    //std::cout << "$command: ";
    //for (std::string s : temp)
    //{
    //    std::cout << s;
    //} 
    //std::cout <<"\n$filename: " << filename << "\n";
    //std::cout << "$in & out: " << is_in << is_out << "\n";

    
    //转换为c风格字符串
    std::vector<char*> c_args;
    int is_bg = trans(temp, c_args);
    

    pid_t pid = fork();
    if (pid == 0)
    {//子进程
        /*重定向输入*/
        if (is_in == 1)
        {
            if((fd_in = open(filename.c_str(), O_RDONLY, S_IRUSR | S_IWUSR)) == -1)
            {
                std::cout << "Can not open " << filename.c_str() << std::endl;
                return;
            }
            //将标准输入重定向到fd_in上
            if (dup2(fd_in, STDIN_FILENO) == -1)
            {
                std::cout << "Redirect standard in error !\n";
                exit(1);
            }
        }
        /*重定向输出(追加)*/
        if (is_out == 2)
        {
            if((fd_out = open(filename.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) == -1)
            {
                std::cout << "Can not open " << filename.c_str() << std::endl;
                return;
            }
            //将标准输出重定向到fd_out上
            if (dup2(fd_out, STDOUT_FILENO) == -1)
            {
                std::cout << "Redirect standard in error !\n";
                exit(1);
            }
            
        }
        /*重定向输出*/
        if (is_out == 1)
        {
            if((fd_out = open(filename.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1)
            {
                std::cout << "Can not open " << filename.c_str() << std::endl;
                return;
            }
            //将标准输出重定向到fd_out上
            if (dup2(fd_out, STDOUT_FILENO) == -1)
            {
                std::cout << "Redirect standard in error !\n";
                exit(1);
            }
            
        }
        execute_external_command(c_args);
    }
    else
    {//父进程
        add_node(pid, args, "running");
        if (is_bg == 0)
        {
            cur_pid = pid;
            //等待，并保证在以下情况退出
            //ctrlc，ctrlz会清除cur_pid，执行完成返回cur_pid
            //防止出现收到其他进程结束信号后，未等待进程结束的情况
            while(waitpid(cur_pid, NULL, 0) != cur_pid && cur_pid != 0);
            cur_pid = 0;
        }
    }
}
//管道命令
void pipel(std::vector<std::string> args)
{
    //本shell仅支持两个命令的管道命令
    std::string filename1, filename2; //命令一的输入重定向文件，命令二的输出重定向文件
    std::vector<std::string> temp1, temp2; //命令一，命令二
    //pipefd管道端口，in仅对第一个命令有效，out只对第二个命令有效，is_t2检测是否遇到|
    int pipefd[2],fd_in, fd_out, is_in = 0, is_out = 0, is_t2 = 0;
    //解析命令
    for (std::string arg : args)
    {
        if (is_t2 == 0)
        {
            //错误检测
            //error1:已经出现过“<”
            //error2:在第一个命令出现“>>”“>”
            //error3:在出现过“<”读取了filename后，第一个命令还未读完
            if ((arg == "<" && is_in != 0) ||
                (arg == ">>" || arg == ">") ||
                (!arg.empty() && arg != "|" && !filename1.empty()))
            {
                std::cout << "Error command1 !\n";
                return;
            }

            if (arg == "<")
            {
                is_in = 1;
            }
            else if (arg == "|")
            {
                is_t2 = 1;
                continue;
            }
            else if (is_in == 0)
            {
                temp1.push_back(arg);
            }
            else if (is_in != 0 && filename1.empty())
            {
                filename1 = arg;
            }
        }
        else if (is_t2 == 1)
        {
            //错误检测
            //error1:已经出现过“>>”“>”
            //error2:在第二个命令出现“<”
            //error3:在第二个命令出现“|”
            if (((arg == ">" || arg == ">>") && is_out != 0) ||
                (arg == "<" || arg == "|"))
            {
                std::cout << "Error command2 !\n";
                return;
            }

            if (arg == ">")
            {
                is_out = 1;
            }
            else if (arg == ">>")
            {
                is_out = 2;
            }
            else if (is_out == 0)
            {
                temp2.push_back(arg);
            }
            else if (is_out != 0 && filename2.empty())
            {
                filename2 = arg;
                break;
            }
        }
    }

    //debug
    //std::cout << "$command1: ";
    //for (std::string s : temp1)
    //{
    //    std::cout << s;
    //} 
    //std::cout <<"\n$filename1: " << filename1 << "\n";
    //std::cout << "$in & out: " << is_in << is_out << "\n";
    //std::cout << "$command2: ";
    //for (std::string s : temp2)
    //{
    //    std::cout << s;
    //} 
    //std::cout <<"\n$filename2: " << filename2 << "\n";
    //std::cout << "$in & out: " << is_in << is_out << "\n";

    //转换为c风格字符串
    std::vector<char*> c_args1, c_args2;
    int is_bg = trans(temp2, c_args2);
    trans(temp1, c_args1);

    //管道设置
    /*初始化文件描述符*/
    pipefd[0] = -1;
    pipefd[1] = -1;
    /*为命令建立相应的管道*/
    if (pipe(pipefd) == -1)
    {
        std::cout << "Can not open pipe !\n";
        return;
    }


    pid_t pid = fork();
    if (pid == 0)
    {//子进程管理进程
        signal(SIGCHLD, SIG_DFL);
        signal(SIGTSTP, SIG_DFL);
        signal(SIGINT, SIG_DFL);

        pid_t p1 = fork();
        if (p1 == 0)
        {//子进程1
            /*重定向处理*/
            if (is_in == 1)
            {
                if((fd_in = open(filename1.c_str(), O_RDONLY, S_IRUSR | S_IWUSR)) == -1)
                {
                    std::cout << "Can not open " << filename1.c_str() << std::endl;
                    return;
                }
                //将标准输入重定向到fd_in上
                if (dup2(fd_in, STDIN_FILENO) == -1)
                {
                    std::cout << "Redirect standard in error !\n";
                    exit(1);
                }
            }
            if (pipefd[1] != -1)
            {
                //将标准输出重定向到管道的写端
                dup2(pipefd[1], STDOUT_FILENO);
            }
            close(pipefd[0]);
            //close(pipefd[1]);
            execute_external_command(c_args1);
        }
        else
        {//父进程
            //std::cout << "test1\n";
            while(waitpid(p1, NULL, 0) != p1);
            //std::cout << "test2\n";
            //close(pipefd[1]);
        }


        pid_t p2 = fork();
        if (p2 == 0)
        {//子进程2
            /*重定向输出(追加)*/
            if (is_out == 2)
            {
                if((fd_out = open(filename2.c_str(), O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR)) == -1)
                {
                    std::cout << "Can not open " << filename2.c_str() << std::endl;
                    return;
                }
                //将标准输出重定向到fd_out上
                if (dup2(fd_out, STDOUT_FILENO) == -1)
                {
                    std::cout << "Redirect standard in error !\n";
                    exit(1);
                }
                
            }
            /*重定向输出*/
            if (is_out == 1)
            {
                if((fd_out = open(filename2.c_str(), O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR)) == -1)
                {
                    std::cout << "Can not open " << filename2.c_str() << std::endl;
                    return;
                }
                //将标准输出重定向到fd_out上
                if (dup2(fd_out, STDOUT_FILENO) == -1)
                {
                    std::cout << "Redirect standard in error !\n";
                    exit(1);
                }
                
            }

            if (pipefd[0] != -1)
            {
                //将标准输入重定向到管道的读端
                dup2(pipefd[0], STDIN_FILENO);
            }
            //close(pipefd[0]);
            close(pipefd[1]);// ！！！关闭管道写端，防止命令读完后继续等待其他命令写入
            execute_external_command(c_args2);
        }
        else
        {//父进程
            //std::cout << "test3\n";
            close(pipefd[0]);
            close(pipefd[1]);
            while(waitpid(p2, NULL, 0) != p2);
            //std::cout << "test4\n";
        }
        exit(0);

    }
    else
    {//父进程
        close(pipefd[0]);
        close(pipefd[1]);
        add_node(pid, args, "running");
        if (is_bg == 0)
        {
            cur_pid = pid;
            //等待，并保证在以下情况退出
            //ctrlc，ctrlz会清除cur_pid，执行完成返回cur_pid
            //防止出现收到其他进程结束信号后，未等待进程结束的情况
            while(waitpid(pid, NULL, 0) != cur_pid && cur_pid != 0);
            cur_pid = 0;
        }
    }
}
//外部命令
void execute_external_command(std::vector<char*>& c_args) 
{
    // 执行外部命令
    execvp(c_args[0], c_args.data());
    
    // 如果execvp返回，则表示执行失败
    std::cerr << "Error: Failed to execute command: " << c_args[0] << std::endl;
    exit(EXIT_FAILURE);
}
//执行内部命令
void execute_internal_command(const std::string& command, const std::vector<std::string>& args) 
{
    if (command == "exit")
    {
        exit(EXIT_SUCCESS);
    }
    else if (command == "jobs")
    {
        show_node();
    }
    else if (command == "fg")
    {
        NODE* target = get_node_byindex(atoi(args[1].c_str()));
        if (target == nullptr)
        {
            std::cerr << "Error: The process does not exist." << std::endl;
            return;
        }
        else
        {
            target->status = "running";
            cur_pid = target->pid;
            kill(target->pid, SIGCONT);
            //等待，并保证在以下情况退出
            //ctrlc，ctrlz会清除cur_pid，执行完成返回cur_pid
            //防止出现收到其他进程结束信号后，未等待进程结束的情况
            while(waitpid(cur_pid, NULL, 0) != cur_pid && cur_pid != 0);
            cur_pid = 0;
            //std::cout << "The process " << target->pid << " is finished." << std::endl;
        }
    }
    else if (command == "bg")
    {
        NODE* target = get_node_byindex(atoi(args[1].c_str()));
        if (target == nullptr)
        {
            std::cerr << "Error: The process does not exist." << std::endl;
            return;
        }
        else
        {
            target->status = "running";
            kill(target->pid, SIGCONT);
        }
    }
    else 
    {
        std::cerr << "Error: Unknown internal command: " << command << std::endl;
    }
    return;
}

//ctrlz中断
void ctrl_z()
{
    NODE *target = get_node_bypid(cur_pid);
    if(target != nullptr)
    {
        is_sigz = 1;
        target->status = "stopped";
        kill(target->pid, SIGTSTP);
        cur_pid = 0;
        std::cout << std::endl;
    }
    else
    {
        std::cout << "\nysh> ";
    }
    //fflush(stdout); // 刷新输出缓冲区
    return;
}
//ctrlc中断
void ctrl_c()
{
    NODE *target = get_node_bypid(cur_pid);
    if (target != nullptr)
    {
        target->status = "killed";
        kill(target->pid, SIGINT);
        cur_pid = 0;
        std::cout << std::endl;
    }
    else
    {
        std::cout << "\nysh> ";
    }
    //fflush(stdout); // 刷新输出缓冲区
    return;
}

//信号处理函数总入口
void sign_handle(int sig, siginfo_t *sip, void *vp)
{
    //std::cout << sig;
    if(sig == SIGCHLD)
    {
        remove_node(sip->si_pid);
        return;
    }
    else if (sig == SIGTSTP)
    {
        ctrl_z();
        return;
    }
    else if (sig == SIGINT)
    {
        ctrl_c();
        return;
    }
}
//初始化信号处理函数
void setup_signal_handler() 
{
    struct sigaction action;
    action.sa_sigaction = sign_handle;
    sigfillset(&action.sa_mask);
    action.sa_flags = SA_SIGINFO;
    sigaction(SIGCHLD, &action, NULL);
    //signal(SIGTSTP, ctrl_z);
    //signal(SIGINT, ctrl_c);
    sigaction(SIGTSTP, &action, NULL);
    sigaction(SIGINT, &action, NULL);
}

//获取输入并规范化
std::vector<std::string> get_line()
{
    std::string input;
    do
    {
        std::cin.clear();
    }//！！！要判断，防止在等待时被中断出现错误
    while (!std::getline(std::cin, input));

    std::vector<std::string> args;
    std::string arg;
    for (const auto& c : input)
    {
        if (c == ' ' || c == '\n')
        {
            if (!arg.empty())
            {
                args.push_back(arg);
                arg.clear();
            }
        }
        else
        {
            arg += c;
        }
    }
    if (!arg.empty())
    {
        args.push_back(arg);
    }
    
    return args;
}
//将vector转换为C风格的字符串数组
int trans(std::vector<std::string> args, std::vector<char*>& c_args)
{
    int is_bg = 0;
    for (const auto& arg : args) 
    {
        if (arg == "&") 
        {
            is_bg = 1;
            break;
        }
        c_args.push_back(const_cast<char*>(arg.c_str()));
    }
    //execvp要求最后一个参数为nullptr
    c_args.push_back(nullptr); 
    return is_bg;
}

int main() 
{
    setup_signal_handler();
    while (true) 
    {
        // 打印命令提示符
        std::cout << "ysh> ";

        // 输入命令
        std::vector<std::string> args;
        args = get_line();
    
        // 如果输入为空则继续等待输入
        if (args.empty()) 
        {
            continue;
        }

        //管道，重定向命令
        if (std::find(args.begin(), args.end(), "|") != args.end())
        {//管道
            pipel(args);
            continue;
        }
        else if (std::find(args.begin(), args.end(), "<") != args.end() ||
            std::find(args.begin(), args.end(), ">") != args.end() ||
            std::find(args.begin(), args.end(), ">>") != args.end())
        {//重定向
            redirect(args);
            continue;
        }


        //内部命令
        if (args[0] == "exit" || args[0] == "jobs" || args[0] == "fg" || args[0] == "bg") 
        {
            execute_internal_command(args[0], args);
            continue;
        }

        // 将vector转换为C风格的字符串数组
        std::vector<char*> c_args;
        int is_bg = trans(args, c_args);

        // 创建子进程执行外部命令
        pid_t pid = fork();
        if (pid == 0) 
        { // 子进程
            execute_external_command(c_args);
        } 
        else if (pid < 0) 
        { // fork失败
            std::cerr << "Error: Failed to fork process." << std::endl;
        } 
        else 
        { // 父进程
            // 等待子进程结束
            add_node(pid, args, "running");
            if (is_bg == 0)
            {
                cur_pid = pid;
                while(waitpid(cur_pid, NULL, 0) != cur_pid && cur_pid != 0);
                
                //do 
                //{
                //    pid = waitpid(cur_pid, NULL, 0);
                    //int h_8 = (status >> 8) & 0xff;
                    //int l_8 = status & 0xff;
                    //std::cout << "h_8: " << h_8 << "   l_8: " << l_8 << std::endl;
                    //std::cout << "exit code: " << WEXITSTATUS(status);
                //}
                //while(cur_pid != 0 && pid != cur_pid);
                //TODO: waitpid某一个子进程时，收到另一个子进程结束的信号，waitpid被跳过
                
                //std::cout << "The process " << pid << " is finished." << std::endl;
                cur_pid = 0;
            }
        }
    }

    return 0;
}