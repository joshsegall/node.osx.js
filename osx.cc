#include <v8.h>
#include <node.h>

#define PRIVATE 1

#include <errno.h>
#include <ifaddrs.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/ucred.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <utmpx.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_pcb.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#endif

using namespace v8;
using namespace node;

#define MNT_PREFIX "/dev/"

Handle<Value> GetProcesses(const Arguments& args) {
    HandleScope scope;
    int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    size_t len;
    struct kinfo_proc *kprocs = NULL;
    int nprocs;
    Local<Array> procs;
    int i;

    // load the kernel process table
    if (sysctl(mib, 4, NULL, &len, NULL, 0) < 0) {
        return ThrowException(Exception::Error(String::New("sysctl failed.")));
    }        
    kprocs = (struct kinfo_proc *)malloc(len);
    if (sysctl(mib, 4, kprocs, &len, NULL, 0) < 0) {
        free(kprocs);
        return ThrowException(Exception::Error(String::New("sysctl failed.")));
    }        
    nprocs = len / sizeof(struct kinfo_proc);
    procs = Array::New(nprocs);
    if (procs.IsEmpty()) {
        free(kprocs);
        return ThrowException(Exception::Error(String::New("array creation failed.")));
    }

    for (i = 0; i < nprocs; i++) {
        // collect values
        uid_t uid = kprocs[i].kp_eproc.e_ucred.cr_uid;
        struct passwd *user = getpwuid(uid);
        const char *username = user ? user->pw_name : "<unknown>";
        struct timeval *t = &kprocs[i].kp_proc.p_starttime;
        double starttime = (double)t->tv_sec*1000 + t->tv_usec/1000;
 
        // create js object
        Local<Object> proc = Object::New();
        procs->Set(i, proc);
        proc->Set(NODE_PSYMBOL("pid"), Integer::New(kprocs[i].kp_proc.p_pid));
        proc->Set(NODE_PSYMBOL("parent_pid"), Integer::New(kprocs[i].kp_eproc.e_ppid));
        proc->Set(NODE_PSYMBOL("uid"), Integer::New(uid));
        proc->Set(NODE_PSYMBOL("username"), String::New(username));
        proc->Set(NODE_PSYMBOL("command"), String::New(kprocs[i].kp_proc.p_comm));
        proc->Set(NODE_PSYMBOL("starttime"), Date::New(starttime));
   }
    free(kprocs);

    return scope.Close(procs);
}

Handle<Value> GetProcessArgs(const Arguments& args) {
    HandleScope scope;
    int pid = args[0]->Int32Value();
    int mib[] = { CTL_KERN, KERN_PROCARGS2, pid };
    char proc_args[8192];
    size_t argsize;
    Local<Array> cmdargs;
    char *p;
    size_t size = sizeof(int);
    int argmaxmib[] = { CTL_KERN, KERN_ARGMAX };
    int argc;
    char *s, *ps;
    int i;

    sysctl(argmaxmib, 2, &argsize, &size, NULL, 0);

    if (argsize > 8192) {
        argsize = 8192;
    }

    if (sysctl(mib, 3, proc_args, &argsize, NULL, 0) < 0) {
        return ThrowException(Exception::Error(String::New("sysctl failed: invalid pid or no privileges")));
    }

    memcpy(&argc, proc_args, sizeof(argc));
    argsize -= sizeof(argc);
    p = proc_args + sizeof(argc); 
    cmdargs = Array::New(1);
    i = 0;
    for (s = p, ps = p; argc > 0 && s < p + argsize; s++) {
        if (*s == '\0') {
            Local<String> param = String::New(ps);
            cmdargs->Set(i++, param);
            ps = s+1;
            argc--;
        }
    }
 
    return scope.Close(cmdargs);
}

