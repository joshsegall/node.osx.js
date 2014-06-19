// This isn't a unit test, but it can be used to verify all the functions still operate.

var osx = require('osx'),
    os = require('os');

function relativeTime(secs) {
    return new Date(new Date().getTime() - (secs * 1000));
}

function uptime(d) {
    return Math.round(new Date().getTime() - d.getTime());
}

var ONE_MINUTE = 60 * 1000;
var ONE_HOUR = 60 * ONE_MINUTE;
var ONE_DAY = 24 * ONE_HOUR;

function prettyAge(m) {
    var days = Math.round(m / ONE_DAY);
    m = m % ONE_DAY;
    var hours = Math.round(m / ONE_HOUR);
    m = m % ONE_HOUR;
    var mins = Math.round(m / ONE_MINUTE);

    str = [];
    if (days) {
        str.push(days);
        str.push('d ');
    }
    if (days || hours) {
        str.push(hours);
        str.push('h ');
    }
    str.push(mins);
    str.push('m');
    return str.join('');
}

var DISK_SIZES = [
    ['TB', 1024*1024*1024*1024],
    ['GB', 1024*1024*1024],
    ['MB', 1024*1024],
    ['KB', 1024]];

function prettySize(n) {
    for (i in DISK_SIZES) {
        size = DISK_SIZES[i];
        if (n > size[1]) {
            return Math.round(10*n / size[1])/10 + size[0];
        }
    }
    return n;
}

procs = osx.procs();
console.log('num procs = ' + procs.length);
for (var i in procs) {
    var p = procs[i];
    var cmd = '';
    try {
        var cmds = osx.procArgs(p.pid);
        cmds[0] = cmds[0].split('/').pop();
        cmd = cmds.join(' ');
        if (cmd.length > 64) {
            cmd = cmd.slice(0, 64) + '...';
        }
    } catch (err) {
        cmd = p.command;
    }
    
    console.log('  ' + p.pid + ' ' + p.parent_pid + ' ' + p.uid + ' ' +
            p.username + ' (' + prettyAge(uptime(p.starttime)) + ') ' +
            cmd
            );

}

console.log('hostname = ' + os.hostname());
console.log('OS release = ' + os.release());
console.log('OS type = ' + os.type());

var uptime = os.uptime();
var boottime = relativeTime(uptime);
console.log('boottime = ' + boottime);
console.log('uptime = ' + prettyAge(uptime*1000));
console.log('cpus = ' + os.cpus().length);
console.log('mem = ' + prettySize(os.freemem()) + ' free / ' + prettySize(os.totalmem()));
console.log('loadavg = ' + os.loadavg());
console.log('pid = ' + process.pid);

mounts = osx.mountInfo();
for (var i in mounts) {
    var m = mounts[i];
    console.log(m.name + 
            ' size=' + prettySize(m.size) +
            ' free=' + prettySize(m.free) +
            ' (' + Math.round(m.pctFree * 100) + '%)');
}

var who = osx.who();
console.log('who = ' + who.length);
for (var id in who) {
    var u = who[id];
    console.log('  ' + u.name + ' ' + u.line + ' ' + u.host + ' ' + u.type + ' ' + u.starttime);
}

var users = osx.users();
console.log('users = ' + users);

var users = osx.accounts();
var count = 0;
console.log('accounts = ');
for (var id in users) {
    var u = users[id];
    console.log('  ' + u.name + ' ' + u.uid + ' ' + u.gid + ' ' + u.dir);
    count++;
}
console.log(count + ' entries');
        
var ifaces = osx.interfaces();
console.log('ifaces = ' + ifaces.length);
for (var i in ifaces) {
    var ifa = ifaces[i];
    console.log('  ' + ifa.name + ' up=' + ifa.up + ' ' + ifa.family + ' ' + ifa.address);
}

console.log('net activity = ');
var stat = osx.networkActivity();
for (var i in stat) {
    console.log('   ' + i + ' = ' + prettySize(stat[i]));
}

