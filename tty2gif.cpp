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
        Close();
    }

    void Close()
    {
        if(Handle>=0)
        {
            close(Handle);
            Handle = -1;
        }
    }
    bool Open(const char *fileName,int opt=O_RDONLY,int mode=S_IRUSR|S_IWUSR)
    {
        Close();
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
TFile master,slave,file;
struct termios rawt,orgt;
struct winsize ws;
//------------------------------------------------------------------------------
void TermReset()
{
    assert(tcsetattr(STDIN_FILENO,TCSANOW,&orgt)!=-1);
}
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    // save current term attributes
    assert(tcgetattr(STDIN_FILENO,&orgt)!=-1); // term mode
    assert(ioctl(STDIN_FILENO,TIOCGWINSZ,&ws)>=0); // term size

    assert(master.Open("/dev/ptmx",O_RDWR|O_NOCTTY)); // open master
    grantpt(master.Handle); // grant access to slave
    unlockpt(master.Handle); // unlock slave

    int pid = fork();
    assert(pid>=0);
    
    if(pid==0) // open a shell in child
    {
        assert(slave.Open(ptsname(master.Handle),O_RDWR)); // open slave
        assert(tcsetattr(slave.Handle,TCSANOW,&orgt)!=-1);
        assert(ioctl(slave.Handle,TIOCSWINSZ,&ws)!= -1);
 
        // start a new session
        assert(setsid()!=-1);
        // redirect stdin/out/err to salver
        assert(dup2(slave.Handle,STDIN_FILENO)  == STDIN_FILENO);
        assert(dup2(slave.Handle,STDOUT_FILENO) == STDOUT_FILENO);
        assert(dup2(slave.Handle,STDERR_FILENO) == STDERR_FILENO);
        // open a shell
        const char *sh = getenv("SHELL");
        if(sh==NULL || *sh==0)
            sh = "/bin/sh";
        execlp(sh,sh,NULL);
        assert(1);
    }

    assert(file.Open("test",O_CREAT|O_WRONLY|O_TRUNC)); // open output

    char   buff[BUFLEN];
    int    num;
    fd_set fds;

    cfmakeraw(&rawt); // build a raw term attrib
    assert(tcsetattr(STDIN_FILENO,TCSAFLUSH,&rawt)!=-1); // raw mode
    assert(atexit(TermReset)==0); // restore mode at exit

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

    return 0;
}
//------------------------------------------------------------------------------