Handle<Value> GetMountInfo(const Arguments& args) {
    HandleScope scope;
    struct statfs *mounts = NULL;
    int nmounts = 0;
    int jnmounts = 0;
    Local<Array> jmounts;
    int i, j;

    nmounts = getmntinfo(&mounts, MNT_NOWAIT);
    jmounts = Array::New(jnmounts);    
    for (i=0, j=0; i<nmounts; i++) {
        Local<Object> mnt;
        long bsize = mounts[i].f_bsize;
        long btotal = mounts[i].f_blocks * bsize;
        long bfree = mounts[i].f_bfree * bsize;
        double pct_free = btotal ? ((double)bfree/(double)btotal) : 0.0;

        // skip any mounts that aren't a mounted device
        if (strncmp(MNT_PREFIX, mounts[i].f_mntfromname, strlen(MNT_PREFIX))) {
            continue;
        }

        mnt = Object::New();
        jmounts->Set(j++, mnt);
        mnt->Set(NODE_PSYMBOL("name"), String::New(mounts[i].f_mntfromname));
        mnt->Set(NODE_PSYMBOL("size"), Number::New((double)btotal));
        mnt->Set(NODE_PSYMBOL("free"), Number::New((double)bfree));
        mnt->Set(NODE_PSYMBOL("pctFree"), Number::New((double)pct_free));
    }

    return scope.Close(jmounts);
}

const char *USER_TYPES[] = {
    "EMPTY",
    "RUN_LVL",
    "BOOT_TIME",
    "NEW_TIME",
    "OLD_TIME",
    "INIT_PROCESS",
    "LOGIN_PROCESS",
    "USER_PROCESS",
    "DEAD_PROCESS",
    "ACCOUNTING" };
int MAX_USER_TYPES = 9;

Handle<Value> GetLoggedInUsers(const Arguments &args) {
    HandleScope scope;
    Local<Array> users;
    Local<Object> user;
    struct utmpx *urec;
    int i = 0;

    setutxent();
    users = Array::New(1);

    while ((urec = getutxent()) != NULL) {
        double starttime = (double)urec->ut_tv.tv_sec*1000 + urec->ut_tv.tv_usec/1000;
        const char *type = "OTHER";
                if (urec->ut_user[0] == '\0') {
            continue;
        }
        if (urec->ut_type <= MAX_USER_TYPES) {
            type = USER_TYPES[urec->ut_type];
        }
        user = Object::New();
        user->Set(NODE_PSYMBOL("name"), String::New(urec->ut_user));
        user->Set(NODE_PSYMBOL("type"), String::New(type));
        user->Set(NODE_PSYMBOL("host"), String::New(urec->ut_host));
        user->Set(NODE_PSYMBOL("line"), String::New(urec->ut_line));
        user->Set(NODE_PSYMBOL("host"), String::New(urec->ut_host));
        user->Set(NODE_PSYMBOL("starttime"), Date::New(starttime));
        users->Set(i++, user);
    }

    endutxent();

    return scope.Close(users);
}

Handle<Value> GetUsers(const Arguments &args) {
    HandleScope scope;
    Local<Array> users;
    struct passwd *urec;

    setpwent();
    users = Array::New(1);

    while ((urec = getpwent()) != NULL) {
        Local<Integer> uid = Integer::New(urec->pw_uid);
        Local<Object> user = Object::New();
        user->Set(NODE_PSYMBOL("name"), String::New(urec->pw_name));
        user->Set(NODE_PSYMBOL("uid"), uid);
        user->Set(NODE_PSYMBOL("gid"), Integer::New(urec->pw_gid));
        user->Set(NODE_PSYMBOL("dir"), String::New(urec->pw_dir));
        users->Set(uid, user);
    }

    endpwent();

    return scope.Close(users);
}

