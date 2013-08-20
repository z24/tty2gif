//------------------------------------------------------------------------------
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <fcntl.h>
//------------------------------------------------------------------------------
class TFile
{
public:
    int Handle;
public:
    TFile()
    {
        Handle = -1;
    }
    ~TFile()
    {
        if(Handle>=0)
        {
            close(Handle);
            Handle = -1;
        }
    }

    bool Open(const char *fileName,int opt=O_RDONLY,int mode=S_IRUSR|S_IWUSR)
    {
        Handle = open(fileName,opt,mode);
        return Handle>=0;
    }
};
//------------------------------------------------------------------------------
struct TCmdInfo
{
    unsigned time;
    unsigned len;
};
//------------------------------------------------------------------------------
#define BUFLEN 4096
TFile master,slaver,file;
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    assert(master.Open("/dev/ptmx",O_RDWR|O_NOCTTY));
    grantpt(master.Handle);
    unlockpt(master.Handle);
    assert(slaver.Open(ptsname(master.Handle),O_RDWR));
    
    int pid = fork();
    assert(pid>=0);
    
    if(pid==0) // open a shell in child
    {
        assert(setsid()!=-1);
        // assert(ioctl(slaver.Handle,TIOCSCTTY,0)!=-1);
        // redirect stdin/out/err to salver
        assert(dup2(slaver.Handle,STDIN_FILENO)  == STDIN_FILENO);
        assert(dup2(slaver.Handle,STDOUT_FILENO) == STDOUT_FILENO);
        assert(dup2(slaver.Handle,STDERR_FILENO) == STDERR_FILENO);
        // open a shell
        const char *sh = getenv("SHELL");
        if(sh==NULL || *sh==0)
            sh = "/bin/sh";
        execlp(sh,sh,NULL);
        exit(1);
    }

    assert(file.Open("test",O_CREAT|O_RDWR));

    char   buff[BUFLEN];
    int    num;
    fd_set fds;

    struct termios rt,ot;
    assert(tcgetattr(STDIN_FILENO,&ot)!=-1);
    cfmakeraw(&rt);
    assert(tcsetattr(STDIN_FILENO,TCSAFLUSH,&rt)!=-1);

    while(1)
    {
        FD_ZERO(&fds);
        FD_SET(STDIN_FILENO,&fds);
        FD_SET(master.Handle,&fds);
        
        assert(select(master.Handle + 1,&fds,NULL,NULL,NULL)!=-1);
        
        // stdin -> pty
        if(FD_ISSET(STDIN_FILENO,&fds))
        {
            num = read(STDIN_FILENO,buff,BUFLEN);
            if(num<=0)
                break;
            assert( write(master.Handle,buff,num) == num );
        }
        // pty -> stdout+file
        if(FD_ISSET(master.Handle,&fds))
        {
            num = read(master.Handle,buff,BUFLEN);
            if(num<=0)
                break;
            assert( write(STDOUT_FILENO,buff,num) == num );
            
            TCmdInfo info;
            info.time = time(NULL);
            info.len  = num;
            assert( write(file.Handle,&info,sizeof(info)) == sizeof(info) );
            assert( write(file.Handle,buff,num) == num );
         }
    }

    assert(tcsetattr(STDIN_FILENO,TCSAFLUSH,&ot)!=-1);
    return 0;
}
//------------------------------------------------------------------------------
