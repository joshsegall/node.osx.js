OSX
===

This module provides access to Mac OS X system information for access in Javascript via node.js

It provides a set of functions that allow programmatic access to information that you might find from the Mac Activity Monitor or linux command line utilities such as ps, top, df, who, dscl, and netstat.

For example, `get_procs` returns a list of all running processes along with relevant properties.

If you're looking for this type of stuff you should also check out the standard `os` module which contains similar functionality for cpus, hostname, load average, release, memory, and uptime. This module is not intended to duplicate any of that functionality.

Dependencies
------------

This was specifically developed for Mac OS X, though it could be extended to work on other systems, and might work on them already. I've only tested it on Darwin 11.2 and various versions up to 13.2.0

This module relies on APIs that Apple considers unsupported, such as `sysctl` and its extensions, which are subject to change from release to release.

Installation
------------

From npm:

    npm install osx

From source:

    node-waf configure build
    npm link

Usage
-----

Example: print out the pid and command of every running process

    var osx = require('osx');
    var procs = osx.procs();
    for (id in procs) {
        console.log(procs[id].pid + ' ' + procs[id].command);
    }

API
---

 * `procs()` : array of process objects - all running processes
   * `pid` : integer - process id
   * `parent_pid` : integer - parent process id
   * `uid` : integer - user id of the user who owns the process
   * `username` : string - name of the user with uid
   * `command` : string - name of the executable that started the process
   * `starttime` : number (milliseconds) - time the process started
 * `procArgs(pid)` : array of string - get the command line arguments of the process with pid
   * NOTE: This is likely to throw an exception if you don't have privileges to that process. Use the `command` property of the process objects from `get_procs` instead in that case.
 * `mountInfo()` : array of mount objects - all mounted devices
   * `name` : string
   * `size` : number (bytes)
   * `free` : number (bytes)
   * `pct_free` : number (percent free)
   * NOTE: only returns mounts that start with `/dev/`
 * `who()` : array of user objects - all users logged in
   * `name` : string - username
   * `line` : string - type of device
   * `host` : string - hostname, if remote session
   * `starttime` : Date (of session start)
 * `users()` : array of string - all users logged in
   * NOTE: like `who` but only a list of unique names
 * `accounts()` : array of user objects - all users with accounts on this system
   * `uid` : integer - user id
   * `gid` : integer - group id
   * `name` : string - user name
   * `dir` : string - home directory
 * `interfaces()` : array of interface objects
    * `name` : string
    * `family` : string - type (e.g. IPv4, IPv6)
    * `address` : string - numeric internet address
    * `up` : boolean - interface is active or not
    * `running` : boolean
    * `loopback` : boolean
 * `networkActivity()` : activity object
   * `packetsSent` : number
   * `packetsReceived` : number
   * `bytesSent` : number
   * `bytesReceived` : number