Handle<Value> GetInterfaces(const Arguments &args) {
    HandleScope scope;
    Local<Array> ifaces;
    struct ifaddrs *ifa, *ifhead;
    int i = 0;

    ifaces = Array::New(0);

    if (getifaddrs(&ifhead) != 0) {
        return ThrowException(Exception::Error(String::New("getifaddrs failed")));
    }

    for (ifa = ifhead; ifa != NULL; ifa = ifa->ifa_next) {
        int family;
        char host[NI_MAXHOST];
        const char *type;

        if (ifa->ifa_addr == NULL) {
            continue;
        }
        family = ifa->ifa_addr->sa_family;
        if (family == AF_INET) {
            type = "IPv4";
        } else if (family == AF_INET6) {
            type = "IPv6";
        } else if (family == AF_UNIX) {
            type = "UNIX";
        } else if (family == AF_LINK) {
            type = "LINK";
        } else {
            type = "other";
        }
        host[0] = '\0';
        if (family == AF_INET || family == AF_INET6) {
            getnameinfo(ifa->ifa_addr,
                        (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                              sizeof(struct sockaddr_in6),
                           host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
        }

        Local<Object> jif = Object::New();
        jif->Set(NODE_PSYMBOL("up"), Boolean::New(ifa->ifa_flags & IFF_UP));
        jif->Set(NODE_PSYMBOL("loopback"), Boolean::New(ifa->ifa_flags & IFF_LOOPBACK));
        jif->Set(NODE_PSYMBOL("running"), Boolean::New(ifa->ifa_flags & IFF_RUNNING));
        jif->Set(NODE_PSYMBOL("name"), String::New(ifa->ifa_name));
        jif->Set(NODE_PSYMBOL("family"), String::New(type));
        jif->Set(NODE_PSYMBOL("address"), String::New(host));
        ifaces->Set(i++, jif);
    }

    freeifaddrs(ifhead);
    return scope.Close(ifaces);
}

Handle<Value> GetNetActivity(const Arguments &args) {
    HandleScope scope;
    struct tcpstat tcpstat;
    size_t tcplen = sizeof(struct tcpstat);
    struct ipstat ipstat;
    size_t iplen = sizeof(struct ipstat);
#ifdef INET6
    struct ip6stat ip6stat;
    size_t ip6len = sizeof(struct ip6stat);
    int ip6mib[] = { CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6_STATS };
#endif
    double packets_sent = 0;
    double packets_recvd = 0;
    double bytes_sent = 0;
    double bytes_recvd = 0;
    Local<Object> stats;

    if (sysctlbyname("net.inet.tcp.stats", &tcpstat, &tcplen, 0, 0) < 0) {
        return ThrowException(Exception::Error(String::New("sysctl failed: tcp4 stats")));
    }
    if (sysctlbyname("net.inet.ip.stats", &ipstat, &iplen, 0, 0) < 0) {
        return ThrowException(Exception::Error(String::New("sysctl failed: ip4 stats"))); 
    }

    // hmmm, these seem to be wrong in OSX, so I use tcp as a proxy
    //packets_sent += ipstat.ips_localout;
    //packets_recvd += ipstat.ips_total;
    packets_sent += tcpstat.tcps_sndtotal;
    packets_recvd += tcpstat.tcps_rcvtotal;
    bytes_sent += tcpstat.tcps_sndbyte + tcpstat.tcps_sndrexmitbyte;
    bytes_recvd += tcpstat.tcps_rcvbyte + tcpstat.tcps_rcvoobyte + tcpstat.tcps_rcvdupbyte;
    
#ifdef INET6
    memset(&ip6stat, 0, ip6len);    
    if (sysctl(ip6mib, 4, &ip6stat, &ip6len, NULL, 0) < 0) {
        return ThrowException(Exception::Error(String::New("sysctl failed: ip6 stats"))); 
    }

    packets_sent += ip6stat.ips_localout;
    packets_recvd += ip6stat.ip6s_total;
#endif

    stats = Object::New();
    stats->Set(NODE_PSYMBOL("packetsSent"), Number::New(packets_sent));
    stats->Set(NODE_PSYMBOL("packetsReceived"), Number::New(packets_recvd));
    stats->Set(NODE_PSYMBOL("bytesSent"), Number::New(bytes_sent));
    stats->Set(NODE_PSYMBOL("bytesReceived"), Number::New(bytes_recvd));

    return scope.Close(stats);
}

void init(Handle<Object> target) {
    HandleScope scope;
    NODE_SET_METHOD(target, "procs", GetProcesses);
    NODE_SET_METHOD(target, "procArgs", GetProcessArgs);
    NODE_SET_METHOD(target, "mountInfo", GetMountInfo);
    NODE_SET_METHOD(target, "who", GetLoggedInUsers);
    NODE_SET_METHOD(target, "accounts", GetUsers);
    NODE_SET_METHOD(target, "interfaces", GetInterfaces);
    NODE_SET_METHOD(target, "networkActivity", GetNetActivity);
}
NODE_MODULE(osx, init)
