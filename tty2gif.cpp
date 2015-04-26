// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

// Youjie Zhou // z24
//------------------------------------------------------------------------------
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
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
    struct timeval tm;
    unsigned       len;
};
//------------------------------------------------------------------------------
#define BUFLEN 4096
TFile master,slave,file;
struct termios rawt,orgt;
struct winsize ws;
const char *pwd;
//------------------------------------------------------------------------------
void TermReset()
{
    assert(tcsetattr(STDIN_FILENO,TCSANOW,&orgt)!=-1);
}
//------------------------------------------------------------------------------
void SaveReplay(const char *,int);
//------------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    if(argc!=2 && argc!=3 && argc!=4)
    {
        fprintf(stderr,"Usage: tty2gif file.raw [out.gif] [delay(ms)]\n");
        return 1;
    }

    pwd = argv[0];

    int delay = (argc==4)?atoi(argv[3]):0;

    if(argc>=3 && file.Open(argv[1])==true) // convert mode
    {
        SaveReplay(argv[2],delay);
        return 0;
    }

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

    assert(file.Open(argv[1],O_CREAT|O_WRONLY|O_TRUNC)); // open output

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
            gettimeofday(&info.tm,NULL);
            info.len  = num;

            assert( write(file.Handle,&info,sizeof(TCmdInfo)) == sizeof(TCmdInfo) );
            assert( write(file.Handle,buff,num) == num );
         }
    }

    // do not need a gif
    if(argc==2)
        return 0;

    while(wait(NULL)>0); // wait for child shell

    assert(file.Open(argv[1]));
    SaveReplay(argv[2],delay);

    return 0;
}
//------------------------------------------------------------------------------
#include <vector>
#include <Magick++.h>
#include <X11/Xlib.h>
using namespace std;
using namespace Magick;
//------------------------------------------------------------------------------
Window GetWindowID()
{
    Display *dpy = XOpenDisplay(NULL);
    assert(dpy);
    Window window;
    int revert;
    XGetInputFocus(dpy,&window,&revert);
    return window;
}
//------------------------------------------------------------------------------
void SaveReplay(const char *fileName, int delay)
{
    assert(file.Handle);

    // clean screen
    fprintf(stdout,"\e[1;1H\e[2J");
    fflush(stdout);

    InitializeMagick(pwd);

    int  num;
    char buff[BUFLEN];
    char str[BUFLEN];
    TCmdInfo prev,cur;

    sprintf(str,"x:0x%lx",GetWindowID());

    vector<Image> frame;

    while(1)
    {
        num = read(file.Handle,&cur,sizeof(TCmdInfo));
        if(num<=0)
            break;

        assert( num == sizeof(TCmdInfo) );
        assert(cur.len);

        num = read(file.Handle,buff,cur.len);
        assert( num == cur.len );

        fwrite(buff,num,1,stdout);
        fflush(stdout);

        Image im;
        im.read(str);

#define diff(a,b) ( (a.tv_sec-b.tv_sec)*1e6 + a.tv_usec-b.tv_usec )
        if(frame.size()>0)
            im.animationDelay( ( diff(cur.tm,prev.tm) + delay*1e3 ) / (1e6/1e2) );

        prev = cur;
        frame.push_back(im);
    }

    assert(frame.size()>0);

    vector<Image> opt;
    optimizeImageLayers(&opt,frame.begin(),frame.end());
    writeImages(opt.begin(),opt.end(),fileName);
}
//------------------------------------------------------------------------------
